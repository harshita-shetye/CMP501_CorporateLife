#include "Client.h"

bool hasRainbowBall = false; // Flag to track if a rainbow ball is present
GameState currentState = GameState::MainMenu;
vector<Vector2f> playerPositions; // Store positions of players

void createMainMenu(RenderWindow*& window, TcpSocket& socket, CircleShape& playerShape, vector<Vector2f>& rainbowPositions, vector<Color>& rainbowColors) {
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
					if (playButton.getGlobalBounds().contains(Vector2f(event.mouseButton.x, event.mouseButton.y))) {
						// Attempt to connect to the server here
						if (socket.getRemoteAddress() == sf::IpAddress::None) {
							if (!connectToServer(socket)) {
								cerr << "Failed to connect to server!" << endl;
								continue; // Retry if connection fails
							}
						}

						// Once connected, receive a message from the server
						Packet packet;
						if (socket.receive(packet) == Socket::Done) {
							string message;
							packet >> message;

							if (message == "Waiting for another player to connect...") {
								currentState = GameState::WaitingForPlayer;  // Change to Waiting state
								break;  // Exit the menu loop
							}
							else if (message == "Both players connected. Starting the game!") {
								currentState = GameState::Playing; // Transition to playing
								break;  // Exit the menu loop and start the game loop
							}
						}
					}

					// Check if Quit button was clicked
					else if (quitButton.getGlobalBounds().contains(Vector2f(event.mouseButton.x, event.mouseButton.y))) {
						// Close the window when Quit button is clicked
						window->close();
						return;
					}
				}
			}
		}

		if (currentState == GameState::Playing) {
			// Handle game state logic and transition here (starting game)
			break; // Exit the menu loop and start the game
		}

		if (currentState == GameState::WaitingForPlayer) {
			// Handle waiting state UI
			displayWaitingMessage(*window);  // Show waiting message for second player
		}
	}
}

void displayMainMenu(RenderWindow& window) {
	// Create font
	Font font;
	if (!font.loadFromFile("path/to/font.ttf")) { // Specify the correct font path
		std::cerr << "Error loading font!" << std::endl;
		return;
	}

	// Create text for main menu
	Text mainMenuText;
	mainMenuText.setFont(font);
	mainMenuText.setString("Welcome to the Game!\nClick Play to start.");
	mainMenuText.setCharacterSize(30);
	mainMenuText.setFillColor(Color::White);
	mainMenuText.setPosition(window.getSize().x / 2 - mainMenuText.getGlobalBounds().width / 2, window.getSize().y / 2 - mainMenuText.getGlobalBounds().height / 2);

	// Draw text on window
	window.clear();
	window.draw(mainMenuText);
	window.display();
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
	waitingText.setCharacterSize(30); // Size of the text
	waitingText.setFillColor(Color::White); // Text color
	waitingText.setPosition(window.getSize().x / 2 - waitingText.getGlobalBounds().width / 2,
		window.getSize().y / 2 - waitingText.getGlobalBounds().height / 2); // Center the text

	// Draw text on window
	window.clear();
	window.draw(waitingText);
	window.display();
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

// Function to receive player positions from the server
bool receivePlayerPositions(TcpSocket& socket) {
	Packet packet;
	if (socket.receive(packet) != Socket::Done) {
		return false;
	}

	// Clear previous player positions
	playerPositions.clear();

	// Extract player positions from the received packet
	float x, y;
	while (packet >> x >> y) {
		playerPositions.push_back(Vector2f(x, y));
	}
	return true;
}

// Function to render all players, including local and remote
void renderPlayers(RenderWindow& window, CircleShape& playerShape) {
	// Render local player
	window.draw(playerShape);

	// Render remote players
	for (const auto& pos : playerPositions) {
		CircleShape otherPlayerShape(CIRCLE_RADIUS);
		otherPlayerShape.setPosition(pos);
		otherPlayerShape.setFillColor(Color::Green); // Color for other players
		window.draw(otherPlayerShape);
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
void gameLoop(RenderWindow& window, TcpSocket& socket, CircleShape& playerShape, vector<Vector2f>& rainbowPositions, vector<Color>& rainbowColors) {
	window.clear();

	// Render all players
	renderPlayers(window, playerShape);

	window.display();
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