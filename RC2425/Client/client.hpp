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


// Global variables
extern std::string serverIP;
extern int serverPort;
extern std::string plid;
extern int udpSocket, tcpSocket;
extern int nT;
extern struct addrinfo hints, *udpRes, *tcpRes;


void parseArguments(int argc, char* argv[]);
void setupUDPSocket();
void closeUDPSocket();

//Communication
void sendUDPMessage(const std::string& message);
std::string receiveUDPMessage();
void setupTCPSocket();
void sendTCPMessage(const std::string& message);
std::string receiveTCPMessage();
void closeTCPSocket();
int handleResponse(std::string response);

//Commands
void handleStartGame(const std::string& command);
void handleTry(const std::string& command);
void handleShowTrials();
void handleScoreboard();
int handleQuitExit();
void handleDebug(const std::string& command);
void handleCommands(int udpSocket, struct addrinfo* udpRes, int tcpSocket, const std::string& serverIP, int serverPort);

#endif