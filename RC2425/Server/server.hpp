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
    void saveScoreFile() const;
    int calculateScore() const;

public:
    // Constructor
    Game(const std::string& pid, int maxPlayTime, char mode);


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

    // Getters
    const std::string& getSecretKey() const { return secretKey; }
    const std::vector<std::string>& getTrials() const { return trials; }
    int getTrialCount() const { return trials.size(); }
    int getMaxTime() const { return maxTime; }
    time_t getStartTime() const { return startTime; }
};

class Server {
private:    
    // Types and constants
    typedef struct {
        int n_scores;
        int score[10];
        char PLID[10][7];
        char col_code[10][5];
        int no_tries[10];
        char mode[10][6]; 
    } SCORELIST;

    enum GameFileStatus {
        NO_GAME = -1,          // No game file exists
        ACTIVE_GAME = 0,       // Game exists and is active (not timed out)
        TIMED_OUT_GAME = 1,    // Game exists but has timed out
        ACTIVE_WITH_TRIES = 2  // Game exists, is active, and has tries
    };

    // Member variables
    const std::unordered_set<std::string> VALID_COLORS = {"B", "G", "Y", "R", "P", "O"};
    int sb_count = 1;
    bool verbose;  
    int max_fd;
    int ufd, tfd;

    fd_set inputs, testfds;
    struct timeval timeout;
    struct addrinfo hints, *res;
    std::map<std::string, Game> activeGames;

    // Setup methods
    void setupDirectory();
    void setupSockets(int port);

    // Request handlers
    std::string handleRequest(const std::string& request, bool isTCP, 
                                const struct sockaddr_in* client_addr);
    std::string handleStartGame(const std::string& request);
    std::string handleTry(const std::string& request);
    std::string handleQuitExit(const std::string& request);
    std::string handleDebug(const std::string& request);
    std::string handleShowTrials(const std::string& request);
    std::string handleScoreBoard();

    // Game logic methods
    void countMatches(const std::string& c1, const std::string& c2,
                 const std::string& c3, const std::string& c4,
                 const std::string& secret,
                 int& nB, int& nW);
    bool isValidColor(const std::string& color);
    bool isValidPlid(const std::string& plid);
    GameFileStatus checkGameFile(const std::string& plid) const;

    // Formatting methods
    std::string formatColors(const std::string& c1, const std::string& c2, 
                           const std::string& c3, const std::string& c4);
    std::string formatGameHeader(const std::string& plid, const std::string& date, 
                               const std::string& time, int maxTime);
    std::string formatTrials(const std::vector<std::string>& lines);
    std::string formatRemainingTime(int remainingTime);
    std::string formatClientInfo(const struct sockaddr_in* client_addr);

    // File I/O methods
    std::string processActiveGame(const std::string& plid, Game& game);
    std::string processFinishedGame(const std::string& plid);
    std::vector<std::string> readGameFile(const std::string& filePath);
    std::map<std::string, Game>::iterator loadGameFromFile(const std::string& plid);
    int FindLastGame(const char* PLID, char* fname);
    int FindTopScores(SCORELIST* list);


public:
    Server(int port, bool verboseMode);
    void run();
};