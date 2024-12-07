#include "server.hpp"
#include "../constant.hpp"
#include "../utils.hpp"
#include <cstdlib>
#include <algorithm>
#include <random>

using namespace std;

// Game implementation
Game::Game(const std::string& pid, int maxPlayTime) 
    : plid(pid), maxTime(maxPlayTime), active(true) {
    startTime = time(nullptr);
    generateSecretKey();
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
    setupSockets(port);
    run();
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
        
        // Send response back to client(falta usar sendUDPMessage !!!!!!!!!!!!)
            sendto(ufd, response.c_str(), response.length(), 0,
                   (struct sockaddr*)&client_addr, addrlen);
        }
    }
    
    if (FD_ISSET(tfd, &testfds)) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(tfd, (struct sockaddr*)&client_addr, &addrlen);
        
        if (client_fd >= 0) {
            std::string message = protocols::receiveTCPMessage(client_fd);
            std::string response = handleRequest(message, true);
            
            // Send response back to client(AQUI TAMBEM!!!!!!!!)
            write(client_fd, response.c_str(), response.length());
            close(client_fd);
        }
    }
}

std::string Server::handleRequest(const std::string& request, bool isTCP) {
    std::istringstream iss(request);
    std::string command;
    iss >> command;

    if (command == "SNG") {
        return handleStartGame(iss);
    } else if (command == "TRY") {
        return handleTry(iss);
    } else if (command == "QUT") {
        return handleQuitExit(iss);
    } else if (command == "DBG") {
        return handleDebug(iss);
    } else if (command == "STR" && isTCP) {
        // return handleShowTrials(iss);
        return "TODO";
    } else if (command == "SSB" && isTCP) {
        // return handleScoreboard();
        return "TODO";
    }
    return "ERR\n";
}

std::string Server::handleStartGame(std::istringstream& iss) {
    std::string plid;
    std::string key;
    int time;
    
    if (!(iss >> plid >> time)) {
        std::cerr << "Failed to parse SNG command\n";
        return "RSG ERR\n";
    }

    if (plid.length() != 6 || time <= 0 || time > 600) {
        std::cerr << "Invalid SNG command\n";
        return "RSG ERR\n";
    }

    auto it = activeGames.find(plid);
    if (it != activeGames.end() && it->second.isActive()) {
        std::cerr << "Duplicate SNG command(player " << plid 
                  << " already has an ongoing game\n";
        return "RSG NOK\n";
    } else {
        // Erase the existing game
        if (it != activeGames.end()) {
            activeGames.erase(it);
        }
        // Create a new game
        Game newGame(plid, time);
        key = newGame.getSecretKey();
        activeGames.insert(std::make_pair(plid, std::move(newGame)));
    }

    std::cout << "PLID: " << plid << ": new game (max " << time << " sec);"
              << " Colors: " << key << "\n";
    return "RSG OK\n";
}


