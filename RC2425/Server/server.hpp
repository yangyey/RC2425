#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <netdb.h>
#include <ctime>
#include <unordered_set>

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
    void setSecretKey(const std::string& newKey) { secretKey = newKey; }
    void addTrial(const std::string& trial) { trials.push_back(trial); }
    const std::vector<std::string>& getTrials() const { return trials; }
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }
    int getTrialCount() const { return trials.size(); }
};

class Server {
private:
    const int MAX_ATTEMPTS = 8;
    fd_set inputs, testfds;
    struct timeval timeout;
    int max_fd;
    struct addrinfo hints, *res;
    int ufd, tfd;
    bool running;
    const std::unordered_set<std::string> VALID_COLORS = {"B", "G", "Y", "R", "P", "O"};
    std::map<std::string, Game> activeGames;
    std::vector<std::pair<std::string, int>> scoreboard;

    void setupSockets(int port);
    void handleConnections();
    // void receiveUDPMessage();
    // void receiveTCPMessage();
    std::string handleRequest(const std::string& request, bool isTCP);
    std::string handleStartGame(std::istringstream& iss);
    std::string handleTry(std::istringstream& iss);
    void countMatches(const std::string& c1, const std::string& c2,
                 const std::string& c3, const std::string& c4,
                 const std::string& secret,
                 int& nB, int& nW);
    bool isValidColor(const std::string& color);
    bool isValidPlid(const std::string& plid);
    std::string formatColors(const std::string& c1, const std::string& c2, 
                           const std::string& c3, const std::string& c4);
    std::string handleQuitExit(std::istringstream& iss);
    std::string handleDebug(std::istringstream& iss);
    // std::string handleShowTrials(std::istringstream& iss);
    // std::string handleScoreboard();

public:
    Server(int port);
    ~Server() = default;
    void run();
};
