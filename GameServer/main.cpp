#include "Server.h"

int main() {
	srand(static_cast<unsigned int>(time(nullptr))); // Seed for random number generation
	SetupServer(PORT); // Start the server
	return 0;
}