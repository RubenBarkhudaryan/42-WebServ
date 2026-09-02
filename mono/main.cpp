#include "./engine/engine.hpp"
#include <csignal>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
		return (1);
	}

	int port = std::atoi(argv[1]);
	if (port <= 0 || port > 65535)
	{
		std::cerr << "Invalid port: " << argv[1] << std::endl;
		return (1);
	}

	// A client disconnecting mid-send() can raise SIGPIPE and kill the
	// process outright. Ignoring it makes send() just return -1 instead,
	// which handleClientWrite already handles.
	signal(SIGPIPE, SIG_IGN);

	try
	{
		Engine engine(port);
		std::cout << "Listening on port " << port << "..." << std::endl;
		engine.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}