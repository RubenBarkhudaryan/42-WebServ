#include "./engine.hpp"

#include "../../parser/include/http/RequestHandler.hpp"

#include <sys/socket.h>
#include <netinet/in.h>

#include <iostream>
#include <stdexcept>

#include <fcntl.h>

#include <algorithm>

#include <map>
#include <vector>

Engine::Engine()
{}

Engine::~Engine()
{
	for (std::map<int, Server *>::iterator it = this->servers.begin(); it != this->servers.end(); ++it)
		delete it->second;
}

void	Engine::run()
{
	while (true)
	{
		int	ready = poll(this->pollfds.data(), this->pollfds.size(), -1);

		if (ready == -1)
			throw std::runtime_error("[Engine]: poll failed");

		for (std::size_t i = 0; i < this->pollfds.size(); ++i)
		{
			if (this->pollfds[i].revents & POLLIN)
			{
				int	current_fd = this->pollfds[i].fd;

				if (this->servers.find(current_fd) != this->servers.end())
					handleNewConnection(current_fd);
				else
					handleClientRead(current_fd);
			}

			if (i < this->pollfds.size() && (this->pollfds[i].revents & POLLOUT))
				handleClientWrite(this->pollfds[i].fd);

			if (i < this->pollfds.size() && (this->pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)))
				handleClientRemove(this->pollfds[i].fd);
		}
	}
}

void	Engine::addServer(Server *new_server)
{
	if (!new_server)
		return ;

	new_server->setup();

	struct pollfd	server_pollfd;

	server_pollfd.fd = new_server->getFd();
	server_pollfd.events = POLLIN;
	server_pollfd.revents = 0;

	this->pollfds.push_back(server_pollfd);

	this->servers.insert(std::make_pair<int, Server *>(new_server->getFd(), new_server));

}

void	Engine::handleNewConnection(int server_fd)
{
	struct sockaddr_in	client_addr;
	socklen_t			client_size = sizeof(client_addr);
	int	new_client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_size);

	if (new_client_fd == -1)
	{
		std::cerr << "[Engine]: client connection broken" << std::endl;
		return ;
	}

	this->servers[server_fd]->addClient(new Client(new_client_fd, client_addr));
	this->client_to_server[new_client_fd] = this->servers[server_fd];

	struct pollfd	client_pollfd;

	client_pollfd.fd = new_client_fd;
	client_pollfd.events = POLLIN;
	client_pollfd.revents = 0;

	this->pollfds.push_back(client_pollfd);

	fcntl(new_client_fd, F_SETFL, O_NONBLOCK);
	fcntl(new_client_fd, F_SETFD, O_CLOEXEC);
}

void	Engine::handleClientRead(int client_fd)
{
	char	buffer[4096] = {0};

	std::map<int, Server *>::const_iterator	target = this->client_to_server.find(client_fd);

	if (target == this->client_to_server.end())
		return ;

	Server	*server = target->second;
	
	if (!server)
	{
		std::cerr << "[Engine]: server not allocated" << std::endl;
		return ;
	}
	
	Client	*client = server->getClient(client_fd);

	if (!client)
	{
		std::cerr << "[Engine]: client not allocated" << std::endl;
		return ;
	}

	ssize_t	bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

	if (bytes_read <= 0)
	{
		handleClientRemove(client_fd);
		return ;
	}

	client->appendReadBuffer(buffer, bytes_read);

	if (client->isRequestComplete())
	{
		HttpResponse	response = RequestHandler::handle(client->getRequest(), server->getConfig());

		response.setHeader("Connection", "close");
		client->appendWriteBuffer(response.serialize());
		updatePollEvents(client_fd, POLLIN | POLLOUT);
	}
}

void	Engine::handleClientWrite(int client_fd)
{
	std::map<int, Server *>::iterator	target = this->client_to_server.find(client_fd);
	if (target == this->client_to_server.end())
		return ;

	Server	*server = target->second;

	if (!server)
	{
		std::cerr << "[Engine]: server not allocated" << std::endl;
		return ;
	}

	Client	*client = server->getClient(client_fd);

	if (!client)
	{
		std::cerr << "[Engine]: client not allocated" << std::endl;
		return ;
	}

	const std::string&	buffer = client->getWriteBuff();

	ssize_t	bytes_sent = send(client_fd, buffer.c_str(), buffer.size(), 0);

	if (bytes_sent <= 0)
	{
		handleClientRemove(client_fd);
		return ;
	}
	client->consumeWriteBuffer(bytes_sent);

	if (client->getWriteBuff().empty())
		updatePollEvents(client_fd, POLLIN);
}

void	Engine::handleClientRemove(int client_fd)
{
	std::map<int, Server *>::iterator	target = this->client_to_server.find(client_fd);

	if (target != this->client_to_server.end())
	{
		target->second->removeClient(client_fd);
		this->client_to_server.erase(client_fd);

		for (std::vector<struct pollfd>::iterator it = this->pollfds.begin(); it != this->pollfds.end(); ++it)
		{
			if (it->fd == client_fd)
			{
				this->pollfds.erase(it);
				break ;
			}
		}
	}
}

void	Engine::updatePollEvents(int fd, short events)
{
	for (std::size_t i = 0; i < this->pollfds.size(); ++i)
	{
		if (this->pollfds[i].fd == fd)
		{
			this->pollfds[i].events = events;
			break ;
		}
	}
}