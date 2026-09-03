#pragma once

#include <map>
#include <string>
#include <sstream>

class HttpRequest
{
private:
	std::string _method;
	std::string _target;
	std::string _version;
	std::multimap<std::string, std::string> _headers;
	std::string _body;

public:
	HttpRequest();

	void parse(const std::string &rawRequest);

	const std::string &getMethod() const;
	const std::string &getTarget() const;
	const std::string &getVersion() const;
	const std::multimap<std::string, std::string> &getHeaders() const;
	const std::string &getBody() const;
	bool hasHeader(const std::string &name) const;
	const std::string &getHeader(const std::string &name) const;
};
