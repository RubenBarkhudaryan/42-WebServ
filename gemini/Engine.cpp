#include "Engine.hpp"
#include <iostream>
#include <fcntl.h>

void Engine::run() {
	std::cout << "Engine starting..." << std::endl;

	while (true) {
		int ready = poll(&_pollFds[0], _pollFds.size(), -1);
		if (ready < 0) {
			std::cerr << "Poll error" << std::endl;
			break;
		}

		// We iterate backwards. Why? Because if a client disconnects and we 
		// erase them from the vector, iterating forwards skips the next element!
		for (int i = _pollFds.size() - 1; i >= 0; i--) {
			if (_pollFds[i].revents == 0)
				continue;

			int currentFd = _pollFds[i].fd;
			bool isServer = false;

			// 1. Check if the FD belongs to one of our Servers
			for (size_t j = 0; j < _servers.size(); j++) {
				if (currentFd == _servers[j].getFd()) {
					_acceptNewConnection(_servers[j]);
					isServer = true;
					break;
				}
			}

			// 2. If it's not a server, it must be an existing Client
			if (!isServer) {
				_handleClientData(currentFd, _pollFds[i].revents);
			}
		}
	}
}

void Engine::_acceptNewConnection(Server& server) {
	int clientFd = accept(server.getFd(), NULL, NULL);
	if (clientFd >= 0) {
		// Enforce non-blocking
		fcntl(clientFd, F_SETFL, O_NONBLOCK);
		fcntl(clientFd, F_SETFD, FD_CLOEXEC);

		// Create new Client object and map it
		_clients[clientFd] = new Client(clientFd);

		// Add to poll tracking
		struct pollfd pfd;
		pfd.fd = clientFd;
		pfd.events = POLLIN | POLLOUT;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
	}
}