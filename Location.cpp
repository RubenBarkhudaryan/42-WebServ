#include "Location.hpp"

#include <algorithm>

Location::Location()
	: root(""),
	  autoindex(false),
	  redirectionCode(0),
	  redirection("")
{
	indexes.push_back("index.html");
}
Location::~Location() {}

Location::Location(const Location &other)
	: path(other.path), methods(other.methods), root(other.root),
	  indexes(other.indexes), autoindex(other.autoindex), redirectionCode(other.redirectionCode), redirection(other.redirection) {}

Location &Location::operator=(const Location &other)
{
	if (this != &other)
	{
		path = other.path;
		methods = other.methods;
		root = other.root;
		indexes = other.indexes;
		autoindex = other.autoindex;
		redirectionCode = other.redirectionCode;
		redirection = other.redirection;
	}
	return *this;
}

void Location::setPath(const std::string &p)
{
	path = p;
}
void Location::addMethod(const std::string &m)
{
	if (m != "GET" && m != "POST" && m != "DELETE")
		throw std::runtime_error("Error: Invalid method: " + m);
	if (std::find(methods.begin(), methods.end(), m) != methods.end())
		throw std::runtime_error("Error: Duplicate method: " + m);
	methods.push_back(m);
}

void Location::setRoot(const std::string &r)
{
	root = r;
}

std::string Location::getPath() const
{
	return path;
}

const std::vector<std::string> &Location::getMethods() const
{
	return methods;
}

std::string Location::getRoot() const
{
	return root;
}

void Location::addIndex(const std::string &i)
{
	if (std::find(indexes.begin(), indexes.end(), i) != indexes.end())
		throw std::runtime_error("Error: Duplicate index: " + i);
	indexes.push_back(i);
}

void Location::setAutoIndex(const std::string &a)
{
	autoindex = (a == "on");
}

bool Location::getAutoindex() const
{
	return autoindex;
}

std::string Location::getIndex() const
{
	if (indexes.empty())
		return "";
	return indexes[0];
}

const std::vector<std::string> &Location::getIndexes() const
{
	return indexes;
}

std::string Location::getRedirection() const
{
	return redirection;
}

void Location::setRedirection(const std::string &r)
{
	redirection = r;
}
int Location::getRedirectionCode() const
{
	return redirectionCode;
}
void Location::setRedirectionCode(int code)
{
	redirectionCode = code;
}
