#ifndef CLIENT_HPP

# define CLIENT_HPP

# include <string>

class	Client
{
	private:
		int			sock_fd;
		std::string	writeBuffer;
		std::string	readBuffer;
		bool		writeState;

		Client(const Client& other);
		Client& operator=(const Client& other);

	public:
		Client(int fd);
		~Client();

		int		getFd() const;

		void	appendToReadBuffer(const std::string& data);
		void	setWriteBuffer(const std::string& response);

		const std::string&	getWriteBuffer() const;
		const std::string&	getReadBuffer() const;
		
		void	consumeWriteBuffer(size_t n);
		void	consumeReadBuffer(size_t n);

		bool	isReadyToWrite() const;
};

#endif //CLIENT_HPP