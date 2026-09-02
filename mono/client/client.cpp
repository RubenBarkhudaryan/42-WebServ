#include "./client.hpp"
#include <unistd.h>

Client::Client(int fd) : sock_fd(fd), writeState(false)
{}

Client::~Client()
{
	close(this->sock_fd);
}

int	Client::getFd() const
{
	return (this->sock_fd);
}

void	Client::appendToReadBuffer(const std::string& data)
{
	this->readBuffer.append(data);
}

void	Client::setWriteBuffer(const std::string& response)
{
	this->writeBuffer = response;
	this->writeState = !this->writeBuffer.empty();
}

const std::string&	Client::getWriteBuffer() const
{
	return (this->writeBuffer);
}

const std::string&	Client::getReadBuffer() const
{
	return (this->readBuffer);
}

void	Client::consumeWriteBuffer(size_t n)
{
	this->writeBuffer.erase(0, n);
	this->writeState = !this->writeBuffer.empty();
}

void	Client::consumeReadBuffer(size_t n)
{
	this->readBuffer.erase(0, n);
}

bool	Client::isReadyToWrite() const
{
	return (this->writeState);
}
