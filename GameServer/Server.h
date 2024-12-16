#ifndef SERVER_H
#define SERVER_H

#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <memory>
#include <string>
#include <cstdlib>

using namespace std;
using namespace sf;
using ClockType = chrono::steady_clock;

// Constants
constexpr unsigned short PORT = 5555;
constexpr float CIRCLE_RADIUS = 15.f;
constexpr float WINDOW_WIDTH = 1700.f;
constexpr float WINDOW_HEIGHT = 900.f;

static Clock ticker;

// Struct to store client data
struct ClientData {
	unique_ptr<TcpSocket> socket;
	Vector2f position;
	int ID;
	int score = 0;
	string playerName;

	// For prediction
	Vector2f velocity;
	Vector2f acceleration;
	Clock lastUpdateTime;
	Clock lastReceivedUpdate;
};

// Server class
class Server {
public:
	// Constructor and Destructor
	Server(unsigned short port = PORT);
	~Server();

	void run();

private:
	TcpListener listener;
	SocketSelector selector;
	vector<ClientData> clientData;

	bool running = true;
	bool bothPlayersConnected = false;
	bool sent = false;
	bool hasRainbowBall = false;
	pair<Vector2f, Color> rainbowBall;
	ClockType::time_point spawnTime;
	ClockType::time_point lastSpawnAttempt = ClockType::now();
	float smoothingFactor = 0.8f; // Apply a smoothing factor when updating velocities to reduce sudden changes caused by small inaccuracies or lag.
	float dampingFactor = 0.95f; // Apply a damping factor to slow down abrupt velocity changes.

	// Server methods
	void processNewClient();
	void sendFullLobbyMessage(TcpListener& listener);
	void notifyClientsOnConnection();
	void sendPlayerId();

	void processClientData(TcpSocket& client, size_t clientIndex);
	bool isPlayerTouchingRainbowBall(ClientData& player) const;

	void handleDisconnection(size_t index);
	void trySpawnRainbowBall();
	void spawnRainbowBall();
	void checkRainbowBallTimeout();
	void despawnRainbowBall();
	void broadcastUpdatedScores();
	void broadcastToClients(Packet& packet);
	void sendPlayerPositions();
	void handleErrors(Socket::Status status);
};

#endif