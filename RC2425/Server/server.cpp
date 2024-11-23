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
    const std::string charset = "0123456789";
    secretKey = "";
    for(int i = 0; i < 4; i++) {
        secretKey += charset[rand() % charset.length()];
    }
}

bool Game::isTimeExceeded() {
    return (time(nullptr) - startTime) > maxTime;
}

// Server implementation
Server::Server(int port) : running(true) {
    setupSockets(port);
    run();
}


void Server::setupSockets(int port) {
    // Setup TCP socket
    tfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tfd == -1) {
        freeaddrinfo(res);
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
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    if ((errcode = getaddrinfo(NULL, to_string(port).c_str(), &hints, &res)) != 0) {
        throw runtime_error(string("getaddrinfo: ") + gai_strerror(errcode));
    }

    ufd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ufd == -1) {
        freeaddrinfo(res);
        throw runtime_error("Failed to create UDP socket");
    }

    if (bind(ufd, res->ai_addr, res->ai_addrlen) == -1) {
        throw runtime_error("UDP bind failed");
    }

    freeaddrinfo(res);

    // Initialize fd sets
    FD_ZERO(&inputs);
    FD_SET(tfd, &inputs);
    FD_SET(ufd, &inputs);
    
    max_fd = max(tfd, ufd) + 1;
}

void Server::run() {
    while (running) {
        testfds = inputs;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int ready = select(max_fd, &testfds, NULL, NULL, &timeout);
        
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
        receiveUDPMessage();
    }
    
    if (FD_ISSET(tfd, &testfds)) {
        receiveTCPMessage();
    }
}

void Server::receiveUDPMessage() {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ssize_t n = recvfrom(ufd, buffer, BUFFER_SIZE-1, 0,
                    (struct sockaddr*)&client_addr, &addr_len);
        
    if (n == -1) return;
    
    buffer[n] = '\0';
    char host[NI_MAXHOST], service[NI_MAXSERV];
    
    if (getnameinfo((struct sockaddr*)&client_addr, addr_len,
                host, sizeof(host), service, sizeof(service), 0) == 0) {
        cout << "UDP message from " << host << ":" << service << endl;
        string response = handleRequest(string(buffer), false);
        sendto(ufd, response.c_str(), response.length(), 0, 
               (struct sockaddr*)&client_addr, addr_len);
    }
}

void Server::receiveTCPMessage() {
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(tfd, (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_fd < 0) {
        cerr << "Accept failed\n";
        return;
    }

    ssize_t n = read(client_fd, buffer, BUFFER_SIZE-1);
    if (n > 0) {
        buffer[n] = '\0';
        char host[NI_MAXHOST], service[NI_MAXSERV];
        if (getnameinfo((struct sockaddr*)&client_addr, addr_len,
                   host, sizeof(host), service, sizeof(service), 0) == 0) {
            cout << "TCP connection from " << host << ":" << service << endl;
            string response = handleRequest(string(buffer), true);
            write(client_fd, response.c_str(), response.length());
        }
    }

    close(client_fd);
}

std::string Server::handleRequest(const std::string& request, bool isTCP) {
    std::istringstream iss(request);
    std::string command;
    iss >> command;

    if (command == "SNG") {
        // return handleStartGame(iss);
        return "TODO";
    } else if (command == "TRY") {
        // return handleTry(iss);
        return "TODO";
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

// std::string Server::handleStartGame(std::istringstream& iss) {
//     std::string plid;
//     int time;
//     iss >> plid >> time;

//     if (plid.length() != 6 || time <= 0 || time > 600) {
//         return "RSG ERR\n";
//     }

//     if (activeGames.find(plid) != activeGames.end()) {
//         return "RSG NOK\n";
//     }

//     activeGames.emplace(plid, Game(plid, time));
//     return "RSG OK " + activeGames[plid].getSecretKey() + "\n";
// }

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
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    try {
        Server server(std::stoi(argv[1]));
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}