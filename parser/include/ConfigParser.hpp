#pragma once

#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "Location.hpp"
#include "ServerConfig.hpp"

#include <set>
#include <string>
#include <vector>

class ConfigParser
{
private:
	const std::vector<std::string> &_tokens;
	size_t _pos;

private:
	bool atEnd() const;
	bool check(const std::string &value) const;
	const std::string &peek() const;

	void fail(const std::string &message) const;
	void expect(const std::string &expected);

	std::string consumeValue(const std::string &directive);

	std::vector<std::string> consumeValueList(
		const std::string &directive);

	void ensureUnique(
		std::set<std::string> &directives,
		const std::string &directive);

	int parsePort(const std::string &value) const;
	size_t parseBodySize(const std::string &value) const;
	int parseStatusCode(const std::string &value) const;

	void parseListen(ServerConfig &server);
	void parseErrorPage(ServerConfig &server);
	void validateMethod(const std::string &method) const;

	void parseLocationDirective(
		Location &location,
		std::set<std::string> &directives);

	void parseLocation(ServerConfig &server);

	void parseServerDirective(
		ServerConfig &server,
		std::set<std::string> &directives);

	ServerConfig parseServer();

public:
	ConfigParser(const std::vector<std::string> &tokens);

	std::vector<ServerConfig> parse();
};

#endif
