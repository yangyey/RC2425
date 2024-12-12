#include "server.hpp"
#include "../constant.hpp"
#include "../utils.hpp"
#include <cstdlib>
#include <algorithm>
#include <random>

using namespace std;

// Game implementation
Game::Game(const std::string& pid, int maxPlayTime, char mode)
    : plid(pid), maxTime(maxPlayTime), active(false), gameMode(mode) {
    startTime = time(nullptr);
    generateSecretKey();
    saveInitialState();

}

std::string Game::getGameFilePath() const {
    return "GAMES/GAME_" + plid + ".txt";
}

void Game::saveInitialState() const {
    std::ofstream file(getGameFilePath());
    if (!file) {
        throw std::runtime_error("Cannot create game file");
    }

    // Get formatted time strings
    time_t now = time(nullptr);
    struct tm* timeinfo = gmtime(&now);
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Write header: PPPPPP M CCCC T YYYY-MM-DD HH:MM:SS s
    file << std::setfill('0') << std::setw(6) << plid << " "
         << gameMode << " " << secretKey << " "
         << maxTime << " " << timeStr << " "
         << startTime << std::endl;

    file.close();
}

void Game::appendTrialToFile(const std::string& trial, int nB, int nW) const {
    std::ofstream file(getGameFilePath(), std::ios::app);
    if (!file) {
        throw std::runtime_error("Cannot append to game file");
    }

    // Calculate seconds from start
    time_t now = time(nullptr);
    int secondsFromStart = now - startTime;

    // Write trial line: T: CCCC B W s
    file << "T: " << trial << " " << nB << " " << nW << " " 
         << secondsFromStart << std::endl;

    file.close();
}

void Game::finalizeGame(char endCode) {
    if (!active) return;
    active = false;
    
    // Create player directory if needed
    std::string playerDir = "GAMES/" + plid;
    mkdir(playerDir.c_str(), 0777);

    // Add final timestamp line
    std::ofstream file(getGameFilePath(), std::ios::app);
    if (file) {
        time_t now = time(nullptr);
        struct tm* timeinfo = gmtime(&now);
        char timeStr[30];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
        file << timeStr << " " << (now - startTime) << std::endl;
        file.close();
    }

    // Generate new filename with timestamp and end code
    char newFileName[100];
    time_t now = time(nullptr);
    struct tm* timeinfo = gmtime(&now);
    strftime(newFileName, sizeof(newFileName), 
            "%Y%m%d_%H%M%S", timeinfo);
    
    std::string newPath = playerDir + "/" + newFileName + "_" + endCode + ".txt";
    rename(getGameFilePath().c_str(), newPath.c_str());
}

void Game::generateSecretKey() {
    const std::vector<char> colors = {'R', 'G', 'B', 'Y', 'O', 'P'};
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, colors.size() - 1);

    secretKey = "";
    for(int i = 0; i < 4; i++) {
        if (i > 0) secretKey += " "; 
        secretKey += colors[dis(gen)]; 
    }
}

bool Game::isTimeExceeded() {
    return (time(nullptr) - startTime) > maxTime;
}

// Server implementation
Server::Server(int port) : running(true) {
    std::cout << "Server running on port " << port << std::endl;
    setupDirectory();
    setupSockets(port);
    run();
}

