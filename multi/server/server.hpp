#ifndef SERVER_HPP

# define SERVER_HPP

# include "../client/client.hpp"

# include <netinet/in.h>
# include <map>
# include <string>

class	Server
{
	private:
		int						fd;
		int						port;
		std::string				ipAddr;
		std::map<int, Client *>	clients;
		struct sockaddr_in		addr;

		Server(const Server& other);
		Server& operator=(const Server& other);

	public:
		Server(int port, const std::string& ipAddr);
		~Server();

		int				getFd() const;
		int				getPort() const;
		std::string		getIP() const;
		Client			*getClient(int fd);
		const Client	*getClient(int fd) const;

		void			setup();

		void			addClient(Client *new_client);
		void			removeClient(int target_fd);
};

#endif //SERVER_HPP