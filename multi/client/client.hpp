#ifndef CLIENT_HPP

# define CLIENT_HPP

# include <netinet/in.h>
# include <string>
# include <sys/types.h>

class	Client
{
	private:
		int					fd;
		struct sockaddr_in	addr;

		std::string			read_buff;
		std::string			write_buff;

		ssize_t				content_len;
		bool				headers_parsed;

		bool				write_stat;

		Client(const Client& other);
		Client& operator=(const Client& other);

	public:
		Client(int fd, struct sockaddr_in addr);
		~Client();

		int					getFd() const;
		int					getPort() const;
		struct sockaddr_in	getAddr() const;

		const std::string&	getReadBuff() const;
		const std::string&	getWriteBuff() const;

		//void				setWriteStatus(bool status);

		void				appendReadBuffer(const char *data, ssize_t len);
		void				appendWriteBuffer(const std::string& data);

		void				consumeWriteBuffer(std::size_t size);

		bool				isRequestComplete();
};

#endif //CLIENT_HPP