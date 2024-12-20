# Projeto RC



The goal of this project is to develop a simple version of the Master Mind game.
The development of the project requires implementing a Game Server (GS) and a 
Player Application (Player). The GS and multiple Player application instances are 
intended to operate simultaneously on different machines connected to the Internet.
The GS will be running on a machine with known IP address and ports.

## How to compile program

Open RC2425 folder and compile with the command "make".
"make clean" cleans all the files and folders that are created throughout the program.

### Run GS

To run the server use the command *./GS* with two possible flags:

- "-v" to activate verbose
- "-p __port__" to set a custom port for the server. Default port: **58030**

### Run Player

To run the player use the command "./player" with two possible flags:

- "-n" to set a custom target server IP. Default target IP: **localhost**

- "-p" to set a custom port. Default port: **58030**

## File organization

**RC2425** contains auxiliary functions for the project

**RC2425/Server** server functionality of the project

**RC2425/Client** player functionality of the project

### RC2425

#### constant.hpp

File with the definitions of constants.

#### utils.cpp

File containing all the functions for communication protocols used between Server and Client.

#### utils.hpp

Header file of utils.cpp

### RC2425/Client

#### client.cpp

Main file of the player functionality.

Deals with initialization, termination and parsing the arguments used when invoking 
the user application. Handles, constructs and sends all requests to the Game Server (GS).
Displays all the information received from the server.

#### client.hpp

Header file of client.cpp.

### RC2425/Server

#### server.cpp

Main file of the server functionality.

Deals with initialization, termination and parsing the arguments used when invoking 
the Game Server (GS). Handles the requests received by the player and sends the information back to it so it can be displayed.


#### server.hpp

Header file of server.cpp.

#### Presistence Information storing system

**RC2425**

&emsp;|-> **Client**

&emsp;&emsp;|-> **Game_History**

&emsp;&emsp;&emsp;|-> **STATE_XXXXXX.txt** *file storing the information relative to a game*

&emsp;&emsp;|-> **Top_Scores**

&emsp;&emsp;&emsp;|-> **Top_10_SCORES_X.txt** *file containing the top 10 scores*

&emsp;|-> **Server**

&emsp;&emsp;|-> **GAMES**

&emsp;&emsp;&emsp;|-> **GAME_UID.txt** *file storing ongoing game* 

&emsp;&emsp;&emsp;|-> **UID**

&emsp;&emsp;&emsp;&emsp;|-> **DATE_XXXXXX_X** *file storing a player's past games*

&emsp;&emsp;|-> **SCORES**

&emsp;&emsp;&emsp;|-> **SCORE_UID_DATE** *file storing a game's score*


## Authors

Sun Chenwei 106469

Pedro Yang  106585
