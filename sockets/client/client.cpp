#include "./client.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <cstdlib>

namespace
{
	const std::string::size_type	MAX_HEADER_SIZE = 8192;
	const std::size_t				CHUNK_OVERHEAD_ALLOWANCE = 4096;
}

Client::Client(int fd, struct sockaddr_in addr) :
	fd(fd),
	addr(addr),
	headers_parsed(false),
	write_stat(false),
	bad_request(false),
	body_too_large(false),
	request(),
	framing(FRAMING_NONE),
	content_length(0),
	body_start(0),
	last_activity(time(NULL))
{
}

Client::~Client()
{
	close(this->fd);
}

int	Client::getFd() const
{
	return (this->fd);
}

int	Client::getPort() const
{
	return (ntohs(this->addr.sin_port));
}

struct sockaddr_in	Client::getAddr() const
{
	return (this->addr);
}

const HttpRequest&	Client::getRequest() const
{
	return (this->request);
}

const std::string&	Client::getReadBuff() const
{
	return (this->read_buff);
}

const std::string&	Client::getWriteBuff() const
{
	return (this->write_buff);
}

void	Client::appendReadBuffer(const char *data, ssize_t len)
{
	if (!data || len <= 0)
		return ;

	this->read_buff.append(data, len);
	this->last_activity = time(NULL);
}

time_t	Client::getLastActivity() const
{
	return (this->last_activity);
}

void	Client::appendWriteBuffer(const std::string& data)
{
	this->write_buff += data;
	this->write_stat = !this->write_buff.empty();
}

void	Client::consumeWriteBuffer(std::size_t size)
{
	this->write_buff.erase(0, size);
	this->write_stat = !this->write_buff.empty();
}

void	Client::consumeReadBuffer(std::size_t size)
{
	this->read_buff.erase(0, size);
}

bool	Client::hasBadRequest() const
{
	return (this->bad_request);
}

bool	Client::hasBodyTooLarge() const
{
	return (this->body_too_large);
}

bool	Client::parseHeadersIfNeeded()
{
	if (this->bad_request || this->body_too_large)
		return (false);
	if (this->headers_parsed)
		return (true);

	HeaderParseStatus status = this->request.parseHeaders(this->read_buff, this->body_start);

	if (status == HEADERS_MALFORMED)
	{
		this->bad_request = true;
		return (false);
	}
	if (status == HEADERS_INCOMPLETE)
	{
		if (this->read_buff.size() > MAX_HEADER_SIZE)
			this->bad_request = true;
		return (false);
	}

	this->headers_parsed = true;
	this->framing = this->request.getBodyFraming();

	if (this->framing == FRAMING_INVALID)
	{
		this->bad_request = true;
		return (false);
	}

	if (this->framing == FRAMING_CONTENT_LENGTH)
		this->content_length = this->request.getContentLength();

	return (true);
}

bool	Client::isBodyComplete(std::size_t maxBodySize)
{
	if (this->bad_request || this->body_too_large || !this->headers_parsed)
		return (false);

	if (this->framing == FRAMING_CONTENT_LENGTH &&
		this->content_length >= 0 &&
		static_cast<std::size_t>(this->content_length) > maxBodySize)
	{
		this->body_too_large = true;
		return (false);
	}

	// Chunked framing adds size-line/CRLF overhead on the wire, so the raw
	// buffered size can exceed maxBodySize slightly even when the actual
	// decoded body is within the limit. This is only a buffering safety
	// net against unbounded uploads; the precise check runs against the
	// decoded body size once the request reaches RequestHandler.
	if (this->framing == FRAMING_CHUNKED &&
		this->read_buff.size() - this->body_start > maxBodySize + CHUNK_OVERHEAD_ALLOWANCE)
	{
		this->body_too_large = true;
		return (false);
	}

	bool	complete = false;

	if (this->framing == FRAMING_NONE)
		complete = true;
	else if (this->framing == FRAMING_CONTENT_LENGTH)
		complete = (this->read_buff.size() >=
			this->body_start + static_cast<std::size_t>(this->content_length));
	else if (this->framing == FRAMING_CHUNKED)
	{
		ChunkedState state =
			HttpRequest::getChunkedState(this->read_buff, this->body_start);

		if (state == CHUNKED_MALFORMED)
		{
			this->bad_request = true;
			return (false);
		}
		complete = (state == CHUNKED_COMPLETE);
	}

	if (!complete)
		return (false);

	if (this->framing == FRAMING_CHUNKED)
	{
		std::string	decoded;

		if (!HttpRequest::decodeChunkedBody(this->read_buff.substr(this->body_start), decoded))
		{
			this->bad_request = true;
			return (false);
		}
		this->request.setBody(decoded);
	}
	else
		this->request.setBody(this->read_buff.substr(this->body_start));

	return (true);
}