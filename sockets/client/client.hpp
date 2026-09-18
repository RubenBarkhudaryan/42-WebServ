#ifndef CLIENT_HPP

# define CLIENT_HPP

# include "../../parser/include/http/HttpRequest.hpp"

# include <netinet/in.h>
# include <string>
# include <sys/types.h>
# include <ctime>

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

		time_t						last_activity;
		bool						dispatched;

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

		/*
		** Two-phase completion check: headers must be parsed (and their
		** target known) before the caller can look up the per-location
		** client_max_body_size to enforce, so parsing and the body-size/
		** completion check are separate calls instead of one.
		*/
		bool				parseHeadersIfNeeded();
		bool				isBodyComplete(std::size_t maxBodySize);

		time_t				getLastActivity() const;

		/*
		** Idle-timeout eligibility: a connection that has started receiving
		** a request but not yet been dispatched to RequestHandler (e.g. a
		** large body still uploading) must never be timed out just because
		** the server hasn't gotten around to reading it under heavy
		** concurrent load - that's server-side scheduling delay, not client
		** idleness. Only "no valid request started yet" and "response
		** already sent, client lingering" are genuinely idle.
		*/
		bool				isHeadersParsed() const;
		bool				isDispatched() const;
};

#endif //CLIENT_HPP