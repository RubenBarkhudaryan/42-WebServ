#ifndef CLIENT_HPP

# define CLIENT_HPP

# include "../../parser/include/http/HttpRequest.hpp"

# include <netinet/in.h>
# include <string>
# include <sys/types.h>

class	Client
{
	private:
		int							fd;
		struct sockaddr_in			addr;

		std::string					read_buff;
		std::string					write_buff;

		bool						headers_parsed;
		bool						write_stat;
		bool						bad_request;
		bool						body_too_large;

		HttpRequest					request;
		BodyFraming					framing;

		ssize_t						content_length;
		std::string::size_type		body_start;

		Client(const Client& other);
		Client& operator=(const Client& other);

	public:
		Client(int fd, struct sockaddr_in addr);
		~Client();

		int					getFd() const;
		int					getPort() const;
		struct sockaddr_in	getAddr() const;
		const HttpRequest&	getRequest() const;

		const std::string&	getReadBuff() const;
		const std::string&	getWriteBuff() const;

		void				appendReadBuffer(const char *data, ssize_t len);
		void				appendWriteBuffer(const std::string& data);

		void				consumeWriteBuffer(std::size_t size);
		void				consumeReadBuffer(std::size_t size);

		bool				hasBadRequest() const;
		bool				hasBodyTooLarge() const;
		bool				isRequestComplete(std::size_t maxBodySize = static_cast<std::size_t>(-1));
};

#endif //CLIENT_HPP