#ifndef SERVER_HPP

# define SERVER_HPP

# include <netinet/in.h>

class	Server
{
	private:
		int					sock_fd;
		int					port;
		struct sockaddr_in	addr;

		Server(const Server& other);
		Server& operator=(const Server& other);

	public:
		Server(int port);
		~Server();

		void	setup();
		int		getFd() const;
		int		getPort() const;
};

#endif //SERVER_HPP