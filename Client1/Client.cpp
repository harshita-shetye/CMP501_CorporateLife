#include "Client.h"

// Global variables
bool hasRainbowBall = false;
RenderWindow* window = nullptr; // Pointer to the game window
TcpSocket socket;
CircleShape playerShape(CIRCLE_RADIUS);
vector<Vector2f> playerPositions;
vector<Vector2f> rainbowPositions;
vector<Color> rainbowColors;
GameState currentState;
unordered_map<int, Player> players;
int playerID = -1;  // We'll assign a player ID when connecting to the server

// Function to attempt connecting to the server
bool connectToServer(TcpSocket& socket) {
	// Try to connect to the server
	if (socket.connect(SERVER, PORT) != Socket::Done) {
		std::cerr << "Error: Could not connect to server.\n";
		return false;
	}
	return true;
}

// Function to setup the player's appearance and position
void setupPlayerShape(CircleShape& playerShape, float x, float y) {
	// Set up the player's shape with the given position
	playerShape.setRadius(CIRCLE_RADIUS);
	playerShape.setFillColor(Color::Green);
	playerShape.setOutlineThickness(CIRCLE_BORDER);
	playerShape.setOutlineColor(Color(250, 150, 100));
	playerShape.setPosition(x, y);
}

void createMainMenu(RenderWindow*& window, TcpSocket& socket) {
	// Main Menu Loop
	while (true) { // Infinite loop controlled by states
		if (currentState == GameState::MainMenu) {
			// Check if the window is already created or create a new one
			if (!window) {
				window = new RenderWindow(VideoMode(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), "Main Menu", Style::Close);
				window->setVerticalSyncEnabled(true);
			}

			// Load font for the UI
			Font font;
			if (!font.loadFromFile("res/comic.ttf")) {
				cerr << "Failed to load font!" << endl;
				return;
			}

			// Title
			Text title("Corporate Life", font, 50);
			title.setFillColor(Color(255, 215, 0)); // Gold color
			title.setStyle(Text::Bold | Text::Underlined);
			title.setPosition(window->getSize().x / 2 - title.getGlobalBounds().width / 2, 50);

			// Buttons
			RectangleShape playButton(Vector2f(200, 50));
			playButton.setPosition(window->getSize().x / 2 - 100, 150);
			playButton.setFillColor(Color(100, 149, 237)); // Cornflower blue

			RectangleShape quitButton(Vector2f(200, 50));
			quitButton.setPosition(window->getSize().x / 2 - 100, 250);
			quitButton.setFillColor(Color(178, 34, 34)); // Firebrick

			Text playText("Play", font, 24);
			playText.setFillColor(Color::White);
			playText.setPosition(playButton.getPosition().x + playButton.getSize().x / 2 - playText.getGlobalBounds().width / 2,
				playButton.getPosition().y + playButton.getSize().y / 2 - playText.getGlobalBounds().height / 2);

			Text quitText("Quit", font, 24);
			quitText.setFillColor(Color::White);
			quitText.setPosition(quitButton.getPosition().x + quitButton.getSize().x / 2 - quitText.getGlobalBounds().width / 2,
				quitButton.getPosition().y + quitButton.getSize().y / 2 - quitText.getGlobalBounds().height / 2);



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
						if (socket.getRemoteAddress() == sf::IpAddress::None && !connectToServer(socket)) {
							cerr << "Failed to connect to server!" << endl;
							continue;
						}

						// Once connected, receive a message from the server
						Packet packet;
						if (socket.receive(packet) == Socket::Done) {
							string message;
							packet >> message;

							currentState = (message == "Waiting for another player to connect...")
								? GameState::WaitingForPlayer
								: GameState::Playing;
							break;
						}
					}
					else if (quitButton.getGlobalBounds().contains(mousePosition)) {
						window->close();
						delete window;
						window = nullptr;
						return;
					}
				}
			}


			// Draw the main menu UI
			window->clear();
			window->draw(title);
			window->draw(playButton);
			window->draw(quitButton);
			window->draw(playText);
			window->draw(quitText);
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

void displayWaitingMessage(RenderWindow& window) {
	// Create font
	Font font;
	if (!font.loadFromFile("res/comic.ttf")) { // Specify the correct font path
		std::cerr << "Error loading font!" << std::endl;
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
		positionPacket >> x >> y;
		cout << "Received initial spawn position: (" << x << ", " << y << ")" << endl;
		return true;
	}
	else {
		cout << "Failed to receive initial position!" << endl;
		return false;
	}
}

void startGame(RenderWindow*& window, TcpSocket& socket) {
	// Initial Setup
	float spawnX, spawnY;
	if (!receiveInitialPosition(socket, spawnX, spawnY)) return;

	// Setup window
	window = new RenderWindow(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Corporate Life", Style::Default);
	window->setVerticalSyncEnabled(true);

	if (!window) {
		cerr << "Failed to create window for the game!" << endl;
		return;
	}

	// Setup player shape with initial position from server
	setupPlayerShape(playerShape, spawnX, spawnY);
	cout << "Player spawned at (" << spawnX << ", " << spawnY << ")" << endl;

	// Start game loop
	gameLoop(*window, socket);
}

// Main game loop for rendering and handling player movement
void gameLoop(RenderWindow& window, TcpSocket& socket) {
	window.setFramerateLimit(60);  // Cap the framerate

	// Create a player shape and set its initial position
	Vector2f targetPos; // Target position based on mouse
	float moveSpeed = 200.f; // Speed of movement in pixels per second
	setupPlayerShape(playerShape, 100.f, 100.f); // Initial position

	// Send initial player position to server
	sf::Packet initPacket;
	initPacket << playerShape.getPosition().x << playerShape.getPosition().y;
	socket.send(initPacket);

	Clock clock; // For frame-based timing

	while (window.isOpen()) {
		float deltaTime = clock.restart().asSeconds(); // Time since last frame

		Event event;
		while (window.pollEvent(event)) {
			if (event.type == Event::Closed) {
				window.close();
				return;
			}
		}

		// Update target position to the mouse location
		targetPos = Vector2f(Mouse::getPosition(window));

		// Get the current position of the player
		Vector2f currentPos = playerShape.getPosition();

		// Compute direction vector and normalize it
		Vector2f direction = targetPos - currentPos;
		float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		if (length > 0.1f) { // Prevent jittering when close to the target
			direction /= length;

			// Move the player towards the target position
			playerShape.move(direction * moveSpeed * deltaTime);
		}

		// Clamp the player's position within the window bounds
		Vector2f position = playerShape.getPosition();
		float radius = playerShape.getRadius();

		position.x = std::max(0.f, std::min(position.x, window.getSize().x - 2 * radius));
		position.y = std::max(0.f, std::min(position.y, window.getSize().y - 2 * radius));
		playerShape.setPosition(position);

		// Send updated player position to the server
		sf::Packet packet;
		packet << playerShape.getPosition().x << playerShape.getPosition().y;
		socket.send(packet);

		// Receive player positions and rainbow data from the server
		receivePlayerPositions(socket);
		receiveRainbowData(socket, rainbowPositions, rainbowColors);

		// Clear the window and render everything
		window.clear();
		renderPlayers(window); // Render all players (local and remote)
		drawRainbowBalls(window); // Draw the rainbow balls (if applicable)
		window.display();
	}
}

// Function to receive player positions from the server
void receivePlayerPositions(TcpSocket& socket) {
	sf::Packet packet;
	if (socket.receive(packet) == sf::Socket::Done) {
		playerPositions.clear();  // Clear the previous positions
		float x, y;
		for (size_t i = 0; i < 2; ++i) {  // Assuming 2 players, adjust as needed
			packet >> x >> y;
			playerPositions.push_back(Vector2f(x, y));
		}
	}
}

// Function to render all players, including local and remote
void renderPlayers(RenderWindow& window) {
	// Draw the local player (green)
	window.draw(playerShape);

	// Draw other players (remote)
	for (const auto& pos : playerPositions) {
		CircleShape otherPlayer(CIRCLE_RADIUS);
		otherPlayer.setPosition(pos);
		otherPlayer.setFillColor(Color::Red);  // Use a different color for other players
		window.draw(otherPlayer);
	}
}

void drawRainbowBalls(RenderWindow& window) {
	for (size_t i = 0; i < rainbowPositions.size(); ++i) {
		CircleShape rainbow(CIRCLE_RADIUS);
		rainbow.setPosition(rainbowPositions[i]);
		rainbow.setFillColor(rainbowColors[i]);
		window.draw(rainbow);
	}
}

void receiveRainbowData(TcpSocket& socket, vector<Vector2f>& positions, vector<Color>& colors) {
	Packet receivedPacket;
	Socket::Status status = socket.receive(receivedPacket);

	if (status == Socket::Done) {
		string command;
		receivedPacket >> command;

		if (command == "SPAWN") {
			float x, y;
			Uint8 r, g, b;
			receivedPacket >> x >> y >> r >> g >> b;
			positions.push_back(Vector2f(x, y));
			colors.push_back(Color(r, g, b));
		}
		else if (command == "DESPAWN") {
			positions.clear();
			colors.clear();
		}
	}
	else {
		cerr << "Error receiving data from socket: " << status << endl;
	}
}