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

// Function to attempt connecting to the server
bool connectToServer(TcpSocket& socket) {
	if (socket.connect(SERVER, PORT) != Socket::Done) {
		cout << "Failed to connect to server!" << endl;
		return false;
	}
	cout << "Connected to server." << endl;
	return true;
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

			// Handle mouse hover effect
			Vector2i mousePos = Mouse::getPosition(*window);
			bool playHovered = playButton.getGlobalBounds().contains(Vector2f(mousePos));
			bool quitHovered = quitButton.getGlobalBounds().contains(Vector2f(mousePos));

			// Change button color on hover
			if (playHovered) playButton.setFillColor(Color(70, 130, 180)); // SteelBlue when hovered
			else playButton.setFillColor(Color(100, 149, 237)); // Cornflower blue
			if (quitHovered)quitButton.setFillColor(Color(255, 69, 0)); // Red-Orange when hovered
			else quitButton.setFillColor(Color(178, 34, 34)); // Firebrick


			// Draw the main menu UI
			window->clear();
			window->draw(title);
			window->draw(playButton);
			window->draw(quitButton);
			window->draw(playText);
			window->draw(quitText);
			window->display();

			// Handle events (e.g., button clicks)
			Event event;
			while (window->pollEvent(event)) {
				if (event.type == Event::Closed) {
					window->close();
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
						return;
					}
				}
			}
		}
		else if (currentState == GameState::WaitingForPlayer) {
			displayWaitingMessage(*window);
			handleWaitingState(window, socket);
		}
		else if (currentState == GameState::Playing) {
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
	else if (socket.receive(packet) == Socket::NotReady) {
		// No data available, continue handling other tasks (like events, drawing)
		cout << "No data available from the server, still waiting...\n";
	}
	socket.setBlocking(true);  // Reset the socket to blocking mode when done
}

void startGame(RenderWindow*& window, TcpSocket& socket) {
	float spawnX, spawnY;
	if (!receiveInitialPosition(socket, spawnX, spawnY)) return;

	delete window;
	window = new RenderWindow(VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Corporate Life", Style::Default);
	window->setVerticalSyncEnabled(true);
	if (!window) {
		cerr << "Failed to create window for the game!" << endl;
		return;
	}

	setupPlayerShape(playerShape, spawnX, spawnY);
	cout << "Player spawned at (" << spawnX << ", " << spawnY << ")" << endl;

	gameLoop(*window, socket);
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

// Game loop where rendering happens
void gameLoop(RenderWindow& window, TcpSocket& socket) {
	while (window.isOpen()) {
		Event event;
		while (window.pollEvent(event)) {
			if (event.type == Event::Closed) {
				window.close();
				return;
			}
		}

		// Send player position to the server
		Packet packet;
		packet << playerShape.getPosition().x << playerShape.getPosition().y;
		cout << "Player Shape Sent: " << playerShape.getPosition().x << ", " << playerShape.getPosition().y << "\n";
		socket.send(packet);

		// Update the game state (positions of other players)
		receivePlayerPositions();

		receiveRainbowData(socket, rainbowPositions, rainbowColors);  // Get rainbow data

		window.clear();
		renderPlayers(window);
		drawRainbowBalls(window);
		window.display();
	}
}

// Receiving Player Positions
void receivePlayerPositions() {
	Packet packet;
	if (socket.receive(packet) == Socket::Done) {
		playerPositions.clear();
		float x, y;
		for (size_t i = 0; i < 2; ++i) {
			packet >> x >> y;
			cout << "Received Coordinates: " << x << ", " << y << "\n";
			playerPositions.push_back(Vector2f(x, y));
		}
	}
}

// Function to render all players, including local and remote
void renderPlayers(RenderWindow& window) {
	window.draw(playerShape);
	for (const auto& pos : playerPositions) {
		CircleShape otherPlayer(CIRCLE_RADIUS);
		otherPlayer.setPosition(pos);
		otherPlayer.setFillColor(Color::Green);
		window.draw(otherPlayer);
		cout << "Player drawn: " << otherPlayer.getPosition().x << ", " << otherPlayer.getPosition().y << "\n";
	}
}

void drawRainbowBalls(RenderWindow& window) {
	for (size_t i = 0; i < rainbowPositions.size(); ++i) {
		CircleShape rainbow(CIRCLE_RADIUS);
		rainbow.setPosition(rainbowPositions[i]);
		rainbow.setFillColor(rainbowColors[i]);
		window.draw(rainbow);
		cout << "Rainbow drawn: " << rainbow.getPosition().x << ", " << rainbow.getPosition().y << " - Colour: " << rainbow.getFillColor().r << ", " << rainbow.getFillColor().g << ", " << rainbow.getFillColor().b << "\n";
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
			cout << "Rainbow ball spawned at (" << x << ", " << y << ") with color ("
				<< static_cast<int>(r) << ", "
				<< static_cast<int>(g) << ", "
				<< static_cast<int>(b) << ").\n";
			//hasRainbowBall = true;
		}
		else if (command == "DESPAWN") {
			positions.clear();
			colors.clear();
			//hasRainbowBall = false;
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