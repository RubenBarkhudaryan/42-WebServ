#include "./engine.hpp"

#include "../../parser/include/http/RequestHandler.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <stdexcept>
#include <cstddef>

#include <fcntl.h>

#include <algorithm>

#include <map>
#include <set>
#include <vector>

namespace
{
	bool isPollFdRemoved(const struct pollfd &entry)
	{
		return entry.fd == -1;
	}
}

Engine::Engine()
{}

Engine::~Engine()
{
	for (std::map<int, Server *>::iterator it = this->servers.begin(); it != this->servers.end(); ++it)
		delete it->second;

	std::set<CgiProcess *>	uniqueCgiSessions;

	for (std::map<int, CgiProcess *>::iterator it = this->cgi_fds.begin(); it != this->cgi_fds.end(); ++it)
		uniqueCgiSessions.insert(it->second);
	for (std::size_t i = 0; i < this->cgi_pending_reap.size(); ++i)
		uniqueCgiSessions.insert(this->cgi_pending_reap[i]);

	for (std::set<CgiProcess *>::iterator it = uniqueCgiSessions.begin(); it != uniqueCgiSessions.end(); ++it)
		delete *it;
}

void	Engine::run()
{
	while (true)
	{
		int	timeout = this->cgi_pending_reap.empty() ? -1 : 25;
		int	ready = poll(this->pollfds.data(), this->pollfds.size(), timeout);

		if (ready == -1)
			throw std::runtime_error("[Engine]: poll failed");

		for (std::size_t i = 0; i < this->pollfds.size(); ++i)
		{
			int		fd = this->pollfds[i].fd;
			short	revents = this->pollfds[i].revents;

			if (fd == -1 || revents == 0)
				continue;

			std::map<int, CgiProcess *>::iterator cgiIt = this->cgi_fds.find(fd);

			if (cgiIt != this->cgi_fds.end())
			{
				CgiProcess	*cgi = cgiIt->second;
				bool		isStdin = (fd == cgi->getStdinFd());

				if (isStdin && (revents & (POLLOUT | POLLHUP | POLLERR)))
					handleCgiWritable(fd);
				else if (!isStdin && (revents & (POLLIN | POLLHUP | POLLERR)))
					handleCgiReadable(fd);
				continue;
			}

			if (revents & POLLIN)
			{
				if (this->servers.find(fd) != this->servers.end())
					handleNewConnection(fd);
				else
					handleClientRead(fd);
			}

			if (this->pollfds[i].fd != -1 && (revents & POLLOUT))
				handleClientWrite(fd);

			if (this->pollfds[i].fd != -1 && (revents & (POLLERR | POLLHUP | POLLNVAL)))
				handleClientRemove(fd);
		}

		this->pollfds.erase(
			std::remove_if(this->pollfds.begin(), this->pollfds.end(), isPollFdRemoved),
			this->pollfds.end());

		reapCgiSessions();
	}
}

void	Engine::addServer(Server *new_server)
{
	if (!new_server)
		return ;

	try
	{
		new_server->setup();
	}
	catch (...)
	{
		delete new_server;
		throw;
	}

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

	bool request_complete = client->isRequestComplete(server->getConfig().getClientMaxBodySize());
	if (client->hasBodyTooLarge())
	{
		HttpResponse response = RequestHandler::makeErrorResponse(413, server->getConfig());

		response.setHeader("Connection", "close");
		client->appendWriteBuffer(response.serialize());
		updatePollEvents(client_fd, POLLOUT);
		return;
	}

	if (client->hasBadRequest())
	{
		HttpResponse response = RequestHandler::makeErrorResponse(400, server->getConfig());

		response.setHeader("Connection", "close");
		client->appendWriteBuffer(response.serialize());
		updatePollEvents(client_fd, POLLOUT);
		return;
	}

	if (request_complete)
	{
		std::string	clientIp = inet_ntoa(client->getAddr().sin_addr);

		RequestHandler::HandlerResult result =
			RequestHandler::handle(client->getRequest(), server->getConfig(), clientIp);

		if (result.cgi)
		{
			result.cgi->setClientFd(client_fd);
			registerCgiSession(result.cgi);
			return ;
		}

		result.response.setHeader("Connection", "close");
		client->appendWriteBuffer(result.response.serialize());
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
		invalidateCgiClient(client_fd);
		removePollFd(client_fd);
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

void	Engine::removePollFd(int fd)
{
	for (std::size_t i = 0; i < this->pollfds.size(); ++i)
	{
		if (this->pollfds[i].fd == fd)
		{
			this->pollfds[i].fd = -1;
			break ;
		}
	}
}

void	Engine::registerCgiSession(CgiProcess *cgi)
{
	if (cgi->isInputOpen())
	{
		struct pollfd	stdin_pollfd;

		stdin_pollfd.fd = cgi->getStdinFd();
		stdin_pollfd.events = POLLOUT;
		stdin_pollfd.revents = 0;
		this->pollfds.push_back(stdin_pollfd);
		this->cgi_fds[cgi->getStdinFd()] = cgi;
	}

	if (cgi->isOutputOpen())
	{
		struct pollfd	stdout_pollfd;

		stdout_pollfd.fd = cgi->getStdoutFd();
		stdout_pollfd.events = POLLIN;
		stdout_pollfd.revents = 0;
		this->pollfds.push_back(stdout_pollfd);
		this->cgi_fds[cgi->getStdoutFd()] = cgi;
	}
	else
		this->cgi_pending_reap.push_back(cgi);
}

void	Engine::handleCgiWritable(int fd)
{
	std::map<int, CgiProcess *>::iterator	it = this->cgi_fds.find(fd);

	if (it == this->cgi_fds.end())
		return ;

	CgiProcess	*cgi = it->second;

	cgi->handleWritable();
	if (!cgi->isInputOpen())
		removeCgiFd(fd);
}

void	Engine::handleCgiReadable(int fd)
{
	std::map<int, CgiProcess *>::iterator	it = this->cgi_fds.find(fd);

	if (it == this->cgi_fds.end())
		return ;

	CgiProcess	*cgi = it->second;

	cgi->handleReadable();
	if (!cgi->isOutputOpen())
	{
		removeCgiFd(fd);
		this->cgi_pending_reap.push_back(cgi);
	}
}

void	Engine::removeCgiFd(int fd)
{
	this->cgi_fds.erase(fd);
	removePollFd(fd);
}

void	Engine::finalizeCgiSession(CgiProcess *cgi)
{
	std::map<int, Server *>::iterator	target = this->client_to_server.find(cgi->getClientFd());

	if (target != this->client_to_server.end())
	{
		Server	*server = target->second;
		Client	*client = server ? server->getClient(cgi->getClientFd()) : NULL;

		if (client)
		{
			HttpResponse	response = RequestHandler::finishCgi(cgi, server->getConfig());

			response.setHeader("Connection", "close");
			client->appendWriteBuffer(response.serialize());
			updatePollEvents(cgi->getClientFd(), POLLIN | POLLOUT);
		}
	}

	delete cgi;
}

void	Engine::reapCgiSessions()
{
	std::size_t	i = 0;

	while (i < this->cgi_pending_reap.size())
	{
		CgiProcess	*cgi = this->cgi_pending_reap[i];

		if (cgi->tryReap())
		{
			finalizeCgiSession(cgi);
			this->cgi_pending_reap.erase(this->cgi_pending_reap.begin() + i);
		}
		else
			++i;
	}
}

void	Engine::invalidateCgiClient(int client_fd)
{
	for (std::map<int, CgiProcess *>::iterator it = this->cgi_fds.begin(); it != this->cgi_fds.end(); ++it)
	{
		if (it->second->getClientFd() == client_fd)
			it->second->setClientFd(-1);
	}

	for (std::size_t i = 0; i < this->cgi_pending_reap.size(); ++i)
	{
		if (this->cgi_pending_reap[i]->getClientFd() == client_fd)
			this->cgi_pending_reap[i]->setClientFd(-1);
	}
}