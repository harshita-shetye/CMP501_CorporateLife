#ifndef SERVER_H
#define SERVER_H

#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>

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
extern vector<unique_ptr<TcpSocket>> clients;
extern pair<Vector2f, Color> rainbowBall;  // Stores the rainbow ball position and color
extern ClockType::time_point spawnTime;
extern ClockType::time_point lastSpawnAttempt;
extern bool running;
extern bool hasRainbowBall;               // Flag indicating if a rainbow ball exists

// Function declarations
void SetupServer(unsigned short port);
void processNewClient(TcpListener& listener, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients);
void sendFullLobbyMessage(TcpListener& listener);
void  notifyClientsOnConnection();


void processClientData(TcpSocket& client, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients, size_t clientIndex);
void handleDisconnection(TcpSocket& client, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients, size_t index);



void trySpawnRainbowBall();
void spawnRainbowBall();
void checkRainbowBallTimeout();
void despawnRainbowBall(vector<unique_ptr<TcpSocket>>& clients);
void broadcastToClients(Packet& packet);


void sendPlayerPositions(vector<unique_ptr<TcpSocket>>& clients);
Vector2f getPlayerPosition(int playerIndex);


void handleErrors(Socket::Status status);







//void loadRainbowBall(vector<unique_ptr<TcpSocket>>& clients);
//void despawnRainbowBall(vector<unique_ptr<TcpSocket>>& clients);
//void handlePlayerPosition(TcpSocket& playerSocket);
//void updatePlayerPosition(TcpSocket& playerSocket, float x, float y);



#endif // SERVER_H