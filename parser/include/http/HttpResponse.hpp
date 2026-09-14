#pragma once

#include <string>
#include <map>

class HttpResponse
{
private:
	int _statusCode;
	std::string _statusMessage;
	std::multimap<std::string, std::string> _headers;
	std::string _body;

public:
	HttpResponse();

	void setStatus(int code, const std::string &message);
	void setHeader(const std::string &name, const std::string &value);
	void setBody(const std::string &body);

	std::string serialize() const;
};