void Server::setupDirectory() {
    // Create GAMES directory if it doesn't exist
    if (mkdir("GAMES", 0777) == -1) {
        if (errno != EEXIST) {
            perror("Error creating GAMES directory");
            exit(EXIT_FAILURE);
        }
    }

    // Create SCORES directory if it doesn't exist
    if (mkdir("SCORES", 0777) == -1) {
        if (errno != EEXIST) {
            perror("Error creating SCORES directory");
            exit(EXIT_FAILURE);
        }
    }
}
void Server::setupSockets(int port) {
    // Setup TCP socket
    tfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tfd == -1) {
        throw runtime_error("Failed to create TCP socket");
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    int errcode = getaddrinfo(NULL, to_string(port).c_str(), &hints, &res);
    if (errcode != 0) {
        throw runtime_error(string("getaddrinfo: ") + gai_strerror(errcode));
    }

    // reuse the address 
    int yes = 1;
    if (setsockopt(tfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        throw runtime_error("setsockopt failed");
    }

    if (bind(tfd, res->ai_addr, res->ai_addrlen) == -1) {
        throw runtime_error("TCP bind failed");
    }

    if (listen(tfd, SOMAXCONN) == -1) {
        throw runtime_error("TCP listen failed");
    }

    freeaddrinfo(res);

    // Setup UDP socket
    ufd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ufd == -1) {
        throw runtime_error("Failed to create UDP socket");
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    if ((errcode = getaddrinfo(NULL, to_string(port).c_str(), &hints, &res)) != 0) {
        throw runtime_error(string("getaddrinfo: ") + gai_strerror(errcode));
    }


    if (bind(ufd, res->ai_addr, res->ai_addrlen) == -1) {
        throw runtime_error("UDP bind failed");
    }

    freeaddrinfo(res);

    // Initialize fd sets
    FD_ZERO(&inputs);
    FD_SET(tfd, &inputs);
    FD_SET(ufd, &inputs);
    
}

void Server::run() {
    while (running) {
        testfds = inputs;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int ready = select(FD_SETSIZE, &testfds, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (errno == EINTR) continue;
            throw runtime_error("select error");
        }
        
        if (ready == 0) {
            cout << "Timeout - no activity\n";
            continue;
        }

        handleConnections();
    }
}

void Server::handleConnections() {
    if (FD_ISSET(ufd, &testfds)) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        std::string message = protocols::receiveUDPMessage(ufd, &client_addr, &addrlen);
        std::string response = handleRequest(message, false);
        
        // Send response back to client(falta usar sendUDPMessage !!!!!!!!!!!!)(Mudar o sendto para sendUDPMessage)
            sendto(ufd, response.c_str(), response.length(), 0,
                   (struct sockaddr*)&client_addr, addrlen);
    }
    
    
    if (FD_ISSET(tfd, &testfds)) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(tfd, (struct sockaddr*)&client_addr, &addrlen);
        
        if (client_fd >= 0) {
            std::string message = protocols::receiveTCPMessage(client_fd);
            std::string response = handleRequest(message, true);
            
            // Send response back to client(AQUI TAMBEM!!!!!!!!)(mudar o write para sendTCPMessage)
            write(client_fd, response.c_str(), response.length());
            close(client_fd);
        }
    }
}

std::string Server::handleRequest(const std::string& request, bool isTCP) {
    char command[10];
    sscanf(request.c_str(), "%s", command);

    if (strcmp(command, "SNG") == 0) {
        return handleStartGame(request);
    } else if (strcmp(command, "TRY") == 0) {
        return handleTry(request);
    } else if (strcmp(command, "QUT") == 0) {
        return handleQuitExit(request);
    } else if (strcmp(command, "DBG") == 0) {
        return handleDebug(request);
    } else if (strcmp(command, "STR") == 0 && isTCP) {
        return "TODO";
    } else if (strcmp(command, "SSB") == 0 && isTCP) {
        return "TODO";
    }
    return "ERR\n";
}

std::string Server::handleStartGame(const std::string& request) {
    char plid[7];
    int time;
    
    if (sscanf(request.c_str(), "SNG %s %d", plid, &time) != 2) {
        std::cerr << "Failed to parse SNG command\n";
        return "RSG ERR\n";
    }

    if (!isValidPlid(plid) || time <= 0 || time > 600) {
        std::cerr << "Invalid SNG command\n";
        return "RSG ERR\n";
    }

    auto it = activeGames.find(plid);
    // Check if player has an active game
    if (it != activeGames.end() && it->second.isActive()) {
        return "RSG NOK\n";
    }
    // Create a new game
    try {
        // Erase the old game if it exists
        if (it != activeGames.end()) {
            activeGames.erase(it);
        }
        Game newGame(plid, time, 'P'); // 'P' for Play mode
        std::cout << "PLID: " << plid << ": new game (max " << time << " sec);"
                  << " Colors: " << newGame.getSecretKey() << "\n";
        activeGames.insert(std::make_pair(plid, std::move(newGame)));
        return "RSG OK\n";
    } catch (const std::exception& e) {
        std::cerr << "Error creating game: " << e.what() << std::endl;
        return "RSG ERR\n";
    }
}


