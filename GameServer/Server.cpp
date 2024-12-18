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
		handleErrors("Server", listener.listen(port));
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
		if (selector.wait(milliseconds(20))) {

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

			// Send the updated player positions every 30 ms. This includes the predicted positins. Restart the clock after sending the data.
			if (ticker.getElapsedTime().asMilliseconds() > 30) {
				sendPlayerPositions();
				cout << "Sent positions at " << ticker.getElapsedTime().asMilliseconds() << endl;
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

// Function called if more than 2 players try to connect. IT gives a new window which says that the server is full. Player can then go back and exit.
void Server::sendFullLobbyMessage(TcpListener& listener) {
	auto extraPlayer = make_unique<TcpSocket>();
	if (listener.accept(*extraPlayer) == Socket::Done) {
		Packet packet;
		packet << "The lobby is full. Sorry!";
		extraPlayer->send(packet);
		extraPlayer->disconnect();
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
			if (client.socket->send(startPacket) != Socket::Done) handleErrors("notifyClientsOnConnection", client.socket->send(startPacket));
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

	if (clientData[0].socket->send(client1) != Socket::Done) handleErrors("sendPlayerId1", clientData[0].socket->send(client1));
	if (clientData[1].socket->send(client2) != Socket::Done) handleErrors("sendPlayerId2", clientData[1].socket->send(client2));
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

			auto& clientRef = clientData[clientIndex];
			Vector2f newPosition = { x, y };
			clientRef.movementVector = { moveX, moveY };
			Time elapsed = clientRef.lastUpdateTime.getElapsedTime();
			clientRef.lastUpdateTime.restart();
			clientRef.lastReceivedUpdate.restart(); // UPDATE_POSITION command received

			Vector2f newVelocity = clientRef.movementVector / elapsed.asSeconds();
			clientRef.acceleration = (newVelocity - clientRef.velocity) / elapsed.asSeconds();
			clientRef.velocity = smoothingFactor * clientRef.velocity + (1.0f - smoothingFactor) * newVelocity;
			//clientRef.velocity *= dampingFactor;

			//cout << "Received for Client " << clientIndex << ": " << x << ", " << y << " || " << gameTime.getElapsedTime().asSeconds() << " || V" << clientRef.velocity.x << ", " << clientRef.velocity.y << endl;

			// Check for collision with the rainbow ball
			if (hasRainbowBall && isPlayerTouchingRainbowBall(clientRef)) {
				// Increment score and despawn rainbow ball
				clientRef.score++;
				despawnRainbowBall();  // Despawn the rainbow ball if touched
				broadcastUpdatedScores();  // Send updated scores to both players
			}

			// Store the new position in the deque
			PositionSnapshot snapshot = { newPosition, chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() };
			auto& positionHistory = clientRef.positionHistory;
			positionHistory.push_back(snapshot);

			// Initialize or update the position history
			if (positionHistory.empty()) {
				// Add the first snapshot twice to initialize with valid data
				positionHistory.push_back(snapshot);
				positionHistory.push_back(snapshot);
			}
			else if (positionHistory.size() < 2) {
				// Add one more snapshot to allow predictions
				positionHistory.push_back(snapshot);
			}
			else {
				// Regular update: Add the snapshot and ensure the size limit
				positionHistory.push_back(snapshot);
				if (positionHistory.size() > 4) {
					positionHistory.pop_front();
				}
			}

			clientRef.position = newPosition;
		}
	}
	else if (status == Socket::Disconnected) {
		handleDisconnection(clientIndex);
	}
	else {
		handleErrors("processClientData", status);
	}
}

bool Server::isPlayerTouchingRainbowBall(ClientData& player) const {

	// We take player.position (player's position and rainbowBall.first to access the Vecfor2f, which is in the first position of the pair.
	// Then, we use the distance formula to get the distance between the player's shape and rainbow ball shape. Distance = sqrt[(x2 - x1)^2 + (y2 - y1)^2], where 2 represents player and 1 is rainbow ball

	float distance = sqrt(pow(player.position.x - rainbowBall.first.x, 2) + pow(player.position.y - rainbowBall.first.y, 2));
	return distance < RAINBOW_RADIUS;  // Check if the player is within range of the rainbow ball's radius, which is set to 17 since player shape is 15. Added a little outer padding/ The rainbow ball's actual raidus is 7 on cleint's screen.
}

// This function is called if the player touches the present rainbow ball. It creates an "UPDATE_SCORES" command in the packet with the updated scores for both the clients.
void Server::broadcastUpdatedScores() {
	Packet packet;
	packet << "UPDATE_SCORES";
	for (const auto& client : clientData) {
		packet << client.ID << client.playerName << client.score;
		//cout << "Client " << client.ID << " (" << client.playerName << ") score updated: " << client.score << "\n";
	}
	broadcastToClients(packet);  // Send the updated scores to all clients
}

// The packet is then sent to each client. Packet contains updated scores for both clients.
void Server::broadcastToClients(Packet& packet) {
	for (const auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) handleErrors("broadcastToClients", client.socket->send(packet));
	}
}

// Incase a player disconnects,
void Server::handleDisconnection(size_t index) {
	cout << "Client disconnected: " << clientData[index].socket->getRemoteAddress() << "\n";
	bothPlayersConnected = (clientData.size() == 2);
	sent = false;
	running = false;
	clientData.clear();
	selector.remove(listener);
	listener.close();
	cout << "Shutting down server since the player has disconnected. Please restart to play again.\n";
}


//------- ------- ------- HANDLE PREDICTION LOGIC AND SEND CLIENTS THE PREDICTED POSITIONS ------ ------- -------//

