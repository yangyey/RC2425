#include "server.hpp"
#include "../constant.hpp"
#include "../utils.hpp"
#include <cstdlib>
#include <algorithm>

using namespace std;

// Game implementation
Game::Game(const std::string& pid, int maxPlayTime) 
    : plid(pid), maxTime(maxPlayTime), active(true) {
    startTime = time(nullptr);
    generateSecretKey();
}

void Game::generateSecretKey() {
    const std::vector<char> colors = {'R', 'G', 'B', 'Y', 'O', 'P'};  // Valid colors
    secretKey = "";
    for(int i = 0; i < 4; i++) {
        if (i > 0) secretKey += " ";  // Add space between colors
        secretKey += colors[rand() % colors.size()];  // Randomly select a color
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
        
        // Send response back to client
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
            
            // Send response back to client
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
        // return handleQuit(iss);
        return "TODO";
    } else if (command == "DBG") {
        // return handleDebug(iss);
        return "TODO";
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
    }

    // Create game object and insert it into the map
    Game newGame(plid, time);
    std::string key = newGame.getSecretKey();
    activeGames.insert(std::make_pair(plid, std::move(newGame)));
    std::cout << "New game started for player " << plid 
              << "with key: " << key << "\n";
    return "RSG OK\n";
}


std::string Server::handleTry(std::istringstream& iss) {
    std::string plid, c1, c2, c3, c4;
    int trialNum;
    
    // Parse input
    if (!(iss >> plid >> c1 >> c2 >> c3 >> c4 >> trialNum)) {
        return "RTR ERR\n";
    }

    // Validate PLID and find game
    auto it = activeGames.find(plid);
    if (it == activeGames.end() || !it->second.isActive()) {
        return "RTR NOK\n";
    }

    Game& game = it->second;
    std::string guess = c1 + " " + c2 + " " + c3 + " " + c4;
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
            countMatches(guess, secret, nB, nW);
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
    countMatches(guess, secret, nB, nW);

    // Check for win condition
    if (nB == 4) {
        game.setActive(false);
        scoreboard.push_back({plid, game.getTrialCount()});
        return "RTR WIN\n";
    }

    return "RTR OK " + std::to_string(trialNum) + " " + 
           std::to_string(nB) + " " + std::to_string(nW) + "\n";
}

void Server::countMatches(const std::string& guess, const std::string& secret, 
                         int& nB, int& nW) {
    std::string tempSecret = secret;
    std::string tempGuess = guess;
    
    // Count exact matches (nB)
    nB = 0;
    for (size_t i = 0; i < guess.length(); i++) {
        if (guess[i] == secret[i]) {
            nB++;
            tempGuess[i] = 'X';
            tempSecret[i] = 'Y';
        }
    }
    
    // Count wrong position matches (nW)
    nW = 0;
    for (size_t i = 0; i < tempGuess.length(); i++) {
        if (tempGuess[i] == 'X') continue;
        
        size_t pos = tempSecret.find(tempGuess[i]);
        if (pos != std::string::npos) {
            nW++;
            tempSecret[pos] = 'Y';
        }
    }
}

// std::string Server::handleTry(std::istringstream& iss) {
//     std::string plid, guess;
//     iss >> plid >> guess;

//     if (activeGames.find(plid) == activeGames.end()) {
//         return "RTR ERR\n";
//     }

//     Game& game = activeGames[plid];
//     if (!game.isActive() || game.isTimeExceeded()) {
//         return "RTR ERR\n";
//     }

//     if (guess.length() != 4 || !std::all_of(guess.begin(), guess.end(), ::isdigit)) {
//         return "RTR ERR\n";
//     }

//     game.addTrial(guess);
    
//     if (guess == game.getSecretKey()) {
//         game.setActive(false);
//         scoreboard.push_back({plid, game.getTrialCount()});
//         return "RTR WIN\n";
//     }

//     int correctPos = 0;
//     int correctDigits = 0;
//     std::string secret = game.getSecretKey();
    
//     // Count correct positions
//     for (int i = 0; i < 4; i++) {
//         if (guess[i] == secret[i]) correctPos++;
//     }
    
//     // Count correct digits
//     std::string tempSecret = secret;
//     std::string tempGuess = guess;
//     for (char c : tempGuess) {
//         size_t pos = tempSecret.find(c);
//         if (pos != std::string::npos) {
//             correctDigits++;
//             tempSecret[pos] = 'X';
//         }
//     }

//     return "RLG " + std::to_string(correctPos) + " " + std::to_string(correctDigits) + "\n";
// }

// std::string Server::handleQuit(std::istringstream& iss) {
//     std::string plid;
//     iss >> plid;

//     auto it = activeGames.find(plid);
//     if (it == activeGames.end() || !it->second.isActive()) {
//         return "RQT ERR\n";
//     }

//     it->second.setActive(false);
//     scoreboard.push_back({plid, it->second.getTrialCount()});
//     return "RQT OK\n";
// }

// std::string Server::handleDebug(std::istringstream& iss) {
//     std::string response = "Status of active games:\n";
//     for (const auto& pair : activeGames) {
//         response += "PLID: " + pair.first + 
//                    " Secret: " + pair.second.getSecretKey() + 
//                    " Active: " + (pair.second.isActive() ? "yes" : "no") + "\n";
//     }
//     return response;
// }

// std::string Server::handleShowTrials(std::istringstream& iss) {
//     std::string plid;
//     iss >> plid;

//     auto it = activeGames.find(plid);
//     if (it == activeGames.end()) {
//         return "RGH ERR\n";
//     }

//     std::string response = "RGH OK " + std::to_string(it->second.getTrials().size());
//     for (const auto& trial : it->second.getTrials()) {
//         response += " " + trial;
//     }
//     response += "\n";
//     return response;
// }

// std::string Server::handleScoreboard() {
//     if (scoreboard.empty()) {
//         return "RSB EMPTY\n";
//     }

//     // Sort scoreboard by number of trials (ascending)
//     std::sort(scoreboard.begin(), scoreboard.end(),
//               [](const auto& a, const auto& b) { return a.second < b.second; });

//     std::string response = "RSB OK";
//     for (const auto& score : scoreboard) {
//         response += " " + score.first + " " + std::to_string(score.second);
//     }
//     response += "\n";
//     return response;
// }

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