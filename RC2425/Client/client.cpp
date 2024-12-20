#include "client.hpp"
#include "../constant.hpp"
#include "../utils.hpp"

using namespace std;
using namespace protocols;

// Constructor
GameClient::GameClient(int argc, char** argv) 
    : serverIP(DSIP_DEFAULT), 
      serverPort(DSPORT_DEFAULT), 
      udpSocket(-1),
      tcpSocket(-1), 
      nT(0), 
      udpRes(nullptr), 
      tcpRes(nullptr) {
    
    setupDirectory();
    parseArguments(argc, argv);
    setupUDPSocket();
    handleCommands();
    closeUDPSocket();
}

void GameClient::setupDirectory() {
    // Create Game_History directory if it doesn't exist
    if (mkdir("Client/Game_History", 0777) == -1) {
        if (errno != EEXIST) {
            perror("Error creating Game_History directory");
            exit(EXIT_FAILURE);
        }
    }

    // Create TOP_SCORE directory if it doesn't exist
    if (mkdir("Client/Top_Scores", 0777) == -1) {
        if (errno != EEXIST) {
            perror("Error creating Top_Scores directory");
            exit(EXIT_FAILURE);
        }
    }
}
void GameClient::parseArguments(int argc, char** argv) {
    if (argc > 1) {
        serverIP = argv[2];
        serverPort = atoi(argv[4]);
    }
    fprintf(stdout, "Server IP: %s\n", serverIP.c_str());
    fprintf(stdout, "Server Port: %d\n", serverPort);
}

void GameClient::setupUDPSocket() {
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    timeout.tv_sec = 5;  // 5 second timeout
    timeout.tv_usec = 0;

    if (udpSocket == -1) {
        std::cerr << "Error creating UDP socket.\n";
        exit(EXIT_FAILURE);
    }
    if (setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "setsockopt failed\n";
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    std::string portStr = std::to_string(serverPort);
    int errcode = getaddrinfo(serverIP.c_str(), portStr.c_str(), &hints, &udpRes);
    if (errcode != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(errcode) << "\n";
        exit(EXIT_FAILURE);
    }
}

void GameClient::closeUDPSocket() {
    freeaddrinfo(udpRes);
    close(udpSocket);
}


void GameClient::setupTCPSocket() {
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket == -1) {
        std::cerr << "Error creating TCP socket.\n";
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(serverPort);
    int errcode = getaddrinfo(serverIP.c_str(), portStr.c_str(), &hints, &tcpRes);
    if (errcode != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(errcode) << "\n";
        exit(EXIT_FAILURE);
    }

    if (connect(tcpSocket, tcpRes->ai_addr, tcpRes->ai_addrlen) < 0) {
        std::cerr << "Error connecting to server.\n";
        close(tcpSocket);
        exit(EXIT_FAILURE);
    }
}

void GameClient::closeTCPSocket() {
    freeaddrinfo(tcpRes);
    close(tcpSocket);
}

int GameClient::handleResponse(const string response) { 
    char status[32], subStatus[32];
    sscanf(response.c_str(), "%s %s", status, subStatus);

    if (strcmp(status, RESPONSE_START) == 0) {
        if (strcmp(subStatus, STATUS_OK) == 0) {
            char C1[8], C2[8], C3[8], C4[8];
            sscanf(response.c_str(), "%*s %*s %s %s %s %s", C1, C2, C3, C4);
            fprintf(stdout, "Game started successfully.\n");
            return SUCCESS;
        } else if (strcmp(subStatus, STATUS_NOK) == 0) {
            fprintf(stdout, "Failed to start game: Player has an ongoing game.\n");
            return FAIL;
        } else if (strcmp(subStatus, STATUS_ERR) == 0) {
            fprintf(stdout, "Failed to start game: Invalid arguments.\n");
            return FAIL;
        }
    } else if (strcmp(status, RESPONSE_TRY) == 0) {
        if (strcmp(subStatus, STATUS_OK) == 0) {
            int nT, nB, nW;
            char C1[8], C2[8], C3[8], C4[8];
            sscanf(response.c_str(), "%*s %*s %d %d %d %s %s %s %s", &nT, &nB, &nW, C1, C2, C3, C4);
            fprintf(stdout, "Trial %d nB: %d   nW: %d.\n", nT, nB, nW);
            if (nB == 4) {
                fprintf(stdout, "Congratulations! You've guessed the secret key\n");
                return SUCCESS;
            }
        } else if (strcmp(subStatus, DUPLICATE_TRY) == 0) {
            fprintf(stdout, "Duplicated trial.\n");
            return FAIL;
        } else if (strcmp(subStatus, INVALID_TRY) == 0) {
            fprintf(stdout, "Invalid trial number or guess.\n");
            return FAIL;
        } else if (strcmp(subStatus, STATUS_NOK) == 0) {
            fprintf(stdout, "No ongoing game for this player.\n");
            return FAIL;
        } else if (strcmp(subStatus, NO_TRIAL) == 0) {
            char C1[8], C2[8], C3[8], C4[8];
            sscanf(response.c_str(), "%*s %*s %s %s %s %s", C1, C2, C3, C4);
            fprintf(stdout, "No more attempts available. The secret key was: %s %s %s %s\n", C1, C2, C3, C4);
            return FAIL;
        } else if (strcmp(subStatus, MAX_TIME) == 0) {
            char C1[8], C2[8], C3[8], C4[8];
            sscanf(response.c_str(), "%*s %*s %s %s %s %s", C1, C2, C3, C4);
            fprintf(stdout, "Maximum play time exceeded. The secret key was: %s %s %s %s\n", C1, C2, C3, C4);
            return FAIL;
        } else if (strcmp(subStatus, STATUS_ERR) == 0) {
            fprintf(stdout, "Error in trial request.\n");
            return FAIL;
        }
    } else if (strcmp(status, RESPONSE_QUIT) == 0) {
        if (strcmp(subStatus, STATUS_OK) == 0) {
            char C1[8], C2[8], C3[8], C4[8];
            sscanf(response.c_str(), "%*s %*s %s %s %s %s", C1, C2, C3, C4);
            fprintf(stdout, "Game terminated. The secret key was: %s %s %s %s\n", C1, C2, C3, C4);
            return SUCCESS;
        } else if (strcmp(subStatus, STATUS_NOK) == 0) {
            fprintf(stdout, "No ongoing game to terminate.\n");
            return FAIL;
        } else if (strcmp(subStatus, STATUS_ERR) == 0) {
            fprintf(stdout, "Error in quit request.\n");
            return FAIL;
        }
    } else if (strcmp(status, RESPONSE_DEBUG) == 0) {
        if (strcmp(subStatus, STATUS_OK) == 0) {
            fprintf(stdout, "Debug game started successfully.\n");
            return SUCCESS;
        } else if (strcmp(subStatus, STATUS_NOK) == 0) {
            fprintf(stdout, "Failed to start debug game: Player has an ongoing game.\n");
            return FAIL;
        } else if (strcmp(subStatus, STATUS_ERR) == 0) {
            fprintf(stdout, "Failed to start debug game: Invalid request.\n");
            return FAIL;
        }
    } else if (strcmp(status, RESPONSE_SHOW_TRIALS) == 0) {
        if (strcmp(subStatus, ACCEPT) == 0 || strcmp(subStatus, FINISH) == 0) {
            char Fname[128], FsizeStr[32];
            int Fsize;
            if (sscanf(response.c_str(), "%*s %*s %s %s", Fname, FsizeStr) != 2) {
                std::cerr << "Error: Malformed response - couldn't parse filename and size\n";
                return FAIL;
            }
            Fsize = atoi(FsizeStr);
            if (Fsize <= 0) {
                std::cerr << "Error: Invalid file size\n";
                return FAIL;
            }
            std::string Fdata(response.substr(response.find(FsizeStr) + strlen(FsizeStr) + 1));
            if (Fdata.empty()) {
                std::cerr << "Error: No file data found\n";
                return FAIL;
            }
            std::string FnamePath = std::string("Client/Game_History/") + Fname; 
            std::ofstream outFile(FnamePath);
            if (!outFile) {
                std::cerr << "Error: Cannot create output file\n";
                return FAIL;
            }
            outFile << Fdata;
            outFile.close();
            fprintf(stdout, "Received file: %s (%d bytes)\n", Fname, Fsize);
            fprintf(stdout, "%s", Fdata.c_str());
            return SUCCESS;
        } else if (strcmp(subStatus, STATUS_NOK) == 0) {
            fprintf(stdout, "No ongoing or finished game for this player.\n");
            return FAIL;
        } else {
            fprintf(stdout, "Unexpected response: %s", response.c_str());
            return FAIL;
        }
    } else if (strcmp(status, RESPONSE_SCOREBOARD) == 0) {
        if (strcmp(subStatus, STATUS_OK) == 0) {
            char Fname[128], FsizeStr[32];
            int Fsize;
            if (sscanf(response.c_str(), "%*s %*s %s %s", Fname, FsizeStr) != 2) {
                std::cerr << "Error: Malformed response - couldn't parse filename and size\n";
                return FAIL;
            }
            Fsize = atoi(FsizeStr);
            if (Fsize <= 0) {
                std::cerr << "Error: Invalid file size\n";
                return FAIL;
            }
            std::string Fdata(response.substr(response.find(FsizeStr) + strlen(FsizeStr) + 1));
            if (Fdata.empty()) {
                std::cerr << "Error: No file data found\n";
                return FAIL;
            }
            std::string FnamePath = std::string("Client/Top_Scores/") + Fname;
            std::ofstream outFile(FnamePath);
            if (!outFile) {
                std::cerr << "Error: Cannot create output file\n";
                return FAIL;
            }
            outFile << Fdata;
            outFile.close();
            fprintf(stdout, "Received scoreboard file: %s (%d bytes)\n\n", Fname, Fsize);
            fprintf(stdout, "%s", Fdata.c_str());
            return SUCCESS;
        } else if (strcmp(subStatus, "EMPTY") == 0) {
            fprintf(stdout, "Scoreboard is empty.\n");
            return FAIL;
        } else {
            fprintf(stdout, "Unexpected response: %s\n", response.c_str());
            return FAIL;
        }
    } else {
        fprintf(stdout, "Unexpected response(out of the protocol): %s\n", response.c_str());
        return FAIL;
    }
    return SUCCESS;
}

void GameClient::handleStartGame(const string& command) {
    char cmd[32];
    int maxPlayTime;
    char pid[32];
    sscanf(command.c_str(), "%s %s %d", cmd, pid, &maxPlayTime);

    char sngCommand[128];
    snprintf(sngCommand, sizeof(sngCommand), "SNG %s %d\n", pid, maxPlayTime);
    sendUDPMessage(udpSocket, sngCommand, (struct sockaddr_in*)udpRes->ai_addr, udpRes->ai_addrlen);
    std::string response = receiveUDPMessage(udpSocket, (struct sockaddr_in*)udpRes->ai_addr, &udpRes->ai_addrlen);
    if(handleResponse(response) == SUCCESS) {
        fprintf(stdout, "New game started (max %d sec)\n", maxPlayTime);
        nT = 0;
        plid = pid;
    }
}

void GameClient::handleTry(const string& command) {
    char cmd[32], colors[128];
    sscanf(command.c_str(), "%s %[^\n]", cmd, colors);

    char tryCommand[256];
    snprintf(tryCommand, sizeof(tryCommand), "TRY %s %s %d\n", plid.c_str(), colors, ++nT);
    sendUDPMessage(udpSocket, tryCommand, (struct sockaddr_in*)udpRes->ai_addr, udpRes->ai_addrlen);
    std::string response = receiveUDPMessage(udpSocket, (struct sockaddr_in*)udpRes->ai_addr, &udpRes->ai_addrlen);
    if(handleResponse(response) == FAIL)
        nT--;
}

void GameClient::handleShowTrials() {
    // Check player ID
    if (plid.empty()) {
        std::cout << "Player ID not set.\n";
        return;
    }

    setupTCPSocket();

    std::string strCommand = "STR " + plid + "\n";
    sendTCPMessage(tcpSocket, strCommand);

    std::string response = receiveTCPMessage(tcpSocket);

    handleResponse(response);
    closeTCPSocket();
}

void GameClient::handleScoreboard() {
    setupTCPSocket();

    std::string ssbCommand = "SSB\n";
    sendTCPMessage(tcpSocket, ssbCommand);

    std::string response = receiveTCPMessage(tcpSocket);

    handleResponse(response);
    closeTCPSocket();
}

void GameClient::handleQuitExit() {
    std::string qutCommand;
    if(!plid.empty()) {
        qutCommand = "QUT " + plid + "\n";
    } else {
        qutCommand = "QUT\n";
    }
    sendUDPMessage(udpSocket, qutCommand, (struct sockaddr_in*)udpRes->ai_addr, udpRes->ai_addrlen);
    std::string response = receiveUDPMessage(udpSocket, (struct sockaddr_in*)udpRes->ai_addr, &udpRes->ai_addrlen);
    handleResponse(response);
}

void GameClient::handleDebug(const std::string& command) {
    char cmd[32], C1[8], C2[8], C3[8], C4[8];
    int maxPlayTime;
    char pid[32];
    sscanf(command.c_str(), "%s %s %d %s %s %s %s", cmd, pid, &maxPlayTime, C1, C2, C3, C4);
    

    char dbgCommand[256];
    snprintf(dbgCommand, sizeof(dbgCommand), "DBG %s %d %s %s %s %s\n", pid, maxPlayTime, C1, C2, C3, C4);
    sendUDPMessage(udpSocket, dbgCommand, (struct sockaddr_in*)udpRes->ai_addr, udpRes->ai_addrlen);
    std::string response = receiveUDPMessage(udpSocket, (struct sockaddr_in*)udpRes->ai_addr, &udpRes->ai_addrlen);
    if(handleResponse(response) == SUCCESS) {
        fprintf(stdout, "New game started (max %d sec) and secret key %s %s %s %s\n", maxPlayTime, C1, C2, C3, C4);
        plid = pid;
        nT = 0;
    }
}

bool GameClient::checkInputFormat(const std::string& command, int n) {
    const char* ptr = command.c_str();
    int spaces = 0;
    while (*ptr) {
        if (*ptr == ' ') spaces++;
        ptr++;
    }
    if (spaces != n - 1) {
        fprintf(stderr, "Error: parameters number does not correspond\n");
        return false;
    }
    return true;
}

void GameClient::handleCommands() {
    char command[256];

    while (true) {
        fprintf(stdout, ">");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        // Remove trailing newline
        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "start", 5) == 0) {
            if (checkInputFormat(command, 3) == false) continue;
            handleStartGame(command);
        } else if (strncmp(command, "try", 3) == 0) {
            if (checkInputFormat(command, 5) == false) continue;
            handleTry(command);
        } else if (strcmp(command, "show_trials") == 0 || strcmp(command, "st") == 0) {
            if (checkInputFormat(command, 1) == false) continue;
            handleShowTrials();
        } else if (strcmp(command, "scoreboard") == 0 || strcmp(command, "sb") == 0) {
            if (checkInputFormat(command, 1) == false) continue;
            handleScoreboard();
        } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            if (checkInputFormat(command, 1) == false) continue;
            handleQuitExit();
            if (strcmp(command, "quit") == 0) {
                continue;
            } else {
                break;
            }
        } else if (strncmp(command, "debug", 5) == 0) {
            if (checkInputFormat(command, 7) == false) continue;
            handleDebug(command);
        } else {
            fprintf(stdout, "Unknown command.\n");
        }
    }
}

// Main function
int main(int argc, char* argv[]) {
    GameClient client(argc, argv);
    return 0;
}