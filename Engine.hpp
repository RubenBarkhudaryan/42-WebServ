#ifndef ENGINE_HPP
# define ENGINE_HPP

# include "Server.hpp"
# include "Client.hpp"
# include <vector>
# include <map>
# include <poll.h>

class	Engine
{
private:
	std::vector<Server> _servers;
	std::map<int, Client*> _clients;       // Maps a socket FD to a Client object
	std::vector<struct pollfd> _pollFds;   // The vector we pass to poll()

	// Private helper methods
	void _acceptNewConnection(Server& server);
	void _handleClientData(int fd, short revents);
	void _removeClient(int fd);

public:
	Engine();
	~Engine();

	void addServer(int port);
	void run(); // The main poll() loop
};

#endif