std::string Server::handleTry(std::istringstream& iss) {
    std::string plid, c1, c2, c3, c4;
    int trialNum;
    
    // Parse input
    if (!(iss >> plid >> c1 >> c2 >> c3 >> c4 >> trialNum)) {
        std::cerr << "Failed to parse TRY command\n";
        return "RTR ERR\n";
    }

    // Validate PLID format
    if (!isValidPlid(plid)) {
        std::cerr << "Invalid TRY command\n";
        return "RTR ERR\n";
    }

    // Validate colors
    if (!isValidColor(c1) || !isValidColor(c2) || 
        !isValidColor(c3) || !isValidColor(c4)) {
        std::cerr << "Invalid TRY command\n";
        return "RTR ERR\n";
    }

    // Validate PLID and find game
    auto it = activeGames.find(plid);
    if (it == activeGames.end() || !it->second.isActive()) {
        return "RTR NOK\n";
    }

    Game& game = it->second;
    std::string guess = formatColors(c1, c2, c3, c4);
    std::string secret = game.getSecretKey();

    // Check for timeout
    if (game.isTimeExceeded()) {
        game.setActive(false);
        return "RTR ETM " + secret + "\n";
    }

    // Validate trial number
    int expectedTrials = game.getTrialCount() + 1;
    if (trialNum == expectedTrials - 1) {
        // Check if this is a resend of the last trial
        const auto& trials = game.getTrials();
        if (!trials.empty() && trials.back() == guess) {
            // Calculate scores for resend
            int nB = 0, nW = 0;
            countMatches(c1, c2, c3, c4, secret, nB, nW);
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
    const auto& trials = game.getTrials();
    if (std::find(trials.begin(), trials.end(), guess) != trials.end()) {
        return "RTR DUP\n";
    }

    // Add trial and check if max attempts reached
    game.addTrial(guess);
    if (game.getTrialCount() >= MAX_ATTEMPTS) {
        game.setActive(false);
        return "RTR ENT " + secret + "\n";
    }

    // Calculate matches
    int nB = 0, nW = 0;
    countMatches(c1, c2, c3, c4, secret, nB, nW);

    // Check for win condition
    if (nB == 4) {
        game.setActive(false);
        scoreboard.push_back({plid, game.getTrialCount()});
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
    std::istringstream iss(secret);
    std::vector<std::string> secretColors;
    std::string color;
    
    // Parse secret key into individual colors
    while (iss >> color) {
        secretColors.push_back(color);
    }
    
    std::vector<std::string> guess = {c1, c2, c3, c4};
    std::vector<bool> usedGuess(4, false);
    std::vector<bool> usedSecret(4, false);
    
    // Count exact matches (nB)
    nB = 0;
    for (size_t i = 0; i < 4; i++) {
        if (guess[i] == secretColors[i]) {
            nB++;
            usedGuess[i] = true;
            usedSecret[i] = true;
        }
    }
    
    // Count wrong position matches (nW)
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
    // PLID should be exactly 6 digits
    if (plid.length() != 6) return false;
    return std::all_of(plid.begin(), plid.end(), ::isdigit);
}
std::string Server::formatColors(const std::string& c1, const std::string& c2, 
                           const std::string& c3, const std::string& c4) {
        return c1 + " " + c2 + " " + c3 + " " + c4;
    }

std::string Server::handleQuitExit(std::istringstream& iss) {
    std::string plid;

    if(!(iss >> plid)) {
        std::cerr << "Failed to parse QUT command\n";
    }

    // Validate PLID format
    if (!isValidPlid(plid)) {
        return "RTR ERR\n";
    }

    // Validate PLID and find game
    auto it = activeGames.find(plid);
    if (it == activeGames.end() || !it->second.isActive()) {
        return "RTR NOK\n";
    }

    Game& game = it->second;
    std::string secret = game.getSecretKey();
    game.setActive(false);
    return "RQT OK " + secret + "\n";
    
}

std::string Server::handleDebug(std::istringstream& iss) {
    std::string plid, c1, c2, c3, c4, key;
    int time;
    
    // Parse input
    if (!(iss >> plid >> time >> c1 >> c2 >> c3 >> c4)) {
        std::cerr << "Failed to parse DBG command\n";
        return "RDB ERR\n";
    }

    // Validate PLID format
    if (!isValidPlid(plid) || time <= 0 || time > 600) {
        std::cerr << "Invalid DBG command\n";
        return "RDB ERR\n";
    }

    // Validate colors
    if (!isValidColor(c1) || !isValidColor(c2) || 
        !isValidColor(c3) || !isValidColor(c4)) {
        std::cerr << "Invalid DBG command\n";
        return "RDB ERR\n";
    }

    // Check for existing active game
    auto it = activeGames.find(plid);
    key = formatColors(c1, c2, c3, c4);
    if (it != activeGames.end() && it->second.isActive()) {
        return "RDB NOK\n";
    } else {
        // Erase the existing game
        if (it != activeGames.end()) {
            activeGames.erase(it);
        }
        // Create a new game
        Game newGame(plid, time);
        newGame.setSecretKey(key);
        activeGames.insert(std::make_pair(plid, std::move(newGame)));
    }

    std::cout << "PLID: " << plid << ": new game (max " << time << " sec);"
              << " Colors: " << key << "\n";

    return "RDB OK\n";
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