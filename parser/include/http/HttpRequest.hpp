#pragma once

#include <map>
#include <string>
#include <sstream>
#include <algorithm>

struct HeaderNameLess
{
	bool operator()(const std::string &lhs, const std::string &rhs) const;
};
enum BodyFraming
{
	FRAMING_NONE,
	FRAMING_CONTENT_LENGTH,
	FRAMING_CHUNKED,
	FRAMING_INVALID
};
class HttpRequest
{
private:
	std::string _method;
	std::string _target;
	std::string _version;
	std::multimap<std::string, std::string, HeaderNameLess> _headers;
	std::string _body;

public:
	HttpRequest();

	void parse(const std::string &rawRequest);

	const std::string &getMethod() const;
	const std::string &getTarget() const;
	const std::string &getVersion() const;
	const std::multimap<std::string, std::string, HeaderNameLess> &getHeaders() const;
	const std::string &getBody() const;
	bool hasHeader(const std::string &name) const;
	bool parseHeaders(const std::string &rawRequest, std::string::size_type &bodyStart);
	bool parseBody(const std::string &rawRequest, std::string::size_type bodyStart);
	ssize_t getContentLength() const;
	BodyFraming getBodyFraming() const;
	const std::string &getHeader(const std::string &name) const;
};
