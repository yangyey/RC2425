#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <netdb.h>
#include <ctime>

class Game {
private:
    std::string plid;
    std::string secretKey;
    std::vector<std::string> trials;
    time_t startTime;
    int maxTime;
    bool active;

public:
    Game(const std::string& pid, int maxPlayTime);
    void generateSecretKey();
    bool isTimeExceeded();
    const std::string& getSecretKey() const { return secretKey; }
    void addTrial(const std::string& trial) { trials.push_back(trial); }
    const std::vector<std::string>& getTrials() const { return trials; }
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }
    int getTrialCount() const { return trials.size(); }
};

class Server {
private:
    fd_set inputs, testfds;
    struct timeval timeout;
    int max_fd;
    struct addrinfo hints, *res;
    int ufd, tfd;
    bool running;
    std::map<std::string, Game> activeGames;
    std::vector<std::pair<std::string, int>> scoreboard;

    void setupSockets(int port);
    void handleConnections();
    void receiveUDPMessage();
    void receiveTCPMessage();
    std::string handleRequest(const std::string& request, bool isTCP);
    // std::string handleStartGame(std::istringstream& iss);
    // std::string handleTry(std::istringstream& iss);
    // std::string handleQuit(std::istringstream& iss);
    // std::string handleDebug(std::istringstream& iss);
    // std::string handleShowTrials(std::istringstream& iss);
    // std::string handleScoreboard();

public:
    Server(int port);
    ~Server() = default;
    void run();
};
