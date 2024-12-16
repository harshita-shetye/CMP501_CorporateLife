#include "Server.h"

int main() {
	srand(static_cast<unsigned int>(time(nullptr)));

	Server server;
	server.run();

	return 0;
}