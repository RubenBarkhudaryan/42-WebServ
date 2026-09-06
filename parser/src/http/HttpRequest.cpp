#include "../../include/http/HttpRequest.hpp"

#include <cctype>
#include <limits>

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

void HttpRequest::setBody(const std::string &body)
{
	_body = body;
}

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

ssize_t HttpRequest::getContentLength() const
{
	if (!hasHeader("Content-Length"))
		return -1;
	const std::string &value = getHeader("Content-Length");
	if (value.empty())
		return -1;
	unsigned long long result = 0;
	for (std::string::size_type i = 0; i < value.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			return -1;
		result = result * 10 + (value[i] - '0');
		if (result > static_cast<unsigned long long>(std::numeric_limits<ssize_t>::max()))
			return -1;
	}
	return static_cast<ssize_t>(result);
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
		return (getHeader("Transfer-Encoding") == "chunked") ? FRAMING_CHUNKED : FRAMING_INVALID;
	if (hasHeader("Content-Length"))
		return (getContentLength() >= 0) ? FRAMING_CONTENT_LENGTH : FRAMING_INVALID;
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

ChunkedState HttpRequest::getChunkedState(const std::string &buffer,
	std::string::size_type bodyStart)
{
	std::string::size_type pos = bodyStart;

	while (true)
	{
		std::string::size_type lineEnd = buffer.find("\r\n", pos);
		if (lineEnd == std::string::npos)
			return CHUNKED_INCOMPLETE;

		std::string sizeLine = buffer.substr(pos, lineEnd - pos);
		std::string::size_type semicolon = sizeLine.find(';');
		if (semicolon != std::string::npos)
			sizeLine = sizeLine.substr(0, semicolon);
		if (sizeLine.empty())
			return CHUNKED_MALFORMED;

		unsigned long long chunkSize = 0;
		for (std::string::size_type i = 0; i < sizeLine.size(); ++i)
		{
			char c = sizeLine[i];
			unsigned int digit;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				digit = c - 'A' + 10;
			else
				return CHUNKED_MALFORMED;
			chunkSize = chunkSize * 16 + digit;
			if (chunkSize > static_cast<unsigned long long>(std::numeric_limits<std::string::size_type>::max()))
				return CHUNKED_MALFORMED;
		}

		std::string::size_type chunkDataStart = lineEnd + 2;

		if (chunkSize == 0)
		{
			if (buffer.size() < chunkDataStart + 2)
				return CHUNKED_INCOMPLETE;
			if (buffer.compare(chunkDataStart, 2, "\r\n") != 0)
				return CHUNKED_MALFORMED;
			return CHUNKED_COMPLETE;
		}

		std::string::size_type chunkEnd = chunkDataStart + static_cast<std::string::size_type>(chunkSize);
		if (buffer.size() < chunkEnd + 2)
			return CHUNKED_INCOMPLETE;
		if (buffer.compare(chunkEnd, 2, "\r\n") != 0)
			return CHUNKED_MALFORMED;

		pos = chunkEnd + 2;
	}
}

bool HttpRequest::decodeChunkedBody(const std::string &rawChunked, std::string &decodedOut)
{
	decodedOut.clear();
	std::string::size_type pos = 0;

	while (true)
	{
		std::string::size_type lineEnd = rawChunked.find("\r\n", pos);
		if (lineEnd == std::string::npos)
			return false;

		std::string sizeLine = rawChunked.substr(pos, lineEnd - pos);
		std::string::size_type semicolon = sizeLine.find(';');
		if (semicolon != std::string::npos)
			sizeLine = sizeLine.substr(0, semicolon);
		if (sizeLine.empty())
			return false;

		unsigned long long chunkSize = 0;
		for (std::string::size_type i = 0; i < sizeLine.size(); ++i)
		{
			char c = sizeLine[i];
			unsigned int digit;
			if (c >= '0' && c <= '9')
				digit = c - '0';
			else if (c >= 'a' && c <= 'f')
				digit = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				digit = c - 'A' + 10;
			else
				return false;
			chunkSize = chunkSize * 16 + digit;
			if (chunkSize > static_cast<unsigned long long>(std::numeric_limits<std::string::size_type>::max()))
				return false;
		}

		std::string::size_type chunkDataStart = lineEnd + 2;

		if (chunkSize == 0)
		{
			if (rawChunked.size() < chunkDataStart + 2)
				return false;
			if (rawChunked.compare(chunkDataStart, 2, "\r\n") != 0)
				return false;
			return true;
		}

		std::string::size_type chunkEnd = chunkDataStart + static_cast<std::string::size_type>(chunkSize);
		if (rawChunked.size() < chunkEnd + 2)
			return false;
		if (rawChunked.compare(chunkEnd, 2, "\r\n") != 0)
			return false;

		decodedOut.append(rawChunked, chunkDataStart, static_cast<std::string::size_type>(chunkSize));
		pos = chunkEnd + 2;
	}
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
