#pragma once
#include "./Location.hpp"
#include "./ServerName.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>

class ServerConfig
{
private:
	std::string host;
	int port;
	std::vector<ServerName> server_names;
	size_t client_max_body_size;
	std::vector<std::string> indexes;
	std::map<int, std::string> error_pages;
	std::vector<Location> locations;
	std::string root;

public:
	ServerConfig();
	~ServerConfig();
	ServerConfig(const ServerConfig &other);
	ServerConfig &operator=(const ServerConfig &other);

	void setServerName(const std::string &h);
	void setClientMaxBodySize(size_t size);
	void setHost(const std::string &h);
	void setPort(int p);
	void setRoot(const std::string &r);
	void addLocation(const Location &loc);
	void addErrorPage(int code, const std::string &path);
	void addIndex(const std::string &index);

	// Геттеры (для Роли 1 и Роли 3)
	std::vector<ServerName> getServerNames() const;
	int getPort() const;
	const std::map<int, std::string> &getErrorPages() const;
	const std::string &getHost() const;
	const std::string &getRoot() const;
	size_t getClientMaxBodySize() const;
	const std::vector<Location> &getLocations() const;
	const std::vector<std::string> &getIndexes() const;
};
