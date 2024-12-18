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
#include <dirent.h>
#include <cstdlib>
#include <algorithm>
#include <random>

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
    void saveScoreFile() const;
    int calculateScore() const;

public:
    // Constructor
    Game(const std::string& pid, int maxPlayTime, char mode = 'P');


    // Methods engaging with file system
    std::string getGameFilePath() const { return "Server/GAMES/GAME_" + plid + ".txt"; };
    std::string formatTrialFileName() const { return "STATE_" + plid + ".txt"; };
    void saveInitialState() const;
    void appendTrialToFile(const std::string& trial, int nB, int nW) const;
    void finalizeGame(char endCode);


    // Methods engaging with game state
    void generateSecretKey();
    bool isTimeExceeded() { return (time(nullptr) - startTime) > maxTime; };
    bool isActive() const { return active; }
    void setActive(bool status) { active = status; }
    void setSecretKey(const std::string& newKey) { secretKey = newKey; }
    void addTrial(const std::string& trial) { trials.push_back(trial); }
    const std::string& getSecretKey() const { return secretKey; }
    const std::vector<std::string>& getTrials() const { return trials; }
    int getTrialCount() const { return trials.size(); }
    int getMaxTime() const { return maxTime; }
    time_t getStartTime() const { return startTime; }
};

class Server {
private:    
    typedef struct {
        int n_scores;
        int score[10];
        char PLID[10][7];
        char col_code[10][5];
        int no_tries[10];
        char mode[10][6]; 
    } SCORELIST;

    const int MAX_ATTEMPTS = 8;
    fd_set inputs, testfds;
    struct timeval timeout;
    int max_fd;
    struct addrinfo hints, *res;
    int ufd, tfd;
    const std::unordered_set<std::string> VALID_COLORS = {"B", "G", "Y", "R", "P", "O"};
    std::map<std::string, Game> activeGames;
    // std::vector<std::pair<std::string, int>> scoreboard;
    bool verbose;  // Add this member variable

    void setupDirectory();
    void setupSockets(int port);
    std::string handleRequest(const std::string& request, bool isTCP, 
                                const struct sockaddr_in* client_addr);
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
    std::string handleShowTrials(const std::string& request);

    std::string formatGameHeader(const std::string& plid, const std::string& date, 
                               const std::string& time, int maxTime);
    std::string formatTrials(const std::vector<std::string>& lines);
    std::string formatRemainingTime(int remainingTime);
    std::string processActiveGame(const std::string& plid, Game& game);
    std::string processFinishedGame(const std::string& plid);
    std::vector<std::string> readGameFile(const std::string& filePath);
    int FindLastGame(const char* PLID, char* fname);
    std::string handleScoreBoard();
    int FindTopScores(SCORELIST* list);
    bool hasActiveTry(const std::string& plid);
    std::string formatClientInfo(const struct sockaddr_in* client_addr);

public:
    Server(int port, bool verboseMode = false);  // Modify constructor
    ~Server() = default;
    void run();
};