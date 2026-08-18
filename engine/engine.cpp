#include "./engine.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>

Engine::Engine(int port) : server(port)
{
	this->server.setup();

	struct pollfd	server_pollfd;

	server_pollfd.fd = this->server.getFd();
	server_pollfd.events = POLLIN;
	server_pollfd.revents = 0;
	this->fds.push_back(server_pollfd);
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
		int	ready = poll(this->fds.data(), this->fds.size(), -1);

		if (ready == -1)
			continue;

		for (std::size_t i = 0; i < this->fds.size(); ++i)
		{
			if (this->fds[i].revents & POLLIN)
			{
				if (this->fds[i].fd == this->server.getFd())
					this->handleNewConnection();
				else
					handleClientRead(this->fds[i].fd);
			}
			if (i < this->fds.size() && (this->fds[i].revents & POLLOUT))
				handleClientWrite(this->fds[i].fd);
		}
	}
}

void	Engine::handleNewConnection()
{
	int	new_client_fd = accept(this->server.getFd(), NULL, NULL);

	if (new_client_fd == -1)
		return ;

	fcntl(new_client_fd, F_SETFL, O_NONBLOCK);
	fcntl(new_client_fd, F_SETFD, FD_CLOEXEC);

	Client*	new_client = new Client(new_client_fd);

	this->clients[new_client_fd] = new_client;

	struct pollfd	client_pollfd;

	client_pollfd.fd = new_client_fd;
	client_pollfd.events = POLLIN;
	client_pollfd.revents = 0;
	this->fds.push_back(client_pollfd);
	// std::cout << "New client connected: fd " << new_client_fd << std::endl;
}

void	Engine::handleClientRead(int fd)
{
	char	buff[4096];
	ssize_t	bytes_read = recv(fd, buff, sizeof(buff), 0);

	if (bytes_read > 0)
	{
		Client* client = this->clients[fd];
		client->appendToReadBuffer(std::string(buff, bytes_read));

		/*
			Parser integration
		*/

		if (client->isReadyToWrite())
			this->updatePollEvents(fd, POLLIN | POLLOUT);
	}
	else
		this->removeClient(fd);
}

void	Engine::handleClientWrite(int fd)
{
	Client*	client = this->clients[fd];

	const std::string&	writeBuffer = client->getWriteBuffer();

	ssize_t	bytes_sent = send(fd, writeBuffer.data(), writeBuffer.size(), 0);

	if (bytes_sent > 0)
	{
		client->consumeWriteBuffer(bytes_sent);

		if (!client->isReadyToWrite())
			this->updatePollEvents(fd, POLLIN);
	}
	else if (bytes_sent == -1)
		this->removeClient(fd);
}

void	Engine::removeClient(int fd)
{
	std::map<int, Client*>::iterator it = this->clients.find(fd);
	if (it != this->clients.end())
	{
		delete it->second;
		this->clients.erase(it);
	}
	this->fds.erase(
		std::remove_if
		(
			this->fds.begin(),
			this->fds.end(),
			MatchFd(fd)
		),
		this->fds.end());
	// std::cout << "Removing client fd " << fd << std::endl;
}

void	Engine::updatePollEvents(int fd, short events)
{
	for (std::size_t i = 0; i < this->fds.size(); ++i)
	{
		if (this->fds[i].fd == fd)
		{
			this->fds[i].events = events;
			break ;
		}
	}
}