#include <iostream>
#include <conio.h>

#include "chat_server.h"

int main()
{
	std::cout << "[Main] Started" << std::endl;

	ChatServer server;

	server.Start(7777, 1000, 1024);

	while (server.IsServerRunning()) {
		if (_kbhit()) {
			auto key = _getch();

			if (key == 'q' || key == 'Q') {
				server.Terminate();
			}
		}
	}

	std::cout << "[Main] Exit" << std::endl;
  	return 0;
}