#include "./server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <vector>

Server::Server(const ServerConfig& config) :
		config(config),
		port(config.getPort()),
		ipAddr(config.getHost())
{
}

Server::~Server()
{
	close(this->fd);

	for (std::map<int, Client *>::iterator it = this->clients.begin(); it != clients.end(); ++it)
		delete it->second;
}

void	Server::addClient(Client *new_client)
{
	if (!new_client)
		return ;

	this->clients.insert(std::make_pair<int, Client *>(new_client->getFd(), new_client));
}

void	Server::removeClient(int target_fd)
{
	std::map<int, Client *>::iterator target_it = this->clients.find(target_fd);

	if (target_it != this->clients.end())
	{
		delete target_it->second;
		this->clients.erase(target_it);
	}
}

int	Server::getFd() const
{
	return (this->fd);
}

int	Server::getPort() const
{
	return (this->port);
}

std::string	Server::getIP() const
{
	return (this->ipAddr);
}

const ServerConfig&	Server::getConfig() const
{
	return (this->config);
}

Client	*Server::getClient(int fd)
{
	std::map<int, Client *>::iterator	target = this->clients.find(fd);

	if (target != this->clients.end())
		return (target->second);
	return (NULL);
}

const Client	*Server::getClient(int fd) const
{
	std::map<int, Client *>::const_iterator	target = this->clients.find(fd);

	if (target != this->clients.end())
		return (target->second);
	return (NULL);
}

void	Server::setup()
{
	int	server_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (server_sock == -1)
		throw std::runtime_error("[Server]: socket init failed.");

	this->fd = server_sock;

	int	opt = 1;
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		close(server_sock);
		throw std::runtime_error("[Server]: setsockopt failed.");
	}

	this->addr.sin_family = AF_INET;
	this->addr.sin_port = htons(this->port);

	if (!ipAddr.empty())
	{
		if (inet_pton(AF_INET, this->ipAddr.c_str(), (struct sockaddr*)&this->addr.sin_addr) != 1)
		{
			close(server_sock);
			throw std::runtime_error("[Server]: invalid listen host '" + this->ipAddr + "'.");
		}
	}
	else
		this->addr.sin_addr.s_addr = INADDR_ANY;

	int	bind_stat = bind(server_sock, (struct sockaddr*)&this->addr, sizeof(this->addr));

	if (bind_stat == -1)
	{
		close(server_sock);
		throw std::runtime_error("[Server]: socket bind failed.");
	}

	int	listen_stat = listen(server_sock, SOMAXCONN);

	if (listen_stat == -1)
	{
		close(server_sock);
		throw std::runtime_error("[Server]: listen failed.");
	}

	fcntl(server_sock, F_SETFL, O_NONBLOCK);
	fcntl(server_sock, F_SETFD, O_CLOEXEC);
}
