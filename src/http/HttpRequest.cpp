#include "../../include/http/HttpRequest.hpp"

#include <cctype>
#include <algorithm>

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

bool HeaderNameLess::operator()(const std::string &a, const std::string &b) const
{
	std::string::size_type i = 0;
	while (i < a.size() && i < b.size())
	{
		int left = std::tolower(static_cast<unsigned char>(a[i]));
		int right = std::tolower(static_cast<unsigned char>(b[i]));
		if (left < right)
			return true;
		if (left > right)
			return false;
		i++;
	}
	return a.size() < b.size();
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

const std::multimap<std::string, std::string, HeaderNameLess> &HttpRequest::getHeaders() const
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
	std::multimap<std::string, std::string, HeaderNameLess>::const_iterator it = _headers.find(name);
	if (it != _headers.end())
	{
		return it->second;
	}
	static const std::string emptyString = "";
	return emptyString;
}

bool HttpRequest::parseHeaders(
	const std::string &rawRequest,
	std::string::size_type &bodyStart)
{
	_method.clear();
	_target.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	bodyStart = std::string::npos;

	std::string::size_type headerEnd = rawRequest.find("\r\n\r\n");
	std::string::size_type delimiterLength = 4;

	if (headerEnd == std::string::npos)
	{
		headerEnd = rawRequest.find("\n\n");
		delimiterLength = 2;
	}
	if (headerEnd == std::string::npos)
		return false;

	std::string headersPart = rawRequest.substr(0, headerEnd);

	std::istringstream headerStream(headersPart);
	std::string line;

	if (!std::getline(headerStream, line))
		return false;

	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1);

	std::istringstream requestLine(line);

	if (!(requestLine >> _method >> _target >> _version))
		return false;

	while (std::getline(headerStream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		std::size_t colonPos = line.find(':');

		if (colonPos == std::string::npos)
			return false;

		std::string name = trim(line.substr(0, colonPos));
		std::string value = trim(line.substr(colonPos + 1));

		if (name.empty())
			return false;

		_headers.insert(std::make_pair(name, value));
	}

	bodyStart = headerEnd + delimiterLength;

	return true;
}

BodyFraming HttpRequest::getBodyFraming() const
{
	if (hasHeader("Transfer-Encoding"))
	{
		if (getHeader("Transfer-Encoding") == "chunked")
			return FRAMING_CHUNKED;

		return FRAMING_INVALID;
	}

	if (hasHeader("Content-Length"))
	{
		if (getContentLength() < 0)
			return FRAMING_INVALID;

		return FRAMING_CONTENT_LENGTH;
	}

	return FRAMING_NONE;
}

bool HttpRequest::parseBody(const std::string &rawRequest, std::string::size_type bodyStart)
{
	if (bodyStart >= rawRequest.size())
	{
		_body.clear();
		return true;
	}

	_body = rawRequest.substr(bodyStart);
	return true;
}
ssize_t HttpRequest::getContentLength() const
{
	if (!hasHeader("Content-Length"))
		return -1;

	const std::string &value = getHeader("Content-Length");

	if (value.empty())
		return -1;

	std::istringstream lengthStream(value);
	long long length;
	char extra;

	if (!(lengthStream >> length))
		return -1;

	if (length < 0)
		return -1;

	if (lengthStream >> extra)
		return -1;

	return static_cast<ssize_t>(length);
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