std::string Server::handleTry(const std::string& request) {
    char plid[7], c1[2], c2[2], c3[2], c4[2];
    int trialNum;
    
    if (sscanf(request.c_str(), "TRY %s %s %s %s %s %d", 
               plid, c1, c2, c3, c4, &trialNum) != 6) {
        std::cerr << "Failed to parse TRY command\n";
        return "RTR ERR\n";
    }

    if (!isValidPlid(plid)) {
        std::cerr << "Invalid TRY command\n";
        return "RTR ERR\n";
    }

    if (!isValidColor(c1) || !isValidColor(c2) || 
        !isValidColor(c3) || !isValidColor(c4)) {
        std::cerr << "Invalid TRY command\n";
        return "RTR ERR\n";
    }

    // Check if game exists
    auto it = activeGames.find(plid);
    if (it == activeGames.end()) {
        return "RTR NOK\n";
    }

    // Activate game if it's the first trial
    if (trialNum == 1) {
        it->second.setActive(true);
    }

    std::string secretKey;
    secretKey = it->second.getSecretKey();

    // Check if game has timed out
    if (it->second.isTimeExceeded()) {
        it->second.finalizeGame('T');
        return "RTR ETM " + secretKey + "\n";
    }

    Game& game = it->second;
    std::string guess = formatColors(c1, c2, c3, c4);
    const auto& trials = game.getTrials();

    // Handle trial number verification
    int expectedTrials = game.getTrialCount() + 1;
    if (trialNum == expectedTrials - 1) {
        // Check if this is resend of the last trial
        if (!trials.empty() && trials.back() == guess) {
            // Resend the last response
            int nB = 0, nW = 0;
            countMatches(c1, c2, c3, c4, secretKey, nB, nW);
            cout << "PLID: " << plid << ":try " << c1 << " " << c2 << " " << c3 
                 << " " << c4 << " nB: " << nB << " nW: " << nW << " not guessed\n";
            return "RTR OK " + std::to_string(trialNum) + " " + 
                   std::to_string(nB) + " " + std::to_string(nW) + "\n";
        }
        return "RTR INV\n";
    } else if (trialNum != expectedTrials) {
        return "RTR INV\n";
    }

    // Check for duplicate trial
    if (std::find(trials.begin(), trials.end(), guess) != trials.end()) {
        return "RTR DUP\n";
    }
    
    // Add trials and check if max attempts reached
    int nB = 0, nW = 0;
    countMatches(c1, c2, c3, c4, secretKey, nB, nW);
    game.addTrial(guess);
    game.appendTrialToFile(guess, nB, nW);
    
    if (game.getTrialCount() >= MAX_ATTEMPTS) {
        game.finalizeGame('F');
        return "RTR ENT " + secretKey + "\n";
    }

    // Check for win condition
    if (nB == 4) {
        game.finalizeGame('W');
        cout << "PLID: " << plid << ":try " << c1 << " " << c2 << " " << c3 
             << " " << c4 << " nB: " << nB << " nW: " << nW << " Win (game ended)\n";
        return "RTR OK " + std::to_string(trialNum) + " 4 0\n";
    }
    
    return "RTR OK " + std::to_string(trialNum) + " " + 
           std::to_string(nB) + " " + std::to_string(nW) + "\n";
}

