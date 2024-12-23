#include "Client.h"

// Constructor
Client::Client() :
	window(nullptr),
	playerID(-1),
	currentState(GameState::MainMenu),
	hasRainbowBall(false),
	actualPlayerShape(CIRCLE_RADIUS),
	predictedPlayerShape(CIRCLE_RADIUS),
	otherPlayerShape(CIRCLE_RADIUS)
{}

// Destructor
Client::~Client() {
	if (window) {
		delete window;
	}
}

// Run the client
void Client::run() {
	createMainMenu();
}

// Connect to the server
bool Client::connectToServer() {
	// Try to connect to the server
	if (socket.connect(SERVER, PORT) != Socket::Done) {
		cerr << "Error: Could not connect to server.\n";
		return false;
	}
	return true;
}

/* Game UI */
// Create and display the main menu
void Client::createMainMenu() {

	bool showError = false;
	bool isEditingName = false;
	bool isEditingIPv4 = false;

	// Loading the font
	Font font;
	if (!font.loadFromFile("res/comic.ttf")) {
		cerr << "Failed to load font!" << endl;
		return;
	}

	// Main Menu Loop
	while (true) {
		if (currentState == GameState::MainMenu) {
			// Check if the window is already created or create a new one
			if (!window) {
				window = new RenderWindow(VideoMode(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), "Main Menu", Style::Close);
				window->setVerticalSyncEnabled(true);
			}

			// Adjust UI elements dynamically
			Vector2u windowSize = window->getSize();
			float centerX = windowSize.x / 2;
			float centerY = windowSize.y / 2;

			Text title = createText("Eat The Dots", font, 50, Color(255, 215, 0), centerX - 180, 50);
			title.setStyle(Text::Bold | Text::Underlined);

			// Buttons
			float buttonWidth = 200.f, buttonHeight = 50.f;
			RectangleShape playButton = createButton(buttonWidth, buttonHeight, Color(100, 149, 237), centerX - buttonWidth / 2, centerY - 75);
			RectangleShape quitButton = createButton(buttonWidth, buttonHeight, Color(178, 34, 34), centerX - buttonWidth / 2, centerY + 25);

			// Button Text
			Text playText = createText("Play", font, 24, Color::White, centerX - 25, centerY - 65);
			Text quitText = createText("Quit", font, 24, Color::White, centerX - 25, centerY + 35);

			// Prompts and Input
			Text namePrompt = createText("Enter your name:", font, 24, Color::White, centerX - 200, centerY + 100);
			Text nameInput = createText(playerName.empty() ? "Click to enter name" : playerName, font, 24, isEditingName ? Color::White : Color::Cyan, centerX, centerY + 100);

			Text IPv4Prompt = createText("Enter IPv4:", font, 24, Color::White, centerX - 200, centerY + 150);
			Text IPv4Input = createText(SERVER.empty() ? "Click to enter IPv4" : SERVER, font, 24, isEditingIPv4 ? Color::White : Color::Cyan, centerX, centerY + 150);

			Text errorText = createText("Name and IPv4 cannot be empty! >:(", font, 20, Color::Red, centerX - 200, centerY + 200);

			// Update button colors for hover effects
			Vector2i mousePos = Mouse::getPosition(*window);
			playButton.setFillColor(playButton.getGlobalBounds().contains(Vector2f(mousePos)) ? Color(70, 130, 180) : Color(100, 149, 237));
			quitButton.setFillColor(quitButton.getGlobalBounds().contains(Vector2f(mousePos)) ? Color(255, 69, 0) : Color(178, 34, 34));

			// Handle events (e.g., button clicks)
			Event event;
			while (window->pollEvent(event)) {
				if (event.type == Event::Closed) {
					window->close();
					delete window;  // Clean up the window
					window = nullptr;
					return;
				}

				if (event.type == Event::MouseButtonPressed) {
					Vector2f mousePosition(event.mouseButton.x, event.mouseButton.y);
					if (playButton.getGlobalBounds().contains(mousePosition)) {

						if (playerName.empty() || SERVER.empty()) {
							showError = true;
						}
						else {
							showError = false;

							if (socket.getRemoteAddress() == IpAddress::None && !connectToServer()) {
								cerr << "Failed to connect to server!" << endl;
								continue;
							}
							else {
								// Send player name to server
								Packet packet;
								packet << "PLAYER_NAME" << playerName;

								if (socket.send(packet) != Socket::Done) {
									cerr << "Failed to send player name to server!" << endl;
									continue;
								}
							}
						}

						// Once connected, receive a message from the server
						Packet packet;
						if (socket.receive(packet) == Socket::Done) {
							string message;
							packet >> message;

							if (message == "Waiting for another player to connect...") {
								currentState = GameState::WaitingForPlayer;
							}
							else if (message == "The lobby is full. Try again later.") {
								currentState = GameState::LobbyFull;
							}
							else {
								currentState = GameState::Playing;
							}
							break;
						}
					}
					else if (quitButton.getGlobalBounds().contains(mousePosition)) {
						window->close();
						delete window;
						window = nullptr;
						return;
					}
					else if (nameInput.getGlobalBounds().contains(mousePosition)) {
						isEditingName = true;
						isEditingIPv4 = false;
						playerName.clear();
					}
					else if (IPv4Input.getGlobalBounds().contains(mousePosition)) {
						isEditingIPv4 = true;
						isEditingName = false;
						SERVER.clear();
					}
					else {
						isEditingName = false;
						isEditingIPv4 = false;
					}
				}

				if (event.type == Event::TextEntered) {
					if (isEditingName) {
						// Handle playerName input
						if (event.text.unicode == '\b') { // Handle backspace
							if (!playerName.empty()) {
								playerName.pop_back();
							}
						}
						else if (event.text.unicode == '\r') { // Handle Enter key
							isEditingName = false; // Finish editing
						}
						else if (event.text.unicode < 128) { // Append valid ASCII characters
							playerName += static_cast<char>(event.text.unicode);
						}
					}
					else if (isEditingIPv4) {
						// Handle SERVER input
						if (event.text.unicode == '\b') { // Handle backspace
							if (!SERVER.empty()) {
								SERVER.pop_back();
							}
						}
						else if (event.text.unicode == '\r') { // Handle Enter key
							isEditingIPv4 = false; // Finish editing
						}
						else if (event.text.unicode < 128) { // Append valid ASCII characters
							SERVER += static_cast<char>(event.text.unicode);
						}
					}
				}

			}

			// Draw the main menu UI
			window->clear();
			window->draw(title);
			window->draw(playButton);
			window->draw(playText);
			window->draw(quitButton);
			window->draw(quitText);
			window->draw(namePrompt);
			window->draw(nameInput);
			window->draw(IPv4Prompt);
			window->draw(IPv4Input);
			if (showError) {
				window->draw(errorText);
			}
			window->display();
		}
		else if (currentState == GameState::LobbyFull) {

			Font font;
			if (!font.loadFromFile("res/comic.ttf")) {
				cerr << "Failed to load font!" << endl;
				return;
			}

			Text fullMessage("The lobby is full. Try again later.", font, 30);
			fullMessage.setFillColor(Color::White);
			fullMessage.setPosition(window->getSize().x / 2 - fullMessage.getGlobalBounds().width / 2, window->getSize().y / 3);

			RectangleShape backButton(Vector2f(200, 50));
			backButton.setPosition(window->getSize().x / 2 - 100, window->getSize().y / 2);
			backButton.setFillColor(Color(100, 149, 237)); // Cornflower blue

			Text backText("Back", font, 24);
			backText.setFillColor(Color::White);
			backText.setPosition(backButton.getPosition().x + backButton.getSize().x / 2 - backText.getGlobalBounds().width / 2,
				backButton.getPosition().y + backButton.getSize().y / 2 - backText.getGlobalBounds().height / 2);

			// Update button colors for hover effects
			Vector2i mousePos = Mouse::getPosition(*window);
			backButton.setFillColor(backButton.getGlobalBounds().contains(Vector2f(mousePos)) ? Color(70, 130, 180) : Color(100, 149, 237));

			// Handle events for the lobby full screen
			Event event;
			while (window->pollEvent(event)) {
				if (event.type == Event::Closed) {
					window->close();
					delete window;
					window = nullptr;
					return;
				}

				if (event.type == Event::MouseButtonPressed) {
					Vector2f mousePosition(event.mouseButton.x, event.mouseButton.y);
					if (backButton.getGlobalBounds().contains(mousePosition)) {
						currentState = GameState::MainMenu;  // Go back to Main Menu
						break;
					}
				}
			}

			// Draw the lobby full screen UI
			window->clear();
			window->draw(fullMessage);
			window->draw(backButton);
			window->draw(backText);
			window->display();
		}
		else if (currentState == GameState::WaitingForPlayer) {
			displayWaitingMessage();
			handleWaitingState();
		}
		else if (currentState == GameState::Playing) {
			// Close the main menu window and prepare for the next state
			window->close();
			delete window;
			window = nullptr;

			startGame();
			break;
		}
	}
}

