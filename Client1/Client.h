#ifndef CLIENT_H
#define CLIENT_H

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>

using namespace sf;
using namespace std;

// Constants
const float CIRCLE_RADIUS = 15.f;
const float CIRCLE_BORDER = 2.f;
const float WINDOW_WIDTH = 1700.f;
const float WINDOW_HEIGHT = 900.f;
const float RAINBOW_RADIUS = 7.f;
const unsigned short PORT = 5555;
const string SERVER = "127.0.0.1";

// Structs

// Struct to manage player state and movement
struct PlayerState {
	Vector2f lastPosition;
	Vector2f currentPosition;
	Vector2f targetPosition;
	Vector2f velocity; // For prediction
	float interpolationFactor = 0.1f;
};

// Enum for game state
enum class GameState {
	MainMenu,
	Playing,
	WaitingForPlayer
};

// Struct for representing players in the game
struct Player {
	CircleShape shape;
	Vector2f position;
};

// External variables
extern unordered_map<int, Player> players;
extern int playerID;
extern GameState currentState;
extern bool hasRainbowBall;

// Function declarations

// Main menu creation and management
void createMainMenu(RenderWindow*& window, TcpSocket& socket);
void displayWaitingMessage(RenderWindow& window);
void handleWaitingState(RenderWindow* window, TcpSocket& socket);
void startGame(RenderWindow*& window, TcpSocket& socket);

// TCP Client functions
bool connectToServer(TcpSocket& socket);
bool receiveInitialPosition(TcpSocket& socket, float& x, float& y);

// Player setup and game loop
void setupPlayerShape(CircleShape& playerShape, float x, float y);

void gameLoop(RenderWindow& window, TcpSocket& socket);
void sendPlayerPosition(TcpSocket& socket, const CircleShape& playerShape);
void receivePlayerPositions(TcpSocket& socket, vector<Vector2f>& otherPlayerPositions);
void renderPlayer(RenderWindow& window, float x, float y);
void renderOtherPlayers(RenderWindow& window, const vector<Vector2f>& otherPlayerPositions);


// Network data handling
void receiveRainbowData(TcpSocket& socket, vector<Vector2f>& positions, vector<Color>& colors);
void drawRainbowBalls(RenderWindow& window);

// Error handling

#endif