void Server::countMatches(const std::string& c1, const std::string& c2,
                         const std::string& c3, const std::string& c4,
                         const std::string& secret,
                         int& nB, int& nW) {
    std::vector<std::string> secretColors;
    char secretStr[20];
    strcpy(secretStr, secret.c_str());
    
    char* token = strtok(secretStr, " ");
    while (token != NULL) {
        secretColors.push_back(std::string(token));
        token = strtok(NULL, " ");
    }
    
    std::vector<std::string> guess = {c1, c2, c3, c4};
    std::vector<bool> usedGuess(4, false);
    std::vector<bool> usedSecret(4, false);
    
    nB = 0;
    for (size_t i = 0; i < 4; i++) {
        if (guess[i] == secretColors[i]) {
            nB++;
            usedGuess[i] = true;
            usedSecret[i] = true;
        }
    }
    
    nW = 0;
    for (size_t i = 0; i < 4; i++) {
        if (usedGuess[i]) continue;
        
        for (size_t j = 0; j < 4; j++) {
            if (!usedSecret[j] && guess[i] == secretColors[j]) {
                nW++;
                usedSecret[j] = true;
                break;
            }
        }
    }
}
bool Server::isValidColor(const std::string& color) {
        return VALID_COLORS.find(color) != VALID_COLORS.end();
}

bool Server::isValidPlid(const std::string& plid) {
    if (plid.length() != 6) return false;
    return std::all_of(plid.begin(), plid.end(), ::isdigit);
}
std::string Server::formatColors(const std::string& c1, const std::string& c2, 
                           const std::string& c3, const std::string& c4) {
        return c1 + " " + c2 + " " + c3 + " " + c4;
    }

std::string Server::handleQuitExit(const std::string& request) {
    char plid[7];

    if(sscanf(request.c_str(), "QUT %s", plid) != 1) {
        std::cerr << "Failed to parse QUT command\n";
        return "RQT ERR\n";
    }

    if (!isValidPlid(plid)) {
        return "RQT ERR\n";
    }

    // Check if game exists
    auto it = activeGames.find(plid);
    if (it == activeGames.end() || !it->second.isActive()) {
        return "RQT NOK\n";
    }

    try {
        Game& game = it->second;
        std::string secretKey = game.getSecretKey();
        game.finalizeGame('Q');
        return "RQT OK " + secretKey + "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error finalizing game: " << e.what() << std::endl;
        return "RQT ERR\n";
    }
}

std::string Server::handleDebug(const std::string& request) {
    char plid[7], c1[2], c2[2], c3[2], c4[2];
    int time;
    
    if (sscanf(request.c_str(), "DBG %s %d %s %s %s %s", 
               plid, &time, c1, c2, c3, c4) != 6) {
        std::cerr << "Failed to parse DBG command\n";
        return "RDB ERR\n";
    }

    if (!isValidPlid(plid) || time <= 0 || time > 600) {
        std::cerr << "Invalid DBG command\n";
        return "RDB ERR\n";
    }

    if (!isValidColor(c1) || !isValidColor(c2) || 
        !isValidColor(c3) || !isValidColor(c4)) {
        std::cerr << "Invalid DBG command\n";
        return "RDB ERR\n";
    }

    std::string key = formatColors(c1, c2, c3, c4);
    
    // Check for active game
    auto it = activeGames.find(plid);
    if (it != activeGames.end() && it->second.isActive()) {
        return "RDB NOK\n";
    }

    try {
        // Erase the old game if it exists
        if (it != activeGames.end()) {
            activeGames.erase(it);
        }
        Game newGame(plid, time, 'D'); // 'D' for Debug mode
        newGame.setSecretKey(key);
        std::cout << "PLID: " << plid << ": new debug game (max " << time << " sec);"
                  << " Colors: " << key << "\n";
        activeGames.insert(std::make_pair(plid, std::move(newGame)));
        return "RDB OK\n";
    } catch (const std::exception& e) {
        std::cerr << "Error creating debug game: " << e.what() << std::endl;
        return "RDB ERR\n";
    }
}

int main(int argc, char* argv[]) {
    int port = DSPORT_DEFAULT;
    if (argc > 1) {
        port = stoi(argv[1]);
    }

    try {
        Server server(port);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}