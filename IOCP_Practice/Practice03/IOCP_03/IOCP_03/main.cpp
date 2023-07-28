#include <iostream>
#include <conio.h>

#include "echo_server.h"

int main()
{
	std::cout << "[Main] Started" << std::endl;

	EchoServer server;
	if (!server.InitSocket())
	{
		std::cout << "[Main] Failed to Initialize\n";
		return -1;
	}

	if (!server.BindAndListen(7777))
	{
		std::cout << "[Main] Failed to Bind And Listen\n";
		return -1;
	}

	if (!server.StartServer(1000))
	{
		std::cout << "[Main] Failed to Start\n";
		return -1;
	}

	std::cout << "[Main] Successed to Start Server\n";

	while (server.IsServerRunning()) {
		if (_kbhit()) {
			char key = _getch();

			if (key == 'q' || key == 'Q') {
				server.Terminate();
			}
		}
	}

	std::cout << "[Main] Exit" << std::endl;
	return 0;
}