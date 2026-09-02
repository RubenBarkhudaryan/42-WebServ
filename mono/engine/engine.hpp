#ifndef ENGINE_HPP

# define ENGINE_HPP

# include "../server/server.hpp"
# include "../client/client.hpp"

# include <map>
# include <vector>

# include <poll.h>

class	Engine
{
	private:
		Server						server;
		std::map<int, Client*>		clients;
		std::vector<struct pollfd>	fds;

		Engine(const Engine& other);
		Engine& operator=(const Engine& other);

	public:
		Engine(int port);
		~Engine();

		void	run();

	private:
		void	handleNewConnection();
		void	handleClientRead(int fd);
		void	handleClientWrite(int fd);
		void	removeClient(int fd);
		void	updatePollEvents(int fd, short events);
};

struct	MatchFd
{
	int	_fd;

	MatchFd(int fd) : _fd(fd) {}
	bool	operator()(const struct pollfd& pfd) const
	{
		return (pfd.fd == _fd);
	}
};

#endif //ENGINE_HPP