#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <netdb.h>
#include <ctime>
#include <unordered_set>
#include <fstream>
#include <iomanip>
#include <unistd.h>

class Game {
private:
    std::string plid;
    std::string secretKey;
    std::vector<std::string> trials;
    time_t startTime;
    int maxTime;
    bool active;
    char gameMode; // 'P' for play, 'D' for debug

    // Private methods
    // std::string getGameFilePath() const;
    // void saveInitialState() const;
    // void appendTrialToFile(const std::string& trial, int nB, int nW) const;

public:
    // Constructor
    Game(const std::string& pid, int maxPlayTime, char mode = 'P');


    // Methods engaging with file system
    std::string getGameFilePath() const;
    void saveInitialState() const;
    void appendTrialToFile(const std::string& trial, int nB, int nW) const;
    void finalizeGame(char endCode);


    // Methods engaging with game state
    void generateSecretKey();
    bool isTimeExceeded();
    bool isActive() const { return active; }
    void setActive(bool status) { active = status; }
    void setSecretKey(const std::string& newKey) { secretKey = newKey; }
    void addTrial(const std::string& trial) { trials.push_back(trial); }
    const std::string& getSecretKey() const { return secretKey; }
    const std::vector<std::string>& getTrials() const { return trials; }
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
    // std::vector<std::pair<std::string, int>> scoreboard;

    void setupDirectory();
    void setupSockets(int port);
    void handleConnections();
    std::string handleRequest(const std::string& request, bool isTCP);
    std::string handleStartGame(const std::string& request);
    std::string handleTry(const std::string& request);
    void countMatches(const std::string& c1, const std::string& c2,
                 const std::string& c3, const std::string& c4,
                 const std::string& secret,
                 int& nB, int& nW);
    bool isValidColor(const std::string& color);
    bool isValidPlid(const std::string& plid);
    std::string formatColors(const std::string& c1, const std::string& c2, 
                           const std::string& c3, const std::string& c4);
    std::string handleQuitExit(const std::string& request);
    std::string handleDebug(const std::string& request);
    // std::string handleShowTrials(const std::string& request);
    // std::string handleScoreboard();

public:
    Server(int port);
    ~Server() = default;
    void run();
};
