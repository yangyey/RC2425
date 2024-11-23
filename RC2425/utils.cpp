#include "utils.hpp"
#include "constant.hpp"

namespace protocols {
    int sendTCPMessage(int sock, const std::string& message) {
        size_t total = 0;
        size_t len = message.length();
        
        while (total < len) {
            ssize_t sent = write(sock, message.c_str() + total, len - total);
            if (sent == -1) {
                return FAIL;
            }
            total += sent;
        }
        return SUCCESS;
    }

    std::string receiveTCPMessage(int sock) {
        char buffer[BUFFER_SIZE];
        std::string message;
        
        while (true) {
            ssize_t received = read(sock, buffer, sizeof(buffer) - 1);
            if (received <= 0) {
                std::cerr << "Failed to receive TCP message.\n";
                close(sock);
                exit(1);
            }
            if (received == 0) break;

            buffer[received] = '\0';
            message += std::string(buffer, received);
            
            if (!message.empty() && message.back() == '\n') break;
        }
        return message;
    }

    // Implement UDP send/receive similarly...
    void sendUDPMessage(int sock, const std::string& message, struct addrinfo* udpRes) {
        ssize_t n = sendto(sock, message.c_str(), message.size(), 0, udpRes->ai_addr, udpRes->ai_addrlen);
        if (n == -1) {
            std::cerr << "Failed to send UDP message.\n";
            exit(1);
        }
    }

    std::string receiveUDPMessage(int sock, struct addrinfo* udpRes) {
        char buffer[BUFFER_SIZE];
        socklen_t addrlen = sizeof(udpRes->ai_addr);
        ssize_t n = -1;
        int retryCount = 0;

        // Retry up to 3 times
        while (retryCount < 3) {
            n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                        udpRes->ai_addr, &addrlen);

            if (n == -1) {
                retryCount++;
                // std::this_thread::sleep_for(std::chrono::seconds(1));
                sleep(1);
                continue; // Retry on failure
            }
            // If received successfully, break out of the loop
            break;
        }
        buffer[n] = '\0';
        return std::string(buffer);
    }
}