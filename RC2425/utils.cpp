#include "utils.hpp"
#include "constant.hpp"

namespace protocols {
    void sendTCPMessage(int sock, const std::string& message) {
        size_t total = 0;
        size_t len = message.length();
        ssize_t sent;
        
        while (total < len) {
            sent = write(sock, message.c_str() + total, len - total);
            
            if (sent < 0) {
                if (errno == EINTR) {
                    continue;  
                }
                std::cerr << "Failed to send TCP message.\n";
                return;
            }
            
            if (sent == 0) {
                std::cerr << "Failed to send TCP message(Connection closed).\n";
                return;
            }
            
            total += sent;
        }
    }

    std::string receiveTCPMessage(int sock) {
        char buffer[BUFFER_SIZE];
        std::string message;
        ssize_t received;
        
        while (true) {
            received = read(sock, buffer, sizeof(buffer) - 1);
            
            if (received < 0) {
                if (errno == EINTR) {
                    continue;  
                }
                std::cerr << "Failed to send TCP message.\n";
            }
            
            if (received == 0) {
                if (message.empty()) {
                    std::cerr << "Failed to send TCP message(Connection closed).\n";
                }
                break;  
            }
            
            buffer[received] = '\0';
            message += std::string(buffer, received);
            
            // Check for message completion
            size_t newline_pos = message.find('\n');
            if (newline_pos != std::string::npos) {
                break;
            }
        }
        
        return message;
    }

    void sendUDPMessage(int sock, const std::string& message, struct sockaddr_in* client_addr, socklen_t addrlen) {
        ssize_t n = sendto(sock, message.c_str(), message.size(), 0, (struct sockaddr*)client_addr, addrlen);
        if (n == -1) {
            std::cerr << "Failed to send UDP message.\n";
            exit(1);
        }
    }


    std::string receiveUDPMessage(int sock, struct sockaddr_in* client_addr, socklen_t* addrlen) {
        char buffer[BUFFER_SIZE];
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr*)client_addr, addrlen);

        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "Timeout, please retry\n";
            } else {
                std::cerr << "Error receiving message: " << strerror(errno) << "\n";
            }
            return "";
        }

        if (n >= 0 && n < BUFFER_SIZE) {
            buffer[n] = '\0';
            return std::string(buffer);
        }
        return "";
    }
}