void Server::predictedPosition(const ClientData& client) {
	// Player ID + positions
	for (auto& client : clientData) {
		Vector2f predictedPosition = client.position; // Default to last known position

		// Predict the next position if enough data is available
		if (client.positionHistory.size() >= 2) {
			const PositionSnapshot& lastPosition = client.positionHistory.back();
			const PositionSnapshot& secondLastPosition = client.positionHistory[client.positionHistory.size() - 2];

			// Calculate velocity (last two positions)
			float timeDelta = (lastPosition.timestamp - secondLastPosition.timestamp) / 1000.0f; // Convert ms to seconds
			if (timeDelta > 0.0f) {
				Vector2f velocity = (lastPosition.position - secondLastPosition.position) / timeDelta;

				// Predict based on velocity and elapsed time
				float predictionTime = 2.f * timeDelta; // ticker.getElapsedTime().asSeconds();
				predictedPosition = lastPosition.position + (client.velocity * 2.5f) * predictionTime;
			}
			else {
				// Not enough data, use last known position
				client.predictedPosition = client.position;
			}
		}
		else client.predictedPosition = client.position;

		// Apply smoothing to avoid sudden jumps
		predictedPosition = smoothingFactor * client.position + (1.0f - smoothingFactor) * predictedPosition;

		// Limit drift
		float maxDrift = 100.0f; // Maximum allowable drift
		if (abs(predictedPosition.x - client.position.x) > maxDrift ||
			abs(predictedPosition.y - client.position.y) > maxDrift) {
			predictedPosition = client.position; // Revert to last known position
		}


		//cout << "Predicted for Client " << client.ID << ": " << predictedPosition.x << ", " << predictedPosition.y << " || " << gameTime.getElapsedTime().asSeconds() << " || V" << client.velocity.x << ", " << client.velocity.y << endl;

		client.predictedPosition.x = predictedPosition.x;
		client.predictedPosition.y = predictedPosition.y;
	}
}

// Send PLAYER_POSITIONS command to the client along with the predicted positions and the current timestamp
void Server::sendPlayerPositions() {
	Packet packet;
	packet << "PLAYER_POSITIONS";

	// Timestamp in milliseconds (ms)
	auto now = chrono::high_resolution_clock::now();
	auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();

	// Timestamp + Player ID + positions
	for (const auto& client : clientData) {
		predictedPosition(client);
		packet << timestamp << client.ID << client.predictedPosition.x << client.predictedPosition.y;

		//cout << timestamp << " Send for ID " << client.ID << " Predicted: " << predictedPosition.x << ", " << predictedPosition.y << endl;
	}

	for (auto& client : clientData) {
		if (client.socket->send(packet) != Socket::Done) handleErrors("sendPlayerPositions", client.socket->send(packet));
	}
}


//------- ------- ------- ------- RAINBOW BALL SPAWN & DESPAWN LOGIC ------  ------ ------- -------//

// Check if it's been 5 seconds since the last rainbow ball spawn time. If yes, then spawn a new one
void Server::trySpawnRainbowBall() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - rainbowSpawnTime).count() >= 5) {
		spawnRainbowBall();
	}
}

// Once 5 seconds have passed, a new random location (within the window bounds) and random colour will be created to be sent to the client. 
void Server::spawnRainbowBall() {

	// Random width and height. We multiply radius by 2 and subtract it to make sure that the rainbow ball appears within the boundaries
	Vector2f position(rand() % (int)(WINDOW_WIDTH - (RAINBOW_RADIUS * 2)), rand() % (int)(WINDOW_HEIGHT - (RAINBOW_RADIUS * 2)));
	Color color(rand() % 256, rand() % 256, rand() % 256); // Random number generated from 0 - 255

	// Set up the rainbow ball pair
	rainbowBall = { position, color };
	hasRainbowBall = true; // So that a despawn signal can be sent later
	rainbowSpawnTime = ClockType::now(); // Reset the spawn time (5 seconds before it despawns)

	float spawnTimeSeconds = chrono::duration<float>(rainbowSpawnTime.time_since_epoch()).count(); // Convert spawn time to float in seconds to timestamp it's spawn time

	Packet packet;
	packet << "SPAWN" << position.x << position.y << static_cast<Uint8>(color.r) << static_cast<Uint8>(color.g) << static_cast<Uint8>(color.b) << spawnTimeSeconds;
	cout << "Spawn Rainbow at " << position.x << ", " << position.y << ". Time: " << spawnTimeSeconds << " seconds.\n";

	// Send rainbow ball packet to the clients
	broadcastToClients(packet);
}

// Checks if 5 seconds have passed. If yes, then a despawn signail will be sent to the client
void Server::checkRainbowBallTimeout() {
	if (chrono::duration_cast<chrono::seconds>(ClockType::now() - rainbowSpawnTime).count() >= 5) {
		despawnRainbowBall();
	}
}

// Send a DESPAWN command to the client to signal despawning the rainbow ball
void Server::despawnRainbowBall() {
	hasRainbowBall = false;
	Packet packet;
	packet << "DESPAWN";
	broadcastToClients(packet);
}


//------- ------- ------- ------- HANDLE ALL THE ERRORS ------  ------ ------- -------//

// Handles all the errors. Format --> FunctionName: Error encountered
void Server::handleErrors(string func, Socket::Status status) {
	switch (status) {
	case Socket::NotReady:
		cerr << func << ": Socket not ready to send/receive data.\n";
		break;
	case Socket::Partial:
		cerr << func << ": Partial data sent/received.\n";
		break;
	case Socket::Disconnected:
		cerr << func << ": Socket disconnected.\n";
		break;
	case Socket::Error:
	default:
		cerr << func << ": An unexpected socket error occurred.\n";
		break;
	}
}


//------- ------- ------- ------- DECONSTRUCTOR ------ ------  ------ ------- -------//

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