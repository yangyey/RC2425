#ifndef __H_UTILS
#define __H_UTILS

#include <vector>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <cstring>
#include <string>
#include <fstream>
#include <cstdio>
#include <sys/stat.h>

namespace protocols {

    // Common send/receive functions
    int sendTCPMessage(int sock, const std::string& message);
    std::string receiveTCPMessage(int sock);
    void sendUDPMessage(int sock, const std::string& message, struct addrinfo* udpRes);
    std::string receiveUDPMessage(int sock, struct addrinfo* udpRes);
    std::string receiveUDPMessage(int sockfd, struct sockaddr_in* client_addr, socklen_t* addrlen);

    // Response status codes
    const std::string OK = "OK";
    const std::string NOK = "NOK";
    const std::string ERR = "ERR";
    const std::string DUP = "DUP";
    const std::string INV = "INV";
    const std::string ENT = "ENT";
    const std::string ETM = "ETM";
    const std::string ACT = "ACT";
    const std::string FIN = "FIN";
    const std::string EMPTY = "EMPTY";
}

#endif