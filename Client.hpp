#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>
# include <unistd.h>

class Client {
private:
	int _fd;
	std::string _readBuffer;
	std::string _writeBuffer;
	bool _isReadyToWrite; // Toggles when we finish parsing an HTTP request

public:
	Client(int fd);
	~Client();

	int  getFd() const;
	
	// Buffer management
	void appendToReadBuffer(const char* data, ssize_t size);
	void setWriteBuffer(const std::string& response);
	
	// State checking
	bool isReadyToWrite() const;
	void setReadyToWrite(bool state);
};

#endif