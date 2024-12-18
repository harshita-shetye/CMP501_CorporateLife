#ifndef CLIENT_H
#define CLIENT_H

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <deque>
#include <random>
#include <thread>

using namespace sf;
using namespace std;

// Enums for game state
enum class GameState {
	MainMenu,
	Playing,
	WaitingForPlayer,
	LobbyFull
};

struct Player {
	int id;
	CircleShape shape;
	Vector2f position;
	string name;
	int score;
	long long lastReceivedTimestamp = -1;
	Clock lastReceivedUpdate;
};

class Client {
private:

	// Constants
	string SERVER;
	const unsigned short PORT = 5555;

	const float WINDOW_WIDTH = 1700.f;
	const float WINDOW_HEIGHT = 900.f;

	const float CIRCLE_RADIUS = 15.f;
	const float CIRCLE_BORDER = 2.f;
	const float RAINBOW_RADIUS = 7.f;

	// SFML objects
	RenderWindow* window;
	TcpSocket socket;

	// Game state
	Clock ticker;
	int playerID;
	unordered_map<int, Player> playerData;
	GameState currentState;

	vector<Vector2f> rainbowPositions;
	vector<Color> rainbowColors;
	bool hasRainbowBall;

	// UI-related
	string playerName;

	// Circle shapes
	CircleShape actualPlayerShape;
	CircleShape predictedPlayerShape;
	CircleShape otherPlayerShape;


	bool connectToServer();

	// Private helper methods
	void createMainMenu();
	RectangleShape createButton(float width, float height, const Color& color, float x, float y);
	Text createText(const string& content, const Font& font, unsigned int size, const Color& color, float x, float y);
	void displayWaitingMessage();
	void handleWaitingState();

	void startGame();
	void receiveInitialPosition();

	void gameLoop();
	Vector2f applyInterpolation(Vector2f start, Vector2f end, float speed);
	void sendPlayerPosition(Vector2f movementVector);
	void receivePlayerPositions(Packet packet);
	void receiveRainbowData(Packet packet);
	void deleteRainbowData();
	void updateScores(Packet& packet);
	void displayScores(Font& font);
	void renderActualSelf(float x, float y);
	void renderReceivedShapes();
	void drawRainbowBalls();
	void renderPredictedSelf(float x, float y);

	void handleErrors(Socket::Status status);

public:
	// Constructor and Destructor
	Client();
	~Client();

	// Main entry point for the client
	void run();
};

#endif