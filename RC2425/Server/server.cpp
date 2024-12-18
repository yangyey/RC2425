#include "server.hpp"
#include "../constant.hpp"
#include "../utils.hpp"



using namespace std;

// Game implementation
Game::Game(const std::string& pid, int maxPlayTime, char mode)
    : plid(pid), maxTime(maxPlayTime), active(true), gameMode(mode) {
    startTime = time(nullptr);
    generateSecretKey();
    saveInitialState();

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
    
    // Save score file only for winning games
    if (endCode == 'W') {
        try {
            saveScoreFile();
        } catch (const std::exception& e) {
            std::cerr << "Error saving score file: " << e.what() << std::endl;
        }
    }
    
    // Create player directory if needed
    std::string playerDir = "Server/GAMES/" + plid;
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

int Game::calculateScore() const {
    // Calculate time component (0-50 points)
    time_t now = time(nullptr);
    int timeTaken = now - startTime;
    double timePercentage = std::max(0.0, 1.0 - (double)timeTaken / maxTime);
    int timeScore = static_cast<int>(timePercentage * 50);

    // Calculate trials component (0-50 points)
    double trialsPercentage = 1.0 - (double)trials.size() / MAX_ATTEMPTS;
    int trialScore = static_cast<int>(trialsPercentage * 50);

    // Combine scores and ensure bounds
    return std::min(100, std::max(0, timeScore + trialScore));
}

void Game::saveScoreFile() const {
    time_t now = time(nullptr);
    struct tm* timeinfo = gmtime(&now);
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%d%m%Y_%H%M%S", timeinfo);
    
    int score = calculateScore();
    
    // Format: "NNN PLID DDMMYYYY HHMMSS.txt"
    std::string scoreFileName = "Server/SCORES/" + 
                               std::to_string(score) + "_" +
                               plid + "_" +
                               std::string(timeStr) + ".txt";
                               
    std::ofstream scoreFile(scoreFileName);
    if (!scoreFile) {
        throw std::runtime_error("Cannot create score file");
    }
    
    // Format: "SSS PPPPPP CCCC N mode"
    scoreFile << std::setfill('0') << std::setw(3) << score << " "
              << plid << " "
              << secretKey << " "
              << trials.size() << " "
              << (gameMode == 'D' ? "DEBUG" : "PLAY")
              << std::endl;
              
    scoreFile.close();
}

// Server implementation
Server::Server(int port, bool verboseMode) : verbose(verboseMode) {
    std::cout << "Server running on port " << port << std::endl;
    setupDirectory();
    setupSockets(port);
    run();
}

void Server::setupDirectory() {
    // Create GAMES directory if it doesn't exist
    if (mkdir("Server/GAMES", 0777) == -1) {
        if (errno != EEXIST) {
            perror("Error creating GAMES directory");
            exit(EXIT_FAILURE);
        }
    }

    // Create SCORES directory if it doesn't exist
    if (mkdir("Server/SCORES", 0777) == -1) {
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
    while (true) {
        testfds = inputs;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int ready = select(FD_SETSIZE, &testfds, NULL, NULL, &timeout);
        
        if (ready < 0) {
            if (errno == EINTR) continue;
            throw runtime_error("select error");
        }
        
        // if (ready == 0) {
        //     cout << "Timeout - no activity\n";
        //     continue;
        // }

        if (FD_ISSET(ufd, &testfds)) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            std::string message = protocols::receiveUDPMessage(ufd, &client_addr, &addrlen);
            std::string response = handleRequest(message, false, &client_addr);
            
            protocols::sendUDPMessage(ufd, response, &client_addr, addrlen);
        }
    
    
        if (FD_ISSET(tfd, &testfds)) {
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int client_fd = accept(tfd, (struct sockaddr*)&client_addr, &addrlen);
            
            if (client_fd >= 0) {
                std::string message = protocols::receiveTCPMessage(client_fd);
                std::string response = handleRequest(message, true, &client_addr);
                
                protocols::sendTCPMessage(client_fd, response);

                close(client_fd);
            }
        }
    }
}

std::string Server::formatClientInfo(const struct sockaddr_in* client_addr) {
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), ipstr, INET_ADDRSTRLEN);
    return std::string(ipstr) + ":" + std::to_string(ntohs(client_addr->sin_port));
}

