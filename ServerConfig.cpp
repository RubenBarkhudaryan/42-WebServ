#include "Location.hpp"
#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : host("0.0.0.0"), port(8080), client_max_body_size(1024 * 1024) {}

ServerConfig::~ServerConfig() {}

ServerConfig::ServerConfig(const ServerConfig &other)
	: host(other.host), port(other.port), server_names(other.server_names), client_max_body_size(other.client_max_body_size), indexes(other.indexes),
	  error_pages(other.error_pages), locations(other.locations), root(other.root) {}
ServerConfig &ServerConfig::operator=(const ServerConfig &other)
{
	if (this != &other)
	{
		server_names = other.server_names;
		port = other.port;
		host = other.host;
		client_max_body_size = other.client_max_body_size;
		error_pages = other.error_pages;
		locations = other.locations;
		root = other.root;
	}
	return *this;
}

void ServerConfig::addLocation(const Location &loc)
{
	locations.push_back(loc);
}

const std::string &ServerConfig::getHost() const
{
	return host;
}

int ServerConfig::getPort() const
{
	return port;
}

void ServerConfig::setPort(int p)
{
	port = p;
}
void ServerConfig::setHost(const std::string &h)
{
	host = h;
}

const std::vector<Location> &ServerConfig::getLocations() const
{
	return locations;
}
static ServerNameType determineServerNameType(const std::string &name)
{
	if (name.empty())
		return EXACT;

	if (name[0] == '*')
		return WILDCARD_START;
	else if (name[name.length() - 1] == '*')
		return WILDCARD_END;
	else if (name[0] == '~')
		return REGEX;
	else
		return EXACT;
}
void ServerConfig::setServerName(const std::string &h)
{
	ServerName server_name;
	server_name.name = h;
	server_name.type = determineServerNameType(h);
	server_names.push_back(server_name);
}
std::vector<ServerName> ServerConfig::getServerNames() const
{
	return server_names;
}
void ServerConfig::setClientMaxBodySize(size_t size)
{
	client_max_body_size = size;
}

size_t ServerConfig::getClientMaxBodySize() const
{
	return client_max_body_size;
}

void ServerConfig::setRoot(const std::string &r)
{
	root = r;
}

const std::string &ServerConfig::getRoot() const
{
	return root;
}

const std::vector<std::string> &ServerConfig::getIndexes() const
{
	return indexes;
}
void ServerConfig::addIndex(const std::string &index)
{
	if (std::find(indexes.begin(), indexes.end(), index) != indexes.end())
		throw std::runtime_error("Error: Duplicate index: " + index);
	indexes.push_back(index);
}

void ServerConfig::addErrorPage(int code, const std::string &path)
{
	if (error_pages.find(code) != error_pages.end())
	{
		std::stringstream ss;
		ss << code;

		std::string error_msg = "Error: Duplicate error page for code: " + ss.str();
		throw std::runtime_error(error_msg);
	}
	error_pages[code] = path;
}

const std::map<int, std::string> &ServerConfig::getErrorPages() const
{
	return error_pages;
}
