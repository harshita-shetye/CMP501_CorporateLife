#include "Server.h"

struct ClientData {
	unique_ptr<TcpSocket> socket;
	Vector2f position;
	int ID;
};
vector<ClientData> clientData;

// Global variables
TcpListener listener;
SocketSelector selector;
static Clock ticker;
bool running = true;
bool bothPlayersConnected = false;

bool hasRainbowBall = false;
pair<Vector2f, Color> rainbowBall;
ClockType::time_point spawnTime;
ClockType::time_point lastSpawnAttempt = ClockType::now();


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
				processNewClient(listener, selector);
			}

			for (size_t i = 0; i < clientData.size(); ++i) {
				if (selector.isReady(*clientData[i].socket)) {
					processClientData(*clientData[i].socket, i);
				}
			}
		}

		if (bothPlayersConnected) {
			if (ticker.getElapsedTime().asMilliseconds() > 20) {
				sendPlayerPositions();
				ticker.restart();
			}
			if (!hasRainbowBall) trySpawnRainbowBall();
			else checkRainbowBallTimeout();
		}
	}

	cout << "Shutting down the server.\n";
}

void processNewClient(TcpListener& listener, SocketSelector& selector) {
	if (clientData.size() >= 2) {
		sendFullLobbyMessage(listener);
		return;
	}

	auto newClient = make_unique<TcpSocket>();
	if (listener.accept(*newClient) == Socket::Done) {
		selector.add(*newClient);

		ClientData newClientData;
		newClientData.socket = std::move(newClient);
		newClientData.ID = static_cast<int>(clientData.size()); // Assign a unique ID
		newClientData.position = { 0.0f, 0.0f }; // Default position
		clientData.push_back(std::move(newClientData));

		cout << "New Client ID " << newClientData.ID << " connected: " << clientData.back().socket->getRemoteAddress() << "\n";

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
	if (clientData.size() == 1) {
		// Notify the first player that they're waiting for the second player
		Packet packet;
		packet << "Waiting for another player to connect...";
		clientData[0].socket->send(packet);
	}

	// If both players are connected, notify both to start the game
	if (clientData.size() == 2) {
		Packet startPacket;
		startPacket << "Both players connected. Starting the game!";

		for (const auto& client : clientData) {
			if (client.socket->send(startPacket) != Socket::Done) {
				cerr << "Failed to send start game packet to a client.\n";
			}
		}
		bothPlayersConnected = true;
	}
}


void processClientData(TcpSocket& client, size_t clientIndex) {
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

			clientData[clientIndex].position = { x, y };
		}
		else if (command == "DESPAWN") {
			despawnRainbowBall();
		}
	}
	else if (status == Socket::Disconnected) {
		handleDisconnection(clientIndex);
	}
	else {
		handleErrors(status);
	}
}

void handleDisconnection(size_t index) {
	cout << "Client disconnected: " << clientData[index].socket->getRemoteAddress() << "\n";
	selector.remove(*clientData[index].socket);
	clientData.erase(clientData.begin() + index);
	bothPlayersConnected = (clientData.size() == 2);
}


void trySpawnRainbowBall() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - lastSpawnAttempt).count() >= 5) {
		spawnRainbowBall();
		lastSpawnAttempt = ClockType::now();
	}
}

void spawnRainbowBall() {
	Vector2f position(rand() % static_cast<int>(WINDOW_WIDTH - CIRCLE_RADIUS * 2), rand() % static_cast<int>(WINDOW_HEIGHT - CIRCLE_RADIUS * 2));
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
		despawnRainbowBall();
	}
}

void despawnRainbowBall() {
	hasRainbowBall = false;
	Packet packet;
	packet << "DESPAWN";
	broadcastToClients(packet);
}

void broadcastToClients(Packet& packet) {
	for (const auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) {
			cerr << "Failed to send packet to a client.\n";
		}
	}
}


void sendPlayerPositions() {
	Packet packet;
	packet << "PLAYER_POSITIONS";

	// Player ID + positions
	for (const auto& client : clientData) {
		packet << client.ID << client.position.x << client.position.y;
		cout << "Send for ID " << client.ID << " " << client.position.x << ", " << client.position.y << endl;
	}

	for (auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) {
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