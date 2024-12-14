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

// Global variables
extern TcpListener listener;
extern SocketSelector selector;
extern pair<Vector2f, Color> rainbowBall;  // Stores the rainbow ball position and color
extern ClockType::time_point spawnTime;
extern ClockType::time_point lastSpawnAttempt;
extern bool running;
extern bool hasRainbowBall;               // Flag indicating if a rainbow ball exists

// Function declarations
void SetupServer(unsigned short port);
void processNewClient(TcpListener& listener, SocketSelector& selector);
void sendFullLobbyMessage(TcpListener& listener);
void notifyClientsOnConnection();

void processClientData(TcpSocket& client, size_t clientIndex);
void handleDisconnection(size_t index);

void trySpawnRainbowBall();
void spawnRainbowBall();
void checkRainbowBallTimeout();
void despawnRainbowBall();
void broadcastUpdatedScores();
void broadcastToClients(Packet& packet);

void sendPlayerPositions();

void handleErrors(Socket::Status status);


#endif // SERVER_H