#include "Client.h"

// Main entry point
int main() {
	RenderWindow* window = nullptr; // Pointer to the game window
	TcpSocket socket;
	CircleShape playerShape(CIRCLE_RADIUS);
	vector<Vector2f> rainbowPositions;
	vector<Color> rainbowColors;

	createMainMenu(window, socket, playerShape, rainbowPositions, rainbowColors);

	return 0;
}