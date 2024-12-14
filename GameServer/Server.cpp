#include "Server.h"

struct ClientData {
	unique_ptr<TcpSocket> socket;
	Vector2f position;
	int ID;

	// For prediction
	Vector2f velocity;
	Vector2f acceleration;
	Clock lastUpdateTime;
	Clock lastReceivedUpdate; // If updates from a client are delayed or lost, rely on predictions for a limited time before freezing their position to avoid erratic behavior.
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
float smoothingFactor = 0.8f; // Apply a smoothing factor when updating velocities to reduce sudden changes caused by small inaccuracies or lag.
float dampingFactor = 0.95f; // Apply a damping factor to slow down abrupt velocity changes.


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


//******* Initial Connection Logic *******//

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


//******* Current Clients Logic *******//

void processClientData(TcpSocket& client, size_t clientIndex) {
	Packet packet;
	auto status = client.receive(packet);

	if (status == Socket::Done) {
		string command;
		packet >> command;

		if (command == "UPDATE_POSITION") {
			float x, y, moveX, moveY;
			packet >> x >> y >> moveX >> moveY;

			auto& clientRef = clientData[clientIndex];
			Vector2f newPosition = { x, y };
			Vector2f movementVector = { moveX, moveY };
			Time elapsed = clientRef.lastUpdateTime.getElapsedTime();
			clientRef.lastUpdateTime.restart();

			if (elapsed.asSeconds() > 0) {
				Vector2f newVelocity = movementVector / elapsed.asSeconds();
				clientRef.acceleration = (newVelocity - clientRef.velocity) / elapsed.asSeconds();
				clientRef.velocity = smoothingFactor * clientRef.velocity + (1.0f - smoothingFactor) * newVelocity;
				clientRef.velocity *= dampingFactor;
			}
			else {
				cout << "No movement detected from Client " << clientIndex;
				clientRef.velocity = { 0.0f, 0.0f };
				clientRef.acceleration = { 0.0f, 0.0f };
			}

			clientRef.position = newPosition;
			clientData[clientIndex].position = { x, y };
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

// Send the current player positions (predicted)
void sendPlayerPositions() {
	Packet packet;
	packet << "PLAYER_POSITIONS";
	Vector2f predictedPosition;

	// Player ID + positions
	for (const auto& client : clientData) {
		// Predict the next position
		Time elapsed = ticker.getElapsedTime();
		float elapsedSeconds = std::min(elapsed.asSeconds(), 0.2f); // Cap extrapolation to 200 ms. Prevent predictions from drifting too far if updates are delayed. Define a maximum extrapolation time (e.g., 200 ms).

		// Predict using velocity and acceleration. Before sending predicted positions, check if the last update was recent.
		if (client.lastReceivedUpdate.getElapsedTime().asSeconds() > 1.0f) {
			// Prediction using velocity and acceleration
			predictedPosition += (client.velocity * elapsedSeconds)
				+ (0.5f * client.acceleration * elapsedSeconds * elapsedSeconds);
		}
		else {
			// Send last known position if updates are too old
			//predictedPosition += movementVector * elapsedSeconds; // or use velocity to predict
			packet << client.ID << client.position.x << client.position.y;
		}
		//Apply smoothing to reduce jitter
		predictedPosition = smoothingFactor * client.position + (1.0f - smoothingFactor) * predictedPosition;

		float maxDrift = 50.f; // Maximum allowable drift
		if (std::abs(predictedPosition.x - client.position.x) > maxDrift ||
			std::abs(predictedPosition.y - client.position.y) > maxDrift) {
			predictedPosition = client.position; // Revert to last known position
		}

		packet << client.ID << predictedPosition.x << predictedPosition.y;
		//cout << "Send for ID " << client.ID << " Predicted: " << predictedPosition.x << ", " << predictedPosition.y << endl;
	}

	for (auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) {
			cerr << "Failed to send player positions to a client.\n";
		}
	}
}



//******* Rainbow Logic *******//

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
	float spawnTimeSeconds = chrono::duration<float>(spawnTime.time_since_epoch()).count(); // Convert spawn time to a float representing seconds since the epoch

	Packet packet;
	packet << "SPAWN" << position.x << position.y << static_cast<Uint8>(color.r) << static_cast<Uint8>(color.g) << static_cast<Uint8>(color.b) << spawnTimeSeconds;
	cout << "Spawn Rainbow at " << position.x << ", " << position.y << ". Time: " << spawnTimeSeconds << " seconds.\n";
	// You can also add logging to monitor the size of the packet
	cout << "Rainbows Packet size: " << packet.getDataSize() << endl;
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
		else {
			cout << "Sent packet to client " << client.ID << "\n";
		}
	}
}


//******* Handle Errors *******//

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