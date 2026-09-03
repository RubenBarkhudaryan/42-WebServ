#include "./client.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <cstdlib>

Client::Client(int fd, struct sockaddr_in addr) :
	fd(fd),
	addr(addr),
	content_len(0),
	headers_parsed(false),
	write_stat(false)
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


bool	Client::isRequestComplete()
{
	size_t header_end_pos = this->read_buff.find("\r\n\r\n");

	if (!this->headers_parsed && header_end_pos != std::string::npos)
	{
		this->headers_parsed = true;

		size_t cl_pos = this->read_buff.find("Content-Length: ");
		
		if (cl_pos != std::string::npos && cl_pos < header_end_pos)
		{
			cl_pos += 16;
			this->content_len = std::atoi(this->read_buff.c_str() + cl_pos);
		}
		else
			this->content_len = 0;
	}

	if (this->headers_parsed)
	{
		size_t expected_total_size = header_end_pos + 4 + this->content_len;

		if (this->read_buff.size() >= expected_total_size)
			return (true);
	}

	return (false);
}