std::string Server::handleRequest(const std::string& request, bool isTCP, 
                                    const struct sockaddr_in* client_addr) {
    char command[10], plid[7];
    sscanf(request.c_str(), "%s", command);

    if (verbose) {
        // Extract PLID if present in command
        bool hasPlid = false;
        if (sscanf(request.c_str(), "%*s %s", plid) == 1) {
            hasPlid = true;
        }

        std::cout << "Request received: " << command;
        if (hasPlid) {
            std::cout << " from PLID: " << plid;
        }
        std::cout << " via " << (isTCP ? "TCP" : "UDP");
        
        if (client_addr != nullptr) {
            std::cout << " from " << formatClientInfo(client_addr);
        }
        std::cout << std::endl;
    }

    if (strcmp(command, REQUEST_START) == 0) {
        return handleStartGame(request);
    } else if (strcmp(command, REQUEST_TRY) == 0) {
        return handleTry(request);
    } else if (strcmp(command, REQUEST_QUIT) == 0) {
        return handleQuitExit(request);
    } else if (strcmp(command, REQUEST_DEBUG) == 0) {
        return handleDebug(request);
    } else if (strcmp(command, REQUEST_SHOW_TRIALS) == 0 && isTCP) {
        return handleShowTrials(request);
    } else if (strcmp(command, REQUEST_SCOREBOARD) == 0 && isTCP) {
        return handleScoreBoard();
    }
    return "ERR\n";
}