// Helper functions for creating UI elements
Text Client::createText(const string& content, const Font& font, unsigned int size, const Color& color, float x, float y) {
	Text text(content, font, size);
	text.setFillColor(color);
	text.setPosition(x, y);
	return text;
}

RectangleShape Client::createButton(float width, float height, const Color& color, float x, float y) {
	RectangleShape button(Vector2f(width, height));
	button.setFillColor(color);
	button.setPosition(x, y);
	return button;
}


void Client::displayWaitingMessage() {
	// Create font
	Font font;
	if (!font.loadFromFile("res/comic.ttf")) { // Specify the correct font path
		cerr << "Error loading font!" << endl;
		return;
	}

	// Create text to display
	Text waitingText;
	waitingText.setFont(font);
	waitingText.setString("Waiting for player 2...");
	waitingText.setFillColor(Color::White); // Text color
	waitingText.setPosition(window->getSize().x / 2 - waitingText.getGlobalBounds().width / 2,
		window->getSize().y / 2 - waitingText.getGlobalBounds().height / 2); // Center the text

	// Draw text on window
	window->clear();
	window->draw(waitingText);
	window->display();
}

void Client::handleWaitingState() {
	Event event{};
	while (window->pollEvent(event)) {
		if (event.type == Event::Closed) {
			window->close(); // Close the window when the user requests it
			socket.disconnect(); // Disconnect from the server
			return;
		}
	}

	// Make socket non-blocking to prevent it from blocking the window
	socket.setBlocking(false);

	// Try receiving a packet in a non-blocking manner
	Packet packet;
	if (socket.receive(packet) == Socket::Done) {
		string message;
		packet >> message;
		if (message == "Both players connected. Starting the game!") {
			currentState = GameState::Playing;
		}
	}

	socket.setBlocking(true);  // Reset the socket to blocking mode when done
}

void Client::startGame() {

	// Get the spawn position
	receiveInitialPosition();

	// Setup window
	window = new RenderWindow(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Eat The Dots", Style::Default);
	window->setVerticalSyncEnabled(true);
	if (!window) {
		cerr << "Failed to create window for the game!" << endl;
		return;
	}
	gameLoop();
}

// Function to receive the initial position of the player from the server
void Client::receiveInitialPosition() {

	Packet positionPacket;
	string command;
	float x, y;
	int spawnID;

	// If the command is PLAYER_POSITIONS, then we get the spawn position first time
	if (socket.receive(positionPacket) == Socket::Done) {
		positionPacket >> command;
		if (command == "PLAYER_POSITIONS") {
			positionPacket >> spawnID >> x >> y; // Now we extract the position after confirming the command
			actualPlayerShape.setPosition(x, y);
		}
		else {
			cerr << "Received unexpected command: " << command << endl;
		}
	}
	else {
		cout << "There was an error getting the initial spawn positions.";
		return;
	}
}

/* Main Game Loop */

// Main game loop for rendering and handling player movement
void Client::gameLoop() {
	window->setFramerateLimit(60);  // Cap the framerate

	// Create a player shape and set its initial position
	Vector2f targetPos; // Target position based on mouse
	float moveSpeed = 400.f; // Speed of movement in pixels per second
	const float deadZoneRadius = 5.f; // Dead zone radius to prevent jittering

	Clock clock; // For frame-based timing

	while (window->isOpen()) {
		float deltaTime = clock.restart().asSeconds(); // Time since last frame

		Font font;
		if (!font.loadFromFile("res/comic.ttf")) {
			cerr << "Failed to load font!" << endl;
			return;
		}

		Event event;
		while (window->pollEvent(event)) {
			if (event.type == Event::Closed) {
				window->close();
				return;
			}
		}

		// Update target position to the mouse location
		targetPos = window->mapPixelToCoords(Mouse::getPosition(*window)); //convert the pixel coordinates from the mouse to world coordinates

		// Get the current position of the player
		Vector2f currentPos = actualPlayerShape.getPosition();
		Vector2f movementVector;

		// Compute direction vector and normalize it
		Vector2f direction = targetPos - currentPos;
		float distance = sqrt(direction.x * direction.x + direction.y * direction.y);

		// Only move if outside the dead zone
		if (distance > deadZoneRadius) {
			Vector2f normalizedDirection = direction / distance; // Normalize the direction vector
			movementVector = normalizedDirection * moveSpeed * deltaTime;
			actualPlayerShape.move(movementVector);
		}

		// Clamp the player's position within the window bounds
		Vector2f position = actualPlayerShape.getPosition();
		float radius = actualPlayerShape.getRadius();

		position.x = max(0.f, min(position.x, window->getSize().x - 2 * radius));
		position.y = max(0.f, min(position.y, window->getSize().y - 2 * radius));
		actualPlayerShape.setPosition(position);

		sendPlayerPosition(movementVector); // Send the updated position of the player to the server.

		// Receive packet from server. Check what command it is and then call that function.
		Packet receivedPacket;
		auto status = socket.receive(receivedPacket);
		if (status == Socket::Done) {
			string command;
			receivedPacket >> command;

			if (command == "PLAYER_POSITIONS") receivePlayerPositions(receivedPacket);
			else if (command == "PLAYER_ID") receivedPacket >> playerID;
			else if (command == "SPAWN") receiveRainbowData(receivedPacket);
			else if (command == "DESPAWN") deleteRainbowData();
			else if (command == "UPDATE_SCORES") updateScores(receivedPacket);
			else cerr << "Error receiving data from socket: " << status << endl;
		}
		else handleErrors(status);

		// If no update received for 90 ms, then set position to actual player shape.
		if (playerData[playerID].lastReceivedUpdate.getElapsedTime().asMilliseconds() > 90) {
			cout << "Setting to actual position.\n";

			Vector2f currentPosition = predictedPlayerShape.getPosition();
			Vector2f targetPosition = actualPlayerShape.getPosition();
			float extrapolationSpeed = 0.2f;

			Vector2f extrapolatedPosition = applyExtrapolation(currentPosition, targetPosition, extrapolationSpeed);

			// Update the predicted position with the extrapolated value
			predictedPlayerShape.setPosition(extrapolatedPosition);

			// playerData[playerID].lastReceivedUpdate.restart();
		}

		/*for (int i = 0; i < 2; i++) {
			cout << " || ID: " << playerData[i].id << endl;
			cout << " || Name: " << playerData[i].name << endl;
			cout << " || score: " << playerData[i].score << endl;
			cout << " || position: " << playerData[i].position.x << ", " << playerData[i].position.y << endl;
		}*/

		// Clear the window and render everything
		window->clear();
		displayScores(font);
		renderActualSelf(actualPlayerShape.getPosition().x, actualPlayerShape.getPosition().y);
		renderReceivedShapes(); // Draw all the players (self and opponent)
		drawRainbowBalls(); // Draw the rainbow ball
		window->display();
	}
}

// Extrapolation code
Vector2f Client::applyExtrapolation(Vector2f start, Vector2f end, float speed) {
	Vector2f extrapolatedPosition = { start.x + speed * (end.x - start.x), start.y + speed * (end.y - start.y) };
	return extrapolatedPosition;
}

// Send actual player position to the server
void Client::sendPlayerPosition(Vector2f movementVector) {
	Packet packet;
	packet << "UPDATE_POSITION" << actualPlayerShape.getPosition().x << actualPlayerShape.getPosition().y << movementVector.x << movementVector.y;

	cout << "Actual: " << actualPlayerShape.getPosition().x << ", " << actualPlayerShape.getPosition().y << " || " << movementVector.x << ", " << movementVector.y << endl;
	if (socket.send(packet) != Socket::Done) cerr << "Failed to send player position to the server.\n";
}

// Receive the predicted positions from the server
void Client::receivePlayerPositions(Packet packet) {
	for (size_t i = 0; i < playerData.size(); ++i) {
		long long timestamp;
		float x, y;
		int id;
		packet >> timestamp >> id >> x >> y;

		// Update the last received timestamp
		playerData[id].lastReceivedTimestamp = timestamp;
		playerData[id].lastReceivedUpdate.restart();
		playerData[id].id = id;
		playerData[id].position = applyExtrapolation(playerData[id].position, Vector2f(x, y), 0.2); // Apply extrapolation from previous prection position to the newly received predicted position

		if (id == playerID) {
			cout << "Predicted: " << x << ", " << y << " || " << playerData[playerID].lastReceivedUpdate.getElapsedTime().asMilliseconds() << endl;
		}
	}
}

// Receive dots position and colour
void Client::receiveRainbowData(Packet packet) {
	float x, y, spawnTime;
	Uint8 r, g, b;
	packet >> x >> y >> r >> g >> b >> spawnTime;
	rainbowPositions.push_back(Vector2f(x, y));
	rainbowColors.push_back(Color(r, g, b));
	cout << "Rainbow received: " << x << ", " << y << " with spawn time: " << spawnTime << " seconds.\n";
}

// Clears all rainbows since currently we are only spawning 1
void Client::deleteRainbowData() {
	rainbowPositions.clear();
	rainbowColors.clear();
	cout << "Cleared rainbows\n";
}

// Function to update the scores received from the server
void Client::updateScores(Packet& packet) {
	int playerID, score;
	string nameReceived;

	// Decode the scores from the packet
	while (!packet.endOfPacket()) {
		packet >> playerID >> nameReceived >> score;

		if (playerID >= 0 && playerID < 2) { // Assuming two players, adjust if needed
			playerData[playerID].score = score;
			playerData[playerID].name = nameReceived;
		}
	}

	cout << playerData[playerID].name << " (ID: " << playerID << "): " << playerData[playerID].score << ", ";
}

// Function to display scores at the top left of the window
void Client::displayScores(Font& font) {
	int yOffset = 10;

	for (int i = 0; i < 2; i++) {
		string scoreText = playerData[i].name + "'s Score: " + to_string(playerData[i].score);
		Text scoreDisplay = createText(scoreText, font, 24, Color::White, 10, yOffset + (i * 30));
		window->draw(scoreDisplay);
	}
}

// Local player (grey shape)
void Client::renderActualSelf(float x, float y) {
	CircleShape actualPlayerShape(CIRCLE_RADIUS);
	actualPlayerShape.setRadius(CIRCLE_RADIUS);
	actualPlayerShape.setFillColor(Color(128, 128, 128, 150));
	actualPlayerShape.setOutlineThickness(CIRCLE_BORDER);
	actualPlayerShape.setOutlineColor(Color(200, 200, 200, 150));
	actualPlayerShape.setPosition(x, y);
	window->draw(actualPlayerShape);
}

// Draw positions received from server for own and other players
void Client::renderReceivedShapes() {
	CircleShape otherPlayerShape(CIRCLE_RADIUS);
	otherPlayerShape.setFillColor(Color::Red);
	otherPlayerShape.setOutlineThickness(CIRCLE_BORDER);
	otherPlayerShape.setOutlineColor(Color(250, 150, 100));

	// Iterate through all positions and render only if playerID matches
	for (size_t i = 0; i < playerData.size(); ++i) {
		if (static_cast<int>(i) != playerID) {
			otherPlayerShape.setPosition(playerData[i].position.x, playerData[i].position.y);

			//cout << "New position set: " << otherPlayerShape.getPosition().x << ", " << otherPlayerShape.getPosition().y << endl;

			window->draw(otherPlayerShape);
		}
		else {
			renderPredictedSelf(playerData[i].position.x, playerData[i].position.y); // Draw self
		}
	}
}

// Render self predicted shape
void Client::renderPredictedSelf(float x, float y) {
	CircleShape predictedPlayerShape(CIRCLE_RADIUS);
	predictedPlayerShape.setRadius(CIRCLE_RADIUS);
	predictedPlayerShape.setFillColor(Color::Green);
	predictedPlayerShape.setOutlineThickness(CIRCLE_BORDER);
	predictedPlayerShape.setOutlineColor(Color(250, 150, 100));
	predictedPlayerShape.setPosition(x, y);
	window->draw(predictedPlayerShape);
}

// Draw the rainbow dots
void Client::drawRainbowBalls() {
	for (size_t i = 0; i < rainbowPositions.size(); ++i) {
		CircleShape rainbow(RAINBOW_RADIUS);
		rainbow.setPosition(rainbowPositions[i]);
		rainbow.setFillColor(rainbowColors[i]);
		window->draw(rainbow);
	}
}

/* Handle any errors */
void Client::handleErrors(Socket::Status status) {
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