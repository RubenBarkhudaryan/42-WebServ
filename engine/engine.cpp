#include "./engine.hpp"

Engine::Engine(int port) : server(port)
{
	this->server.setup();

	struct pollfd	server_pollfd;

	server_pollfd.fd = this->server.getFd();
}

Engine::~Engine()
{
	for (std::map<int, Client*>::iterator it = this->clients.begin(); it != this->clients.end(); ++it)
		delete it->second;
}

void	Engine::run()
{
	while (true)
	{

	}
}

void	Engine::handleNewConnection()
{
	int	new_fd
}

void	Engine::handleClientRead(int fd)
{

}

void	Engine::handleClientWrite(int fd)
{

}

void	Engine::removeClient(int fd)
{
	std::map<int, Client*>::iterator it = this->clients.find(fd);
	if (it != this->clients.end())
	{
		delete it->second;
		this->clients.erase(it);
	}
}