bool Server::hasActiveTry(const std::string& plid) {
    std::string gamePath = "Server/GAMES/GAME_" + plid + ".txt";
    FILE* file = fopen(gamePath.c_str(), "r");
    if (!file) {
        return false;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "T:", 2) == 0) {
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

std::string Server::handleStartGame(const std::string& request) {
    char plid[7];
    int time;
    bool erased = false;

    if (sscanf(request.c_str(), "SNG %s %d", plid, &time) != 2) {
        std::cerr << "Failed to parse SNG command\n";
        return "RSG ERR\n";
    }

    if (!isValidPlid(plid) || time <= 0 || time > 600) {
        std::cerr << "Invalid SNG command\n";
        return "RSG ERR\n";
    }

    // Check if game already exists and finalize it if it is time exceeded
    auto it = activeGames.find(plid);
    if (it != activeGames.end()) {
        if (it->second.isTimeExceeded()) {
            it->second.finalizeGame('T');
            activeGames.erase(it);
            erased = true;
        } else if (hasActiveTry(plid)) {
            return "RSG NOK\n";
        }
    }

    // Create a new game
    try {
        // Erase the old game if it exists
        if (it != activeGames.end() && erased == false) {
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

    std::string secretKey;
    secretKey = it->second.getSecretKey();

    // Check if game has timed out
    if (it->second.isTimeExceeded()) {
        it->second.finalizeGame('T');
        activeGames.erase(it);
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
        activeGames.erase(it);
        return "RTR ENT " + secretKey + "\n";
    }

    // Check for win condition
    if (nB == 4) {
        game.finalizeGame('W');
        activeGames.erase(it);
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

    auto it = activeGames.find(plid);

    // Check if game exists
    if (it == activeGames.end()) {
        return "RQT NOK\n";
    }

    if (it->second.isTimeExceeded()) {
        it->second.finalizeGame('T');
        activeGames.erase(it);
        return "RQT NOK\n";
    }


    try {
        Game& game = it->second;
        std::string secretKey = game.getSecretKey();
        game.finalizeGame('Q');
        activeGames.erase(it); 
        return "RQT OK " + secretKey + "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error finalizing game: " << e.what() << std::endl;
        return "RQT ERR\n";
    }
}

std::string Server::handleDebug(const std::string& request) {
    char plid[7], c1[2], c2[2], c3[2], c4[2];
    int time;
    bool erased = false;
    
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

    // Check for active game and tries
    auto it = activeGames.find(plid);
    if (it != activeGames.end()) {
        if (it->second.isTimeExceeded()) {
            it->second.finalizeGame('T');
            activeGames.erase(it);
            erased = true;
        } else if (hasActiveTry(plid)) {
            return "RDB NOK\n";
        }
    }

    std::string key = formatColors(c1, c2, c3, c4);
    
    try {
        // Erase the old game if it exists
        if (it != activeGames.end() && erased == false) {
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

std::string Server::handleShowTrials(const std::string& request) {
    char plid[7];

    if (sscanf(request.c_str(), "STR %s", plid) != 1) {
        std::cerr << "Failed to parse STR command\n";
        return "RST NOK\n";
    }

    if (!isValidPlid(plid)) {
        return "RST NOK\n";
    }

    auto it = activeGames.find(plid);

    if (it->second.isTimeExceeded()) {
        it->second.finalizeGame('T');
        activeGames.erase(it);
    }

    // Check for active game first
    if (it != activeGames.end() && it->second.isActive()) {
        return processActiveGame(plid, it->second);
    }

    // No active game - look for finished game
    return processFinishedGame(plid);
}

std::vector<std::string> Server::readGameFile(const std::string& filePath) {
    std::vector<std::string> lines;
    FILE* file = fopen(filePath.c_str(), "r");
    if (!file) {
        throw std::runtime_error("Cannot open game file");
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        lines.push_back(std::string(line));
    }
    
    fclose(file);
    
    if (lines.empty()) {
        throw std::runtime_error("Empty game file");
    }
    
    return lines;
}

std::string Server::formatGameHeader(const std::string& plid, const std::string& date, 
                                   const std::string& time, int maxTime) {
    std::stringstream ss;
    ss << "     Active game found for player " << plid << "\n"
       << "Game initiated: " << date << " " << time 
       << " with " << maxTime << " seconds to be completed\n\n";
    return ss.str();
}

std::string Server::formatTrials(const std::vector<std::string>& lines) {
    std::vector<std::string> trials;
    for (const auto& line : lines) {
        if (line.substr(0, 2) == "T:") {
            trials.push_back(line);
        }
    }

    std::stringstream ss;
    ss << "     --- Transactions found: " << trials.size() << " ---\n\n";

    for (const auto& trial : lines) {
        if (trial.substr(0, 2) == "T:") {
            char c1, c2, c3, c4;
            int nB, nW, seconds;
            if (sscanf(trial.c_str(), "T: %c %c %c %c %d %d %d", 
                      &c1, &c2, &c3, &c4, &nB, &nW, &seconds) == 7) {
                ss << "Trial: " << c1 << c2 << c3 << c4 
                   << ", nB: " << nB << ", nW: " << nW 
                   << " at " << std::setw(3) << seconds << "s\n";
            }
        }
    }
    ss << "\n";
    return ss.str();
}

std::string Server::formatRemainingTime(int remainingTime) {
    std::stringstream ss;
    ss << "  -- " << remainingTime << " seconds remaining to be completed -- \n";
    return ss.str();
}

std::string Server::processActiveGame(const std::string& plid, Game& game) {
    try {
        std::vector<std::string> lines = readGameFile(game.getGameFilePath());
        if (lines.empty()) {
            return "RST NOK\n";
        }

        // Parse header line: PLID MODE C1 C2 C3 C4 TIME DATE TIME TIMESTAMP
        char mode;
        int maxTime;
        char date[11], realTime[9];  // YYYY-MM-DD + null, HH:MM:SS + null
        time_t startTime;
        if (sscanf(lines[0].c_str(), "%*s %c %*s %*s %*s %*s %d %s %s %ld",
                  &mode, &maxTime, date, realTime, &startTime) < 5) {
            return "RST NOK\n";
        }

        // Format header
        std::string content = formatGameHeader(plid, date, realTime, maxTime);

        // Format trials
        content += formatTrials(lines);
        
        // Add remaining time
        int remainingTime = maxTime - (time(nullptr) - startTime);
        content += formatRemainingTime(remainingTime);

        return "RST ACT STATE_" + plid + ".txt " +
               std::to_string(content.length()) + " " + content;
    } catch (const std::exception& e) {
        std::cerr << "Error processing active game: " << e.what() << std::endl;
        return "RST NOK\n";
    }
}

std::string Server::processFinishedGame(const std::string& plid) {
    char fname[256];
    char terminated[2];
    std::string termination;
    if (!FindLastGame(plid.c_str(), fname)) {
        return "RST NOK\n";
    }
    char* last_underscore = strrchr(fname, '_');
    if (last_underscore != nullptr) {
        terminated[0] = *(last_underscore + 1);  // Get the character after the last underscore
        terminated[1] = '\0';
    }

    if (terminated[0] == 'W') {
        termination = "WIN";
    } else if (terminated[0] == 'F') {
        termination = "FAIL";
    } else if (terminated[0] == 'T') {
        termination = "TIMEOUT";
    } else if (terminated[0] == 'Q') {
        termination = "QUIT";
    } else {
        termination = "UNKNOWN";
    }

    try {
        std::vector<std::string> lines = readGameFile(fname);
        if (lines.empty()) {
            return "RST NOK\n";
        }

        // Parse header line
        char mode;
        int maxTime;
        char date[11], time[9];
        time_t startTime;
        if (sscanf(lines[0].c_str(), "%*s %c %*s %*s %*s %*s %d %s %s %ld",
                  &mode, &maxTime, date, time, &startTime) < 5) {
            return "RST NOK\n";
        }

        std::string content = formatGameHeader(plid, date, time, maxTime);
        content += formatTrials(lines);
        
        if (lines.size() >= 2) {
            std::string lastLine = lines.back();
            char date[11], time[9];  // YYYY-MM-DD + null, HH:MM:SS + null
            int duration;
            
            if (sscanf(lastLine.c_str(), "%s %s %d", date, time, &duration) == 3) {
                std::string timestamp = std::string(date) + " " + std::string(time);
                content += "Termination: " + termination + " at " + timestamp + 
                        ", Duration: " + std::to_string(duration) + "s\n";
            }
        }   

        return "RST FIN STATE_" + plid + ".txt " + 
               std::to_string(content.length()) + " " + content;
    } catch (const std::exception& e) {
        std::cerr << "Error processing finished game: " << e.what() << std::endl;
        return "RST NOK\n";
    }
}

int Server::FindLastGame(const char* PLID, char* fname) {
    struct dirent** filelist;
    int n_entries, found;
    char dirname[50];

    snprintf(dirname, sizeof(dirname), "Server/GAMES/%s/", PLID);

    n_entries = scandir(dirname, &filelist, nullptr, alphasort);
    found = 0;

    if (n_entries <= 0) {
        return 0;
    } else {
        while (n_entries-- > 0) {
            if (filelist[n_entries]->d_name[0] != '.' && !found) {
                sprintf(fname, "Server/GAMES/%s/%s", PLID, filelist[n_entries]->d_name);
                found = 1;
            }
            free(filelist[n_entries]);
        }
        free(filelist);
    }

    return found;
}

std::string Server::handleScoreBoard() {
    SCORELIST scores;
    int numScores = FindTopScores(&scores);
    
    if (numScores == 0) {
        return "RSS EMPTY\n";
    }

    // Get current time for filename
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%d%m%y_%H:%M:%S", timeinfo);
    
    // Create filename
    std::string fileName = "TOP_10_SCORES_" + std::string(timestamp) + ".txt";

    std::stringstream content;
    
    // Rest of the content formatting remains the same
    content << "-------------------------------- TOP 10 SCORES --------------------------------\n\n"
           << "                 SCORE PLAYER     CODE    NO TRIALS   MODE\n\n";

    for (int i = 0; i < numScores; i++) {
        content << "            " 
                << std::right << std::setw(2) << (i + 1) << " - "
                << std::right << std::setw(4) << scores.score[i] << "  "
                << std::left << std::setw(10) << scores.PLID[i] << " "
                << std::left << std::setw(8) << scores.col_code[i] << "    "
                << std::right << std::setw(1) << scores.no_tries[i] << "       "
                << std::left << scores.mode[i]
                << "\n";
    }
    
    
    
    std::string fileContent = content.str();
    return "RSS OK " + fileName + " " + 
           std::to_string(fileContent.length()) + " " + 
           fileContent;
}

int Server::FindTopScores(SCORELIST* list) {
    struct dirent** filelist;
    int n_entries, i_file = 0;
    char fname[300];
    FILE* fp;
    char mode[8];
    char c1, c2, c3, c4;

    // Scan SCORES directory for files
    n_entries = scandir("Server/SCORES/", &filelist, nullptr, alphasort);
    if (n_entries <= 0) {
        return 0;
    }

    while (n_entries--) {
        if (filelist[n_entries]->d_name[0] != '.' && i_file < 10) {
            snprintf(fname, sizeof(fname), "Server/SCORES/%s", filelist[n_entries]->d_name);
            fp = fopen(fname, "r");
            if (fp != NULL) {
                if (fscanf(fp, "%d %s %c %c %c %c %d %s",
                           &list->score[i_file],
                           list->PLID[i_file],
                           &c1, &c2, &c3, &c4,
                           &list->no_tries[i_file],
                           mode) == 8) {
                    sprintf(list->col_code[i_file], "%c%c%c%c", c1, c2, c3, c4);
                    // Set mode based on the read value
                    if (strcmp(mode, "PLAY") == 0) {
                        strcpy(list->mode[i_file], "PLAY");
                    } else if (strcmp(mode, "DEBUG") == 0) {
                        strcpy(list->mode[i_file], "DEBUG");
                    }
                    i_file++;
                }
                fclose(fp);
            }
        }
        free(filelist[n_entries]);
    }
    free(filelist);

    list->n_scores = i_file;
    return i_file;
}

int main(int argc, char* argv[]) {
    int port = DSPORT_DEFAULT;
    bool verbose = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++; // Skip next argument
        }
        else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        }
        else {
            std::cerr << "Usage: " << argv[0] << " [-p GSport] [-v]" << std::endl;
            return 1;
        }
    }

    // Validate port number
    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port number. Using default port " << DSPORT_DEFAULT << std::endl;
        port = DSPORT_DEFAULT;
    }

    try {
        Server server(port, verbose);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}