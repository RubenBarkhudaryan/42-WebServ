#include "../../include/http/HttpRequest.hpp"

namespace
{
	std::string trim(const std::string &value)
	{
		std::string::size_type first = value.find_first_not_of(" \t");
		if (first == std::string::npos)
			return "";

		std::string::size_type last = value.find_last_not_of(" \t");
		return value.substr(first, last - first + 1);
	}
}

HttpRequest::HttpRequest() : _method(""), _target(""), _version(""), _headers(), _body("") {}

const std::string &HttpRequest::getMethod() const
{
	return _method;
}

const std::string &HttpRequest::getTarget() const
{
	return _target;
}

const std::string &HttpRequest::getVersion() const
{
	return _version;
}

const std::multimap<std::string, std::string> &HttpRequest::getHeaders() const
{
	return _headers;
}

const std::string &HttpRequest::getBody() const
{
	return _body;
}

bool HttpRequest::hasHeader(const std::string &name) const
{
	return _headers.find(name) != _headers.end();
}

const std::string &HttpRequest::getHeader(const std::string &name) const
{
	std::multimap<std::string, std::string>::const_iterator it = _headers.find(name);
	if (it != _headers.end())
	{
		return it->second;
	}
	static const std::string emptyString = "";
	return emptyString;
}

void HttpRequest::parse(const std::string &rawRequest)
{
	_method.clear();
	_target.clear();
	_version.clear();
	_headers.clear();
	_body.clear();

	std::string::size_type headerEnd = rawRequest.find("\r\n\r\n");
	std::string::size_type delimiterLength = 4;
	if (headerEnd == std::string::npos)
	{
		headerEnd = rawRequest.find("\n\n");
		delimiterLength = 2;
	}

	std::string headerSection = rawRequest;
	if (headerEnd != std::string::npos)
		headerSection = rawRequest.substr(0, headerEnd);

	std::istringstream requestStream(headerSection);
	std::string line;

	if (std::getline(requestStream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		std::istringstream lineStream(line);
		lineStream >> _method >> _target >> _version;
	}

	std::string headerLine;
	while (std::getline(requestStream, headerLine))
	{
		if (!headerLine.empty() && headerLine[headerLine.size() - 1] == '\r')
			headerLine.erase(headerLine.size() - 1);
		if (headerLine.empty())
			break;

		std::size_t colonPos = headerLine.find(':');
		if (colonPos != std::string::npos)
		{
			std::string name = trim(headerLine.substr(0, colonPos));
			std::string value = trim(headerLine.substr(colonPos + 1));
			_headers.insert(std::make_pair(name, value));
		}
	}

	if (headerEnd != std::string::npos)
		_body = rawRequest.substr(headerEnd + delimiterLength);
}
