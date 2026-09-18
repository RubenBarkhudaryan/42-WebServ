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

enum ChunkedState
{
	CHUNKED_INCOMPLETE,
	CHUNKED_COMPLETE,
	CHUNKED_MALFORMED
};

/*
** HEADERS_INCOMPLETE: the header/body delimiter hasn't arrived yet, so more
** data may still make this a valid request - keep waiting.
** HEADERS_MALFORMED: the delimiter arrived, so the whole header block was
** available to parse, and it's invalid (bad request line, header with no
** colon, ...) - no amount of extra data fixes this, treat it as a 400 now.
*/
enum HeaderParseStatus
{
	HEADERS_INCOMPLETE,
	HEADERS_MALFORMED,
	HEADERS_OK
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

	const std::string &getMethod() const;
	const std::string &getTarget() const;
	const std::string &getVersion() const;
	const std::multimap<std::string, std::string, HeaderNameLess> &getHeaders() const;
	const std::string &getBody() const;
	ssize_t getContentLength() const;
	void setBody(const std::string &body);
	static ChunkedState getChunkedState(const std::string &buffer, std::string::size_type bodyStart);
	static bool decodeChunkedBody(const std::string &rawChunked, std::string &decodedBody);
	bool hasHeader(const std::string &name) const;
	HeaderParseStatus parseHeaders(const std::string &rawRequest, std::string::size_type &bodyStart);
	bool parseBody(const std::string &rawRequest, std::string::size_type bodyStart);
	BodyFraming getBodyFraming() const;
	const std::string &getHeader(const std::string &name) const;
};
