#ifndef SERVER_HPP
# define SERVER_HPP

# include <string>
# include <sys/socket.h>
# include <netinet/in.h>

class Server {
private:
	int _listenFd;
	int _port;
	struct sockaddr_in _address;

public:
	Server(int port);
	~Server();

	void setup(); // Contains socket(), setsockopt(), fcntl(), bind(), listen()
	int  getFd() const;
	int  getPort() const;
};

#endif