#include "Client.h"

TcpSocket socket;
RenderWindow* window = nullptr; // Pointer to the game window
GameState currentState = GameState::MainMenu;

unordered_map<int, Player> players;

// Struct to hold data for each player on the client
struct PlayerData {
	Vector2f position;
	Vector2f lastPosition;
	float lerpTime;  // Time for interpolation
	float lastUpdateTime;
};
vector<PlayerData> allPlayerData(2);  // Array to store data for all players
int playerScores[2] = { 0, 0 };
vector<Vector2f> allPlayerPositions(2);
int playerID = -1;
unordered_map<int, string> playerNames; // Map to store player ID and names

CircleShape actualPlayerShape(CIRCLE_RADIUS);
CircleShape predictedPlayerShape(CIRCLE_RADIUS);
CircleShape otherPlayerShape(CIRCLE_RADIUS);

bool hasRainbowBall = false;
vector<Vector2f> rainbowPositions;
vector<Color> rainbowColors;

Clock sendTimer; // Add this clock at the beginning of the game loop or as a global variable.
float sendInterval = 0.1f; // Time interval in seconds (e.g., 100 ms)

string playerName;



// Connect to the server
bool connectToServer(TcpSocket& socket) {
	// Try to connect to the server
	if (socket.connect(SERVER, PORT) != Socket::Done) {
		cerr << "Error: Could not connect to server.\n";
		return false;
	}
	return true;
}

// Helper functions for creating UI elements
Text createText(const std::string& content, const Font& font, unsigned int size, const Color& color, float x, float y) {
	Text text(content, font, size);
	text.setFillColor(color);
	text.setPosition(x, y);
	return text;
}

RectangleShape createButton(float width, float height, const Color& color, float x, float y) {
	RectangleShape button(Vector2f(width, height));
	button.setFillColor(color);
	button.setPosition(x, y);
	return button;
}

// Create and display the main menu
void createMainMenu(RenderWindow*& window, TcpSocket& socket) {

	bool showError = false;
	bool isEditingName = false;

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

			// Dynamically position UI elements
			float centerX = window->getSize().x / 2;
			float buttonWidth = 200.f;
			float buttonHeight = 50.f;

			// Create UI elements
			Text title = createText("Corporate Life", font, 50, Color(255, 215, 0), centerX - 120, 50);
			title.setStyle(Text::Bold | Text::Underlined);

			RectangleShape playButton = createButton(buttonWidth, buttonHeight, Color(100, 149, 237), centerX - buttonWidth / 2, 150);
			RectangleShape quitButton = createButton(buttonWidth, buttonHeight, Color(178, 34, 34), centerX - buttonWidth / 2, 250);

			Text playText = createText("Play", font, 24, Color::White, playButton.getPosition().x + 75, playButton.getPosition().y + 10);
			Text quitText = createText("Quit", font, 24, Color::White, quitButton.getPosition().x + 75, quitButton.getPosition().y + 10);

			Text prompt = createText("Enter your name:", font, 24, Color::White, 50, 350);
			Text playerNameText = createText(playerName.empty() ? "Click to enter name" : playerName, font, 24, isEditingName ? Color::White : Color::Cyan, 250, 350);

			Text errorText = createText("Name cannot be empty!", font, 20, Color::Red, 50, 400);

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

						if (playerName.empty()) {
							showError = true;
						}
						else {
							showError = false;

							if (socket.getRemoteAddress() == IpAddress::None && !connectToServer(socket)) {
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
					else if (playerNameText.getGlobalBounds().contains(mousePosition)) {
						isEditingName = true;
						playerName.clear();
					}
					else {
						isEditingName = false;
					}
				}

				if (event.type == Event::TextEntered && isEditingName) {
					if (event.text.unicode == '\b') { // Handle backspace
						if (!playerName.empty()) {
							playerName.pop_back();
						}
					}
					else if (event.text.unicode == '\r') { // Handle Enter key
						isEditingName = false;
					}
					else if (event.text.unicode < 128) { // Append ASCII characters
						playerName += static_cast<char>(event.text.unicode);
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
			window->draw(prompt);
			window->draw(playerNameText);
			if (showError) {
				window->draw(errorText);
			}
			window->display();
		}
		else if (currentState == GameState::LobbyFull) {
			// Lobby Full Screen Logic
			// Create a screen informing the user the lobby is full

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
			displayWaitingMessage(*window);
			handleWaitingState(window, socket);
		}
		else if (currentState == GameState::Playing) {
			// Close the main menu window and prepare for the next state
			window->close();
			delete window;
			window = nullptr;

			startGame(window, socket);
			break;
		}
	}
}

void handleNameInput(Event& event) {
	if (event.type == Event::TextEntered) {
		if (event.text.unicode == '\b') { // Backspace handling
			if (!playerName.empty())
				playerName.pop_back();
		}
		else if (event.text.unicode == '\r') { // Enter key to confirm
			cout << "Name entered: " << playerName << "\n";
		}
		else if (playerName.length() < 15) { // Limit to 15 characters
			playerName += static_cast<char>(event.text.unicode);
		}
	}
}

// Function to display scores at the top left of the window
void displayScores(RenderWindow& window, Font& font) {
	int yOffset = 10; // Vertical spacing between scores
	int index = 0;

	// Iterate over the unordered_map using iterators
	for (unordered_map<int, string>::iterator it = playerNames.begin(); it != playerNames.end(); ++it) {
		int id = it->first;  // Player ID
		string name = it->second;  // Player Name

		// Ensure playerScores[id] exists even if it is 0
		string scoreText = name + "'s Score: " + to_string(playerScores[id]); // Display 0 if the score is 0
		Text scoreDisplay = createText(scoreText, font, 24, Color::White, 10, yOffset + (index * 30));
		window.draw(scoreDisplay);

		index++;
	}
}

void displayWaitingMessage(RenderWindow& window) {
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
	waitingText.setPosition(window.getSize().x / 2 - waitingText.getGlobalBounds().width / 2,
		window.getSize().y / 2 - waitingText.getGlobalBounds().height / 2); // Center the text

	// Draw text on window
	window.clear();
	window.draw(waitingText);
	window.display();
}

void handleWaitingState(RenderWindow* window, TcpSocket& socket) {
	Event event;
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


// Function to receive the initial position of the player from the server
bool receiveInitialPosition(TcpSocket& socket, float& x, float& y) {
	Packet positionPacket;
	if (socket.receive(positionPacket) == Socket::Done) {
		string command;
		positionPacket >> command;

		if (command == "PLAYER_POSITIONS") {
			positionPacket >> playerID >> x >> y; // Now we extract the position after confirming the command
			return true;
		}
		else {
			cout << "Received unexpected command: " << command << endl;
			return false;
		}
	}
	else {
		cout << "There was an error getting the initial spawn positions.";
		return false;
	}
}

void startGame(RenderWindow*& window, TcpSocket& socket) {
	// Create a ghost circle for actual movements
	float spawnX, spawnY;
	if (!receiveInitialPosition(socket, spawnX, spawnY)) return;
	actualPlayerShape.setPosition(spawnX, spawnY);

	// Setup window
	window = new RenderWindow(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Corporate Life", Style::Default);
	window->setVerticalSyncEnabled(true);
	if (!window) {
		cerr << "Failed to create window for the game!" << endl;
		return;
	}

	gameLoop(*window, socket);
}


// Main game loop for rendering and handling player movement
void gameLoop(RenderWindow& window, TcpSocket& socket) {
	window.setFramerateLimit(60);  // Cap the framerate

	// Create a player shape and set its initial position
	Vector2f targetPos; // Target position based on mouse
	float moveSpeed = 400.f; // Speed of movement in pixels per second
	const float deadZoneRadius = 5.f; // Dead zone radius to prevent jittering

	Clock clock; // For frame-based timing

	while (window.isOpen()) {
		float deltaTime = clock.restart().asSeconds(); // Time since last frame

		Font font;
		if (!font.loadFromFile("res/comic.ttf")) {
			cerr << "Failed to load font!" << endl;
			return;
		}

		Event event;
		while (window.pollEvent(event)) {
			if (event.type == Event::Closed) {
				window.close();
				return;
			}
		}

		// Update target position to the mouse location
		targetPos = window.mapPixelToCoords(Mouse::getPosition(window)); //convert the pixel coordinates from the mouse to world coordinates, especially if the view has been transformed

		// Get the current position of the player
		Vector2f currentPos = actualPlayerShape.getPosition();
		Vector2f movementVector;

		// Compute direction vector and normalize it
		Vector2f direction = targetPos - currentPos;
		float distance = sqrt(direction.x * direction.x + direction.y * direction.y);

		// Only move if outside the dead zone
		if (distance > deadZoneRadius) {
			Vector2f normalizedDirection = direction / distance; // Normalize the direction vector
			Vector2f movement = normalizedDirection * moveSpeed * deltaTime;
			actualPlayerShape.move(movement);
		}

		// Clamp the player's position within the window bounds
		Vector2f position = actualPlayerShape.getPosition();
		float radius = actualPlayerShape.getRadius();

		position.x = max(0.f, min(position.x, window.getSize().x - 2 * radius));
		position.y = max(0.f, min(position.y, window.getSize().y - 2 * radius));
		actualPlayerShape.setPosition(position);
		sendPlayerPosition(socket, actualPlayerShape, movementVector); // Send the updated position of the player to the server.


		// Receive packet from server. Check what command it is and then call that function.
		Packet receivedPacket;
		auto status = socket.receive(receivedPacket);
		if (status == Socket::Done) {
			string command;
			receivedPacket >> command;

			if (command == "PLAYER_POSITIONS") receivePlayerPositions(socket, allPlayerPositions, receivedPacket);
			else if (command == "SPAWN") receiveRainbowData(rainbowPositions, rainbowColors, receivedPacket);
			else if (command == "DESPAWN") deleteRainbowData(rainbowPositions, rainbowColors);
			else if (command == "UPDATE_SCORES") updateScores(receivedPacket);
			else cerr << "Error receiving data from socket: " << status << endl;
		}


		// Clear the window and render everything
		window.clear();
		displayScores(window, font);
		renderActualSelf(window, actualPlayerShape.getPosition().x, actualPlayerShape.getPosition().y);
		renderReceivedShapes(window, allPlayerPositions); // Draw all the players (self and opponent)
		drawRainbowBalls(window); // Draw the rainbow ball
		window.display();
	}
}

void sendPlayerPosition(TcpSocket& socket, const CircleShape& actualPlayerShape, Vector2f movementVector) {
	Packet packet;
	packet << "UPDATE_POSITION" << actualPlayerShape.getPosition().x << actualPlayerShape.getPosition().y << movementVector.x << movementVector.y;
	if (socket.send(packet) != Socket::Done) {
		cerr << "Failed to send player position to the server.\n";
	}
}

void receivePlayerPositions(TcpSocket& socket, vector<Vector2f>& allPlayerPositions, Packet packet) {
	Clock localClock;

	for (size_t i = 0; i < allPlayerPositions.size(); ++i) {
		float x, y;
		int id;
		packet >> id >> x >> y;

		// Interpolation Strategy:
		// If it's a new position, interpolate towards it
		if (id != playerID) {
			PlayerData& player = allPlayerData[id];
			float currentTime = localClock.getElapsedTime().asSeconds();

			// Interpolate from the last position to the new position based on time elapsed
			float deltaTime = currentTime - player.lastUpdateTime;
			player.lerpTime = min(deltaTime, 0.1f);  // Limit max time for interpolation

			// Store the new position and update time
			player.lastPosition = player.position;
			player.position = { x, y };
			player.lastUpdateTime = currentTime;
		}
		allPlayerPositions[id] = (Vector2f(x, y));
	}
}

void receiveRainbowData(vector<Vector2f>& positions, vector<Color>& colors, Packet packet) {
	float x, y, spawnTime;
	Uint8 r, g, b;
	packet >> x >> y >> r >> g >> b >> spawnTime;
	positions.push_back(Vector2f(x, y));
	colors.push_back(Color(r, g, b));
	cout << "Rainbow received: " << x << ", " << y << " with spawn time: " << spawnTime << " seconds.\n";
}

// Function to update the scores received from the server
void updateScores(Packet& packet) {
	int playerID, score;
	string nameReceived;
	memset(playerScores, 0, sizeof(playerScores));

	// Decode the scores from the packet
	while (!packet.endOfPacket()) {
		packet >> playerID >> nameReceived >> score;
		if (playerID >= 0 && playerID < 2) { // Assuming two players, adjust if needed
			playerScores[playerID] = score;
			playerNames[playerID] = nameReceived;
		}
	}
	cout << playerNames[playerID] << " (ID: " << playerID << "): " << playerScores[playerID] << ", ";
}

void deleteRainbowData(vector<Vector2f>& positions, vector<Color>& colors) {
	positions.clear();
	colors.clear();
	cout << "Cleared rainbows\n";
}



// Local player
void renderActualSelf(RenderWindow& window, float x, float y) {
	// Set up the player's shape with the given position
	actualPlayerShape.setRadius(CIRCLE_RADIUS);
	actualPlayerShape.setFillColor(Color(128, 128, 128, 150));
	actualPlayerShape.setOutlineThickness(CIRCLE_BORDER);
	actualPlayerShape.setOutlineColor(Color(200, 200, 200, 150));
	actualPlayerShape.setPosition(x, y);
	window.draw(actualPlayerShape);
}

void renderPredictedSelf(RenderWindow& window, float x, float y) {
	// Set up the player's shape with the given position
	predictedPlayerShape.setRadius(CIRCLE_RADIUS);
	predictedPlayerShape.setFillColor(Color::Green);
	predictedPlayerShape.setOutlineThickness(CIRCLE_BORDER);
	predictedPlayerShape.setOutlineColor(Color(250, 150, 100));
	predictedPlayerShape.setPosition(x, y);
	window.draw(predictedPlayerShape);
}

// Draw positions received from server for own and other players
void renderReceivedShapes(RenderWindow& window, const vector<Vector2f>& allPlayerPositions) {
	otherPlayerShape.setFillColor(Color::Green);
	otherPlayerShape.setOutlineThickness(CIRCLE_BORDER);
	otherPlayerShape.setOutlineColor(Color(250, 150, 100));

	// Iterate through all positions and render only if playerID matches
	for (size_t i = 0; i < allPlayerPositions.size(); ++i) {
		if (static_cast<int>(i) != playerID) {
			PlayerData& player = allPlayerData[i];

			// Smoothly interpolate the player position based on time
			float t = player.lerpTime;
			Vector2f interpolatedPosition = player.lastPosition + t * (player.position - player.lastPosition);

			otherPlayerShape.setPosition(interpolatedPosition);
			window.draw(otherPlayerShape); // Draw opponent player
		}
		else {
			renderPredictedSelf(window, allPlayerPositions[i].x, allPlayerPositions[i].y); // Draw self
		}
	}
}

void drawRainbowBalls(RenderWindow& window) {
	for (size_t i = 0; i < rainbowPositions.size(); ++i) {
		CircleShape rainbow(RAINBOW_RADIUS);
		rainbow.setPosition(rainbowPositions[i]);
		rainbow.setFillColor(rainbowColors[i]);
		window.draw(rainbow);
	}
}