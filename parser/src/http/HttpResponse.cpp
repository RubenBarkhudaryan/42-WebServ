#include "../../include/http/HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse() : _statusCode(200), _statusMessage("OK"), _headers(), _body("") {}

void HttpResponse::setStatus(int code, const std::string &message)
{
	_statusCode = code;
	_statusMessage = message;
}

void HttpResponse::setHeader(const std::string &name, const std::string &value)
{
	_headers.insert(std::make_pair(name, value));
}

void HttpResponse::setBody(const std::string &body)
{
	_body = body;
}

std::string HttpResponse::serialize() const
{
	std::ostringstream responseStream;
	responseStream << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";

	for(std::multimap<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
	{
		const std::pair<std::string, std::string> &header = *it;
		responseStream << header.first << ": " << header.second << "\r\n";
	}

	responseStream << "Content-Length: " << _body.size() << "\r\n";
	responseStream << "\r\n";
	responseStream << _body;

	return responseStream.str();
}
