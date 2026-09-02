#ifndef ENGINE_HPP

# define ENGINE_HPP

# include "../client/client.hpp"
# include "../server/server.hpp"

# include <poll.h>
# include <vector>

class	Engine
{
	private:
		std::map<int, Server *>		servers;
		std::map<int, Server *>		client_to_server;

		std::vector<struct pollfd>	pollfds;

		Engine(const Engine& other);
		Engine&	operator=(const Engine& other);

		void	handleNewConnection(int server_fd);
		void	handleClientWrite(int client_fd);
		void	handleClientRead(int client_fd);
		void	handleClientRemove(int client_fd);

		void	updatePollEvents(int fd, short events);

	public:
		Engine();
		~Engine();

		void	run();
		void	addServer(Server *new_server);
};

#endif //ENGINE_HPP