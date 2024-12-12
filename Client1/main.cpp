#include "Client.h"

// Main entry point
int main() {
	RenderWindow* window = nullptr; // Pointer to the game window
	TcpSocket socket;

	createMainMenu(window, socket);

	return 0;
}