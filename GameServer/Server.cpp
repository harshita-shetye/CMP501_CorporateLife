/*

Code & Functions Overview:

1. Constructor, run() - Main listener + socket flow for if the players connect, disconnect, rainbow balls spawn/despawn, etc.
2. processNewClient(), -
3. processClientData(), - Send predicted coordinates to the clients

*/


#include "Server.h"

// Constructor  -> Initialize
Server::Server(unsigned short port) {

	// Attemp to bind the TCP listener to the specified port. If fail, then set the server's running flag to false and exit.
	if (listener.listen(port) != Socket::Done) {
		cerr << "Could not start on the port: " << port << endl;
		running = false;
		return;
	}

	// If successful, listen for any incoming connections.
	cout << "Listening successful on port " << port << ". Waiting for incoming connections.. \n";
	selector.add(listener);
}

// As long as the server is running...
void Server::run() {
	while (running) {

		// Check for any incoming events with 10 ms timeout
		if (selector.wait(milliseconds(10))) {

			// If socket is ready, then process the new client connection
			if (selector.isReady(listener)) {
				processNewClient(); // Add to clientData
			}

			// Next, iterate through the clients that are connected
			for (size_t i = 0; i < clientData.size(); ++i) {

				// If any socket is ready, then process the data from it
				if (selector.isReady(*clientData[i].socket)) {

					// Gets the player's name provided and latest actual positions
					processClientData(*clientData[i].socket, i);
				}
			}
		}

		// If both the clients are connected
		if (bothPlayersConnected) {

			// Send the updated player positions every 20 ms. This includes the predicted positins. Restart the clock after sending the data.
			if (ticker.getElapsedTime().asMilliseconds() > 20) {
				sendPlayerPositions();
				ticker.restart();
			}

			// Send the Player ID only once, the first time
			if (!sent) sendPlayerId();

			// Although the rainbow ball may despawn if the player collides with it, only spawn it if 5 seconds have passed. It will spawn and despawn every 5 seconds. More info on this in the specific functions.
			if (!hasRainbowBall) trySpawnRainbowBall();

			// If the rainbow ball already exists, then check if 5 seconds have elapsed. If yes, then despawn it.
			else checkRainbowBallTimeout();
		}
	}

	// Server has stopped running
	cout << "Shutting down the server.\n";
}

/* ------------------------ Process New Client ------------------------ */


void Server::processNewClient() {

	// Static so that it doesn't reset on each function call (whenever new client connects)
	static int nextPlayerID = 0;

	// If more than 2 players attempt to connect, then show them an warning message that the server if full and exit.
	if (clientData.size() >= 2) {
		sendFullLobbyMessage(listener);
		return;
	}

	// Else...
	// Create a new socket instance for each client in order to handle separate communication - multiple clients are handled by the server, keeping their data separate!! 

	auto newClient = make_unique<TcpSocket>();

	// Listen for any incoming client connections... When a client connects, create a newClient unique socket.
	if (listener.accept(*newClient) == Socket::Done) {

		// Selector handles multiple sockets to check for incoming data from any of the clients simultaneously
		selector.add(*newClient);

		// Creating a new ClientData object to store and pass the data in an easier way.
		// Could also do: clientData.push_back({move(newClient), nextPlayerID++, {0.0f, 0.0f});
		ClientData newClientData;
		newClientData.socket = move(newClient);
		newClientData.ID = nextPlayerID++;
		newClientData.position = { 100.0f, 100.0f }; // Starting position for both clients - (100, 100)
		clientData.push_back(move(newClientData)); // Push this client's data into clientData

		cout << "New Client ID " << newClientData.ID << " connected: " << clientData.back().socket->getRemoteAddress() << "\n";

		// Checks if both the players are connected
		notifyClientsOnConnection();
	}
}

void Server::sendFullLobbyMessage(TcpListener& listener) {
	auto tempSocket = make_unique<TcpSocket>();
	if (listener.accept(*tempSocket) == Socket::Done) {
		Packet packet;
		packet << "The lobby is full. Try again later.";
		tempSocket->send(packet);
		tempSocket->disconnect();
	}
}

void Server::notifyClientsOnConnection() {
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

		// Set this boolean to true to perform further actions in run()
		bothPlayersConnected = true;
	}
}

void Server::sendPlayerId() {

	// Create unique packets for each client containing their unique ID and send it to the respective client
	Packet client1, client2;
	client1 << "PLAYER_ID" << clientData[0].ID;
	client2 << "PLAYER_ID" << clientData[1].ID;

	if (clientData[0].socket->send(client1) != Socket::Done) {
		cerr << "Failed to send player positions to a client.\n";
	}
	if (clientData[1].socket->send(client2) != Socket::Done) {
		cerr << "Failed to send player positions to a client.\n";
	}
	sent = true;
}

/* ------------------------ Process incoming packets------------------------ */

void Server::processClientData(TcpSocket& client, size_t clientIndex) {
	Packet packet;
	auto status = client.receive(packet);

	// Check if the player is sending their name or current actual position
	if (status == Socket::Done) {
		string command;
		packet >> command;

		// If name, then store it in the respective client's player name
		if (command == "PLAYER_NAME") {
			string receivedName;
			packet >> receivedName;

			clientData[clientIndex].playerName = receivedName;
			cout << "Player " << clientIndex << " is now known as " << receivedName << "\n";
		}

		// If currnet actual position, then synchronize the position incase of network delays. Client sends the current (x, y) position along with the 
		if (command == "UPDATE_POSITION") {
			float x, y, moveX, moveY;
			packet >> x >> y >> moveX >> moveY;

			//cout << "Received: " << x << ", " << y << endl;

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

			// Check for collision with the rainbow ball
			if (hasRainbowBall && isPlayerTouchingRainbowBall(clientRef)) {
				// Increment score and despawn rainbow ball
				clientRef.score++;
				despawnRainbowBall();  // Despawn the rainbow ball if touched
				broadcastUpdatedScores();  // Send updated scores to both players
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

bool Server::isPlayerTouchingRainbowBall(ClientData& player) const {
	float distance = sqrt(pow(player.position.x - rainbowBall.first.x, 2) + pow(player.position.y - rainbowBall.first.y, 2));
	return distance < CIRCLE_RADIUS;  // Check if the player is within range of the rainbow ball's radius
}

void Server::broadcastUpdatedScores() {
	Packet packet;
	packet << "UPDATE_SCORES";
	for (const auto& client : clientData) {
		packet << client.ID << client.playerName << client.score;  // Include player name
		cout << "Client " << client.ID << " (" << client.playerName << ") score updated: " << client.score << "\n";
	}
	broadcastToClients(packet);  // Send the updated scores to all clients
}

void Server::broadcastToClients(Packet& packet) {
	for (const auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) {
			cerr << "Failed to send packet to a client.\n";
		}
		else {
			//cout << "Sent packet to client " << client.ID << "\n";
		}
	}
}

void Server::handleDisconnection(size_t index) {
	cout << "Client disconnected: " << clientData[index].socket->getRemoteAddress() << "\n";
	selector.remove(*clientData[index].socket);
	clientData.erase(clientData.begin() + index);
	bothPlayersConnected = (clientData.size() == 2);
	sent = false;
	running = false;
	clientData.clear();
	selector.remove(listener);
	listener.close();
	cout << "Shutting down server since the player has disconnected. Please restart to play again.\n";
}

/* Process outgoing packets */
void Server::sendPlayerPositions() {
	Packet packet;
	packet << "PLAYER_POSITIONS";
	Vector2f predictedPosition;

	// Player ID + positions
	for (const auto& client : clientData) {

		// Predict the next position
		Time elapsed = ticker.getElapsedTime();
		float elapsedSeconds = min(elapsed.asSeconds(), 0.2f); // Cap extrapolation to 200 ms. Prevent predictions from drifting too far if updates are delayed. Define a maximum extrapolation time (e.g., 200 ms).

		// Predict using velocity and acceleration. Before sending predicted positions, check if the last update was recent.
		if (client.lastReceivedUpdate.getElapsedTime().asSeconds() > 1.0f) { // <=1 or > 1?
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
		if (abs(predictedPosition.x - client.position.x) > maxDrift ||
			abs(predictedPosition.y - client.position.y) > maxDrift) {
			predictedPosition = client.position; // Revert to last known position
		}

		float latency = client.lastReceivedUpdate.getElapsedTime().asSeconds();
		predictedPosition += client.velocity * latency +
			0.5f * client.acceleration * latency * latency;

		packet << client.ID << predictedPosition.x << predictedPosition.y;
		//cout << "Send for ID " << client.ID << " Predicted: " << predictedPosition.x << ", " << predictedPosition.y << endl;
	}

	for (auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) {
			cerr << "Failed to send player positions to a client.\n";
		}
	}
}

/* Spawn rainbow ball */
void Server::trySpawnRainbowBall() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - lastSpawnAttempt).count() >= 5) {
		spawnRainbowBall();
		lastSpawnAttempt = ClockType::now();
	}
}

void Server::spawnRainbowBall() {
	Vector2f position(rand() % static_cast<int>(WINDOW_WIDTH - CIRCLE_RADIUS * 2), rand() % static_cast<int>(WINDOW_HEIGHT - CIRCLE_RADIUS * 2));
	Color color(rand() % 256, rand() % 256, rand() % 256);
	rainbowBall = { position, color };
	hasRainbowBall = true;
	spawnTime = ClockType::now();
	float spawnTimeSeconds = chrono::duration<float>(spawnTime.time_since_epoch()).count(); // Convert spawn time to a float representing seconds since the epoch

	Packet packet;
	packet << "SPAWN" << position.x << position.y << static_cast<Uint8>(color.r) << static_cast<Uint8>(color.g) << static_cast<Uint8>(color.b) << spawnTimeSeconds;
	cout << "Spawn Rainbow at " << position.x << ", " << position.y << ". Time: " << spawnTimeSeconds << " seconds.\n";
	broadcastToClients(packet);
}

/* Despawn rainbow ball */
void Server::checkRainbowBallTimeout() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - spawnTime).count() >= 5) {
		despawnRainbowBall();
	}
}

void Server::despawnRainbowBall() {
	hasRainbowBall = false;
	Packet packet;
	packet << "DESPAWN";
	broadcastToClients(packet);
}

/* Handle any errors */
void Server::handleErrors(Socket::Status status) {
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

// Destructor  -> Clean up the Server
Server::~Server() {

	// Disconnect all the clients present in clientData and then clear it
	for (auto& client : clientData) {
		if (client.socket) {
			client.socket->disconnect();
		}
	}
	clientData.clear();

	// Remove and close the listener from the selector
	selector.remove(listener);
	listener.close();

	cout << "Humanity has been erased successfully ^_^ \n";
}