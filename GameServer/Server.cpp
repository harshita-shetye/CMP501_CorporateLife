#include "Server.h"

// Global variables
TcpListener listener;
SocketSelector selector;
vector<unique_ptr<TcpSocket>> clients;
vector<Vector2f> playerPositions(2);
pair<Vector2f, Color> rainbowBall;
ClockType::time_point spawnTime;
ClockType::time_point lastSpawnAttempt = ClockType::now();
static Clock ticker;
bool running = true;
bool hasRainbowBall = false;
bool bothPlayersConnected = false; // Track if both players are connected


void SetupServer(unsigned short port) {
	if (listener.listen(port) != Socket::Done) {
		cerr << "Failed to start the server on port " << port << ".\n";
		return;
	}

	cout << "Server listening on port " << port << ". Awaiting connections...\n";
	selector.add(listener);

	while (running) {
		if (selector.wait(milliseconds(10))) {
			if (selector.isReady(listener)) {
				processNewClient(listener, selector, clients);
			}

			for (size_t i = 0; i < clients.size(); ++i) {
				if (selector.isReady(*clients[i])) {
					processClientData(*clients[i], selector, clients, i);
				}
			}
		}

		if (bothPlayersConnected) {
			if (ticker.getElapsedTime().asMilliseconds() > 20) {
				sendPlayerPositions(clients);
				ticker.restart();
			}
			if (!hasRainbowBall) trySpawnRainbowBall();
			else checkRainbowBallTimeout();
		}
	}

	cout << "Shutting down the server.\n";
}

void processNewClient(TcpListener& listener, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients) {
	if (clients.size() >= 2) {
		sendFullLobbyMessage(listener);
		return;
	}

	auto newClient = make_unique<TcpSocket>();
	if (listener.accept(*newClient) == Socket::Done) {
		cout << "New client connected: " << newClient->getRemoteAddress() << "\n";
		selector.add(*newClient);
		clients.push_back(std::move(newClient));
		notifyClientsOnConnection();
	}
}

void sendFullLobbyMessage(TcpListener& listener) {
	auto tempSocket = make_unique<TcpSocket>();
	if (listener.accept(*tempSocket) == Socket::Done) {
		Packet packet;
		packet << "The lobby is full. Try again later.";
		tempSocket->send(packet);
		tempSocket->disconnect();
	}
}

void notifyClientsOnConnection() {
	// If there is only 1 player, inform them they're waiting for another player
	if (clients.size() == 1) {
		// Notify the first player that they're waiting for the second player
		Packet packet;
		packet << "Waiting for another player to connect...";
		clients[0]->send(packet);
	}

	// If both players are connected, notify both to start the game
	if (clients.size() == 2) {
		Packet startPacket;
		startPacket << "Both players connected. Starting the game!";
		for (const auto& client : clients) {
			if (client->send(startPacket) != Socket::Done) {
				cerr << "Failed to send start game packet to a client.\n";
			}
		}
		bothPlayersConnected = true;
	}
}


void processClientData(TcpSocket& client, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients, size_t clientIndex) {
	Packet packet;
	auto status = client.receive(packet);

	if (status == Socket::Done) {
		string command;
		packet >> command;

		if (command == "UPDATE_POSITION") {
			float x, y;
			packet >> x >> y;

			//if (abs(playerPositions[clientIndex].x - previousPositions[clientIndex].x) > threshold ||
			//	abs(playerPositions[clientIndex].y - previousPositions[clientIndex].y) > threshold) {
			//	// Send update
			//}

			playerPositions[clientIndex] = Vector2f(x, y);
		}
		else if (command == "DESPAWN") {
			despawnRainbowBall(clients);
		}
	}
	else if (status == Socket::Disconnected) {
		handleDisconnection(client, selector, clients, clientIndex);
	}
	else {
		handleErrors(status);
	}
}

void handleDisconnection(TcpSocket& client, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients, size_t index) {
	cout << "Client disconnected: " << client.getRemoteAddress() << "\n";
	selector.remove(client);
	clients.erase(clients.begin() + index);
	bothPlayersConnected = (clients.size() == 2);
}


void trySpawnRainbowBall() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - lastSpawnAttempt).count() >= 5) {
		spawnRainbowBall();
		lastSpawnAttempt = ClockType::now();
	}
}

void spawnRainbowBall() {
	Vector2f position(rand() % static_cast<int>(WINDOW_WIDTH - CIRCLE_RADIUS * 2),
		rand() % static_cast<int>(WINDOW_HEIGHT - CIRCLE_RADIUS * 2));
	Color color(rand() % 256, rand() % 256, rand() % 256);
	rainbowBall = { position, color };
	hasRainbowBall = true;
	spawnTime = ClockType::now();

	Packet packet;
	packet << "SPAWN" << position.x << position.y << static_cast<Uint8>(color.r) << static_cast<Uint8>(color.g) << static_cast<Uint8>(color.b);
	broadcastToClients(packet);
}

void checkRainbowBallTimeout() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - spawnTime).count() >= 5) {
		despawnRainbowBall(clients);
	}
}

void despawnRainbowBall(vector<unique_ptr<TcpSocket>>& clients) {
	hasRainbowBall = false;
	Packet packet;
	packet << "DESPAWN";
	broadcastToClients(packet);
}

void broadcastToClients(Packet& packet) {
	for (const auto& client : clients) {
		if (client->send(packet) != Socket::Done) {
			cerr << "Failed to send packet to a client.\n";
		}
	}
}


void sendPlayerPositions(vector<unique_ptr<TcpSocket>>& clients) {
	Packet packet;
	packet << "PLAYER_POSITIONS";

	// Player ID + positions
	for (size_t i = 0; i < playerPositions.size(); ++i) {
		packet << static_cast<int>(i) << playerPositions[i].x << playerPositions[i].y;
	}

	for (const auto& client : clients) {
		if (client->send(packet) != Socket::Done) {
			cerr << "Failed to send player positions to a client.\n";
		}
	}
}


void handleErrors(Socket::Status status) {
	switch (status) {
	case Socket::NotReady:
		cerr << "Socket not ready to send/receive data.\n";
		break;
	case Socket::Partial:
		cerr << "Partial data sent/received.\n";
		break;
	case Socket::Disconnected:
		cerr << "Socket disconnected.\n";
		break;
	case Socket::Error:
	default:
		cerr << "An unexpected socket error occurred.\n";
		break;
	}
}