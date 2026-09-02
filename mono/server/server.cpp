#include "./server.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

Server::Server(int port) : sock_fd(-1), port(port)
{}

Server::~Server()
{
	if (this->sock_fd != -1)
		close(this->sock_fd);
}

int	Server::getFd() const
{
	return (this->sock_fd);
}

int	Server::getPort() const
{
	return (this->port);
}

void	Server::setup()
{
	this->sock_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (this->sock_fd == -1)
		throw std::runtime_error("Failed to create server socket.");

	int	opt = 1;
	setsockopt(this->sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	this->addr.sin_family = AF_INET;
	this->addr.sin_addr.s_addr = INADDR_ANY;
	this->addr.sin_port = htons(this->port);

	int	bindStat = bind(this->sock_fd, (struct sockaddr*)&this->addr, sizeof(this->addr));

	if (bindStat == -1)
		throw std::runtime_error("Failed to bind server socket.");

	int	listenStat = listen(this->sock_fd, SOMAXCONN);

	if (listenStat == -1)
		throw std::runtime_error("Failed to listen on server socket.");

	fcntl(this->sock_fd, F_SETFL, O_NONBLOCK);
	fcntl(this->sock_fd, F_SETFD, FD_CLOEXEC);
}