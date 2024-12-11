#include "Server.h"

// Global variables
TcpListener listener;
SocketSelector selector;
vector<unique_ptr<TcpSocket>> clients;
pair<Vector2f, Color> rainbowBall;
ClockType::time_point spawnTime;
ClockType::time_point lastSpawnAttempt = ClockType::now();
bool running = true;
bool hasRainbowBall = false;

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

		// Handle rainbow ball spawn/despawn logic
		if (!hasRainbowBall && chrono::duration_cast<chrono::seconds>(ClockType::now() - lastSpawnAttempt).count() >= 5) {
			loadRainbowBall(clients);
			lastSpawnAttempt = ClockType::now();
		}

		if (hasRainbowBall && chrono::duration_cast<chrono::seconds>(ClockType::now() - spawnTime).count() >= 5) {
			despawnRainbowBall(clients);
		}

		sendPlayerPositions(clients);
	}

	cout << "Shutting down the server.\n";
}

void loadRainbowBall(vector<unique_ptr<TcpSocket>>& clients) {
	Vector2f position(rand() % static_cast<int>(WINDOW_WIDTH - CIRCLE_RADIUS * 2),
		rand() % static_cast<int>(WINDOW_HEIGHT - CIRCLE_RADIUS * 2));
	Color color(rand() % 256, rand() % 256, rand() % 256);

	rainbowBall = { position, color };
	hasRainbowBall = true;
	spawnTime = ClockType::now();

	Packet packet;
	packet << "SPAWN" << position.x << position.y
		<< static_cast<Uint8>(color.r) << static_cast<Uint8>(color.g) << static_cast<Uint8>(color.b);

	for (const auto& client : clients) {
		if (client->send(packet) != Socket::Done) {
			cerr << "Failed to send spawn packet to a client.\n";
		}
	}
	cout << "Rainbow ball spawned at (" << position.x << ", " << position.y << ").\n";
}

void despawnRainbowBall(vector<unique_ptr<TcpSocket>>& clients) {
	hasRainbowBall = false;

	Packet packet;
	packet << "DESPAWN";

	for (const auto& client : clients) {
		if (client->send(packet) != Socket::Done) {
			cerr << "Failed to send despawn packet to a client.\n";
		}
	}
	cout << "Rainbow ball despawned.\n";
}

void processNewClient(TcpListener& listener, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients) {
	if (clients.size() >= 2) {
		auto tempSocket = make_unique<TcpSocket>();
		if (listener.accept(*tempSocket) == Socket::Done) {
			Packet packet;
			packet << "The lobby is full right now. Please try again later.";
			tempSocket->send(packet);
			tempSocket->disconnect();
		}
		return;
	}

	auto newClient = make_unique<TcpSocket>();
	if (listener.accept(*newClient) == Socket::Done) {
		cout << "New client connected: " << newClient->getRemoteAddress() << "\n";

		selector.add(*newClient);
		clients.push_back(std::move(newClient));

		Vector2f initialPosition(rand() % static_cast<int>(WINDOW_WIDTH - CIRCLE_RADIUS * 2),
			rand() % static_cast<int>(WINDOW_HEIGHT - CIRCLE_RADIUS * 2));
		Packet packet;
		packet << initialPosition.x << initialPosition.y;

		clients.back()->send(packet);
	}
}

void processClientData(TcpSocket& client, SocketSelector& selector, vector<unique_ptr<TcpSocket>>& clients, size_t clientIndex) {
	Packet packet;
	auto status = client.receive(packet);

	if (status == Socket::Done) {
		string message;
		packet >> message;
		cout << "Client (" << client.getRemoteAddress() << ") message: " << message << "\n";

		if (message == "DESPAWN") {
			despawnRainbowBall(clients);
		}
	}
	else if (status == Socket::Disconnected) {
		cout << "Client disconnected: " << client.getRemoteAddress() << "\n";
		selector.remove(client);
		clients.erase(clients.begin() + clientIndex);
	}
	else {
		handleErrors(status);
	}
}

void sendPlayerPositions(vector<unique_ptr<TcpSocket>>& clients) {
	Packet packet;
	auto timestamp = chrono::duration_cast<chrono::milliseconds>(ClockType::now().time_since_epoch()).count();

	for (const auto& client : clients) {
		Vector2f position(rand() % static_cast<int>(WINDOW_WIDTH),
			rand() % static_cast<int>(WINDOW_HEIGHT));
		packet << position.x << position.y << timestamp;

		if (client->send(packet) != Socket::Done) {
			cerr << "Failed to send player position to a client.\n";
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