#include "Client.h"

bool hasRainbowBall = false; // Flag to track if a rainbow ball is present
GameState currentState = GameState::MainMenu;

void createMainMenu(RenderWindow*& window, TcpSocket& socket, CircleShape& playerShape, vector<Vector2f>& rainbowPositions, vector<Color>& rainbowColors) {
	// Main Menu Loop
	while (true) { // Infinite loop controlled by states
		if (currentState == GameState::MainMenu) {
			RenderWindow menuWindow(VideoMode(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), "Main Menu", Style::Close);
			Font font;
			if (!font.loadFromFile("res/comic.ttf")) {
				cerr << "Failed to load font!" << endl;
				return;
			}

			// Title
			Text title("Corporate Life", font, 50);
			title.setFillColor(Color(255, 215, 0)); // Gold color
			title.setStyle(Text::Bold | Text::Underlined);
			title.setPosition(menuWindow.getSize().x / 2 - title.getGlobalBounds().width / 2, 50);

			// Buttons
			RectangleShape playButton(Vector2f(200, 50));
			playButton.setPosition(menuWindow.getSize().x / 2 - 100, 150);
			playButton.setFillColor(Color(100, 149, 237)); // Cornflower blue

			RectangleShape quitButton(Vector2f(200, 50));
			quitButton.setPosition(menuWindow.getSize().x / 2 - 100, 250);
			quitButton.setFillColor(Color(178, 34, 34)); // Firebrick

			Text playText("Play", font, 24);
			playText.setFillColor(Color::White);
			playText.setPosition(playButton.getPosition().x + playButton.getSize().x / 2 - playText.getGlobalBounds().width / 2,
				playButton.getPosition().y + playButton.getSize().y / 2 - playText.getGlobalBounds().height / 2);

			Text quitText("Quit", font, 24);
			quitText.setFillColor(Color::White);
			quitText.setPosition(quitButton.getPosition().x + quitButton.getSize().x / 2 - quitText.getGlobalBounds().width / 2,
				quitButton.getPosition().y + quitButton.getSize().y / 2 - quitText.getGlobalBounds().height / 2);

			// Background
			RectangleShape background(Vector2f(menuWindow.getSize().x, menuWindow.getSize().y));
			background.setFillColor(Color(50, 50, 50)); // Dark gray

			while (menuWindow.isOpen()) {
				Event event;
				while (menuWindow.pollEvent(event)) {
					if (event.type == Event::Closed) {
						menuWindow.close();
						return;
					}
					else if (event.type == Event::MouseMoved) {
						Vector2f mousePos = menuWindow.mapPixelToCoords(Mouse::getPosition(menuWindow));
						playButton.setFillColor(playButton.getGlobalBounds().contains(mousePos) ? Color::Blue : Color(100, 149, 237));
						quitButton.setFillColor(quitButton.getGlobalBounds().contains(mousePos) ? Color::Red : Color(178, 34, 34));
					}
					else if (event.type == Event::MouseButtonPressed) {
						Vector2f mousePos = menuWindow.mapPixelToCoords(Mouse::getPosition(menuWindow));
						if (playButton.getGlobalBounds().contains(mousePos)) {
							currentState = GameState::Playing;
							menuWindow.close();
						}
						else if (quitButton.getGlobalBounds().contains(mousePos)) {
							menuWindow.close();
							return;
						}
					}
				}

				menuWindow.clear();
				menuWindow.draw(background);
				menuWindow.draw(title);
				menuWindow.draw(playButton);
				menuWindow.draw(quitButton);
				menuWindow.draw(playText);
				menuWindow.draw(quitText);
				menuWindow.display();
			}
		}

		// Proceed to Game Loop if Playing
		if (currentState == GameState::Playing) {
			if (!window) { // Only create the game window once
				window = new RenderWindow(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Corporate Life");
				window->setVerticalSyncEnabled(true);
			}

			if (!connectToServer(socket)) return;

			float spawnPosX = 0, spawnPosY = 0;
			if (!receiveInitialPosition(socket, spawnPosX, spawnPosY)) return;

			setupPlayerShape(playerShape, spawnPosX, spawnPosY);

			// Game loop
			bool isGameRunning = true;
			while (isGameRunning) {
				Event event;
				while (window->pollEvent(event)) {
					if (event.type == Event::Closed) {
						// Handle window close event
						isGameRunning = false;
						currentState = GameState::MainMenu; // Return to main menu
						socket.disconnect(); // Disconnect from the server
					}
				}

				// Game update logic (e.g., player movement, rendering, etc.)
				gameLoop(*window, socket, playerShape, rainbowPositions, rainbowColors);

				if (!isGameRunning) break;
			}

			// Clean up after game loop ends
			delete window;
			window = nullptr;
		}
	}
}

// Function to run the TCP client logic
void runTcpClient(unsigned short PORT) {
	TcpSocket socket;

	// Attempt to connect to the server
	if (!connectToServer(socket)) return;

	// Receive the initial spawn position for the player
	float spawnPosX = 0, spawnPosY = 0;
	if (!receiveInitialPosition(socket, spawnPosX, spawnPosY)) return;

	// Setup the game window and player shape
	RenderWindow window(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Corporate Life", Style::Default);
	window.setVerticalSyncEnabled(true);
	CircleShape playerShape(CIRCLE_RADIUS);
	setupPlayerShape(playerShape, spawnPosX, spawnPosY);

	// Containers to store rainbow positions and colors
	vector<Vector2f> rainbowPositions;
	vector<Color> rainbowColors;

	// Enter the main game loop
	gameLoop(window, socket, playerShape, rainbowPositions, rainbowColors);
}

// Function to attempt connecting to the server
bool connectToServer(TcpSocket& socket) {
	if (socket.connect(SERVER, PORT) != Socket::Done) {
		cout << "Failed to connect to server!" << endl;
		return false;
	}
	cout << "Connected to server." << endl;
	return true;
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

// Function to setup the player's appearance and position
void setupPlayerShape(CircleShape& playerShape, float x, float y) {
	playerShape.setFillColor(Color(100, 250, 50));
	playerShape.setOutlineThickness(CIRCLE_BORDER);
	playerShape.setOutlineColor(Color(250, 150, 100));
	playerShape.setPosition(x, y);
}

// Main game loop function
void gameLoop(RenderWindow& window, TcpSocket& socket, CircleShape& playerShape, vector<Vector2f>& rainbowPositions, vector<Color>& rainbowColors) {
	const float movementSpeed = 300.0f; // Player movement speed
	Clock clock; // Clock to track time between frames

	while (window.isOpen()) {
		float deltaTime = clock.restart().asSeconds(); // Time elapsed since last frame

		// Event handling
		Event event;
		while (window.pollEvent(event)) {
			if (event.type == Event::Closed) {
				// Notify server about disconnection
				Packet disconnectPacket;
				disconnectPacket << "DISCONNECT"; // Custom message to indicate player leaving
				if (socket.send(disconnectPacket) != Socket::Done) {
					cerr << "Failed to notify server about disconnection." << endl;
				}
				socket.disconnect(); // Close the socket connection
				window.close(); // Close the game window
				return; // Exit the game loop
			}
		}

		// Receive rainbow ball data from the server
		receiveRainbowData(socket, rainbowPositions, rainbowColors);

		// Check for collisions with rainbow balls
		for (size_t i = 0; i < rainbowPositions.size(); ++i) {
			float distance = std::hypot(rainbowPositions[i].x - playerShape.getPosition().x, rainbowPositions[i].y - playerShape.getPosition().y);
			if (distance < CIRCLE_RADIUS + RAINBOW_RADIUS) {
				// Notify the server to despawn the rainbow ball
				Packet despawnPacket;
				despawnPacket << -1.f << -1.f << 0 << 0 << 0;  // Special despawn packet
				socket.send(despawnPacket);

				// Clear the rainbow balls from the client-side list
				rainbowPositions.clear();
				rainbowColors.clear();
				break;
			}
		}

		// Get the mouse position relative to the window
		Vector2f mousePos = Vector2f(Mouse::getPosition(window));

		// Calculate direction vector toward the mouse position
		Vector2f direction = mousePos - playerShape.getPosition();
		float distance = std::hypot(direction.x, direction.y);

		// Move the player shape toward the mouse
		if (distance > 0.1f) {
			direction /= distance; // Normalize direction
			Vector2f newPosition = playerShape.getPosition() + direction * movementSpeed * deltaTime;

			// Enforce window boundaries
			newPosition.x = std::max(CIRCLE_RADIUS, std::min(WINDOW_WIDTH - CIRCLE_RADIUS, newPosition.x));
			newPosition.y = std::max(CIRCLE_RADIUS, std::min(WINDOW_HEIGHT - CIRCLE_RADIUS, newPosition.y));

			playerShape.setPosition(newPosition);
		}

		// Clear the window and draw the updated scene
		window.clear(Color::Black);
		drawScene(window, playerShape, rainbowPositions, rainbowColors);
		window.display();
	}
}

void receiveRainbowData(TcpSocket& socket, vector<Vector2f>& positions, vector<Color>& colors) {
	Packet receivedPacket;
	Socket::Status status = socket.receive(receivedPacket);

	/*while (!receivedPacket.endOfPacket()) {
		int id;
		float x, y;
		packet >> id >> x >> y;
		players[id].targetPosition = { x, y };
	}*/


	if (status == Socket::Done) {
		string command;
		receivedPacket >> command;

		if (command == "SPAWN") {
			float x, y;
			Uint8 r, g, b;
			receivedPacket >> x >> y >> r >> g >> b;
			positions.push_back(Vector2f(x, y));
			colors.push_back(Color(r, g, b));
			cout << "Rainbow ball spawned at (" << x << ", " << y << ") with color ("
				<< static_cast<int>(r) << ", "
				<< static_cast<int>(g) << ", "
				<< static_cast<int>(b) << ").\n";
			hasRainbowBall = true;
		}
		else if (command == "DESPAWN") {
			positions.clear();
			colors.clear();
			hasRainbowBall = false;
			cout << "Rainbow ball despawned.\n";
		}
	}
	else if (status == Socket::NotReady) {
		cout << "No data available on socket.\n";
	}
	else {
		cerr << "Error receiving data from socket: " << status << endl;
	}
}

// Function to draw the player and all rainbow balls in the scene
void drawScene(RenderWindow& window, CircleShape& playerShape, const vector<Vector2f>& rainbowPositions, const vector<Color>& rainbowColors) {
	// Draw the player
	window.draw(playerShape);

	// Draw all rainbow balls
	CircleShape rainbowShape(RAINBOW_RADIUS);
	for (size_t i = 0; i < rainbowPositions.size(); ++i) {
		rainbowShape.setPosition(rainbowPositions[i]);
		rainbowShape.setFillColor(rainbowColors[i]);
		window.draw(rainbowShape);
	}
}

// Provides error messages based on socket status
static void handleErrors(Socket::Status status) {
	switch (status) {
	case Socket::NotReady:
		cout << "The socket is not ready to send/receive data yet." << endl;
		break;
	case Socket::Partial:
		cout << "The socket sent a part of the data." << endl;
		break;
	case Socket::Disconnected:
		cout << "The TCP socket has been disconnected." << endl;
		break;
	case Socket::Error:
		cout << "An unexpected error happened." << endl;
		break;
	default:
		cout << "Unknown socket status." << endl;
		break;
	}
}