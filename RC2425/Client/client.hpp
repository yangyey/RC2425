#ifndef __H_CLIENT
#define __H_CLIENT

#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <thread>
#include <chrono>

class GameClient {
private:
    std::string serverIP;
    int serverPort;
    int udpSocket;
    int tcpSocket;
    int nT;  // Trial number
    std::string plid;
    struct addrinfo hints;
    struct addrinfo *udpRes;
    struct addrinfo *tcpRes;
    struct timeval timeout;

public:
    GameClient(int argc, char** argv);
    ~GameClient() = default;

private:
    // Initialization/Termination
    void setupDirectory();
    void parseArguments(int argc, char** argv);
    void setupUDPSocket();
    void closeUDPSocket();
    void handleCommands();
    
    // Communication
    // void sendUDPMessage(const std::string& message);
    // std::string receiveUDPMessage();
    void setupTCPSocket();
    // void sendTCPMessage(const std::string& message);
    // std::string receiveTCPMessage();
    void closeTCPSocket();
    int handleResponse(const std::string response);

    // Command handlers
    void handleStartGame(const std::string& command);
    void handleTry(const std::string& command);
    void handleShowTrials();
    void handleScoreboard();
    void handleQuitExit();
    void handleDebug(const std::string& command);
};

#endif