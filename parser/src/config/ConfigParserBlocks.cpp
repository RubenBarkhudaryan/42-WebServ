#include "../../include/ConfigParser.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>

static bool isValidIPv4Host(const std::string &host)
{
	if (host.empty())
		return false;

	std::string current;
	std::vector<std::string> octets;
	std::stringstream stream(host);

	while (std::getline(stream, current, '.'))
	{
		if (current.empty())
			return false;

		octets.push_back(current);
	}

	if (octets.size() != 4)
		return false;

	for (size_t i = 0; i < octets.size(); ++i)
	{
		if (octets[i].empty())
			return false;

		for (size_t j = 0; j < octets[i].size(); ++j)
		{
			if (!std::isdigit(static_cast<unsigned char>(octets[i][j])))
				return false;
		}

		long value = 0;
		std::istringstream octetStream(octets[i]);
		octetStream >> value;

		if (value < 0 || value > 255)
			return false;
	}

	return true;
}

static void validateListenHost(const std::string &host)
{
	if (host == "localhost")
		return;

	if (isValidIPv4Host(host))
		return;

	throw std::runtime_error(
		"listen host must be 'localhost' or a valid IPv4 address");
}

/*
** Parses:
**
** listen 8080;
**
** or:
**
** listen 127.0.0.1:8080;
*/
void ConfigParser::parseListen(ServerConfig &server)
{
	std::string value = consumeValue("listen");

	expect(";");

	std::string host;
	std::string portString;

	size_t colonPosition = value.find(':');

	if (colonPosition == std::string::npos)
	{
		portString = value;
	}
	else
	{
		if (value.find(':', colonPosition + 1) !=
			std::string::npos)
		{
			fail("listen contains more than one ':'");
		}

		host = value.substr(0, colonPosition);
		portString = value.substr(colonPosition + 1);

		if (host.empty())
			fail("listen host cannot be empty");

		if (portString.empty())
			fail("listen port cannot be empty");

		validateListenHost(host);
		server.setHost(host);
	}

	server.setPort(parsePort(portString));
}

void ConfigParser::parseErrorPage(ServerConfig &server)
{
	++_pos;

	std::vector<std::string> values;

	while (!atEnd() && !check(";"))
	{
		if (check("{") || check("}"))
			fail("expected ';' after 'error_page'");

		values.push_back(_tokens[_pos]);
		++_pos;
	}

	if (values.size() < 2)
		fail("error_page requires at least one status code and a path");

	expect(";");

	std::string path = values.back();

	for (size_t i = 0; i + 1 < values.size(); ++i)
		server.addErrorPage(parseStatusCode(values[i]), path);
}

/*
** Checks whether an HTTP method is supported.
*/
void ConfigParser::validateMethod(
	const std::string &method) const
{
	if (method != "GET" &&
		method != "POST" &&
		method != "DELETE")
	{
		fail(
			"unsupported HTTP method '" +
			method +
			"'. Allowed methods: GET, POST, DELETE");
	}
}

/*
** Parses one directive inside a location block.
*/
void ConfigParser::parseLocationDirective(
	Location &location,
	std::set<std::string> &directives)
{
	const std::string directive = peek();

	if (directive == "allow_methods")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::vector<std::string> methods =
			consumeValueList(directive);

		std::set<std::string> uniqueMethods;

		for (size_t i = 0; i < methods.size(); ++i)
		{
			validateMethod(methods[i]);

			if (!uniqueMethods.insert(methods[i]).second)
			{
				fail(
					"duplicate HTTP method '" +
					methods[i] + "'");
			}

			location.addMethod(methods[i]);
		}
	}
	else if (directive == "root")
	{
		ensureUnique(directives, directive);
		++_pos;

		location.setRoot(consumeValue(directive));

		expect(";");
	}
	else if (directive == "autoindex")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::string value =
			consumeValue(directive);

		if (value != "on" && value != "off")
		{
			fail(
				"autoindex must be either "
				"'on' or 'off'");
		}

		location.setAutoIndex(value);

		expect(";");
	}
	else if (directive == "index")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::vector<std::string> indexes =
			consumeValueList(directive);

		for (size_t i = 0; i < indexes.size(); ++i)
			location.addIndex(indexes[i]);
	}
	else if (directive == "return")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::vector<std::string> values =
			consumeValueList(directive);

		if (values.size() != 2)
			fail(
				"return directive requires "
				"exactly two values");

		std::string path = values[1];
		int statusCode = parseStatusCode(values[0]);

		if (statusCode < 300 || statusCode > 399)
			fail("return status code must be between 300 and 399");

		location.setRedirectionCode(statusCode);

		std::ostringstream redirectStream;
		redirectStream << statusCode << " " << path;
		location.setRedirection(redirectStream.str());
	}
	else if (directive == "cgi_extension")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::string extension = consumeValue(directive);

		if (extension.size() < 2 || extension[0] != '.')
		{
			fail(
				"cgi_extension must start with '.' "
				"and include at least one character after it");
		}

		location.setCgiExtension(extension);

		expect(";");
	}
	else if (directive == "cgi_path")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::string cgiPath = consumeValue(directive);

		if (cgiPath.empty() || cgiPath[0] != '/')
			fail("cgi_path must be an absolute path");

		location.setCgiPath(cgiPath);

		expect(";");
	}
	else if (directive == "upload_store")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::string uploadStore = consumeValue(directive);

		if (uploadStore.empty())
			fail("upload_store cannot be empty");

		location.setUploadStore(uploadStore);

		expect(";");
	}
	else
	{
		fail(
			"unknown directive in location block: '" +
			directive + "'");
	}
}

/*
** Parses a complete location block.
*/
void ConfigParser::parseLocation(ServerConfig &server)
{
	expect("location");

	std::string path = consumeValue("location");

	if (path.empty() || path[0] != '/')
		fail("location path must start with '/'");

	expect("{");

	Location location;

	location.setPath(path);

	std::set<std::string> directives;

	while (!check("}"))
	{
		if (atEnd())
		{
			fail(
				"missing closing '}' "
				"for location block");
		}

		if (check("location"))
			fail("nested location blocks are not allowed");

		if (check("server"))
		{
			fail(
				"server block cannot be "
				"inside location");
		}

		parseLocationDirective(location, directives);
	}

	expect("}");

	server.addLocation(location);
}

/*
** Parses one directive inside a server block.
*/
void ConfigParser::parseServerDirective(
	ServerConfig &server,
	std::set<std::string> &directives)
{
	const std::string directive = peek();

	if (directive == "listen")
	{
		ensureUnique(directives, directive);
		++_pos;

		parseListen(server);
	}
	else if (directive == "root")
	{
		ensureUnique(directives, directive);
		++_pos;

		server.setRoot(consumeValue(directive));

		expect(";");
	}
	else if (directive == "server_name")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::vector<std::string> names =
			consumeValueList(directive);

		for (size_t i = 0; i < names.size(); ++i)
			server.setServerName(names[i]);
	}
	else if (directive == "error_page")
	{
		parseErrorPage(server);
	}
	else if (directive == "client_max_body_size")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::string value =
			consumeValue(directive);

		expect(";");

		server.setClientMaxBodySize(
			parseBodySize(value));
	}
	else if (directive == "index")
	{
		ensureUnique(directives, directive);
		++_pos;

		std::vector<std::string> indexes =
			consumeValueList(directive);

		for (size_t i = 0; i < indexes.size(); ++i)
			server.addIndex(indexes[i]);
	}
	else
	{
		fail(
			"unknown directive in server block: '" +
			directive + "'");
	}
}

/*
** Parses a complete server block.
*/
ServerConfig ConfigParser::parseServer()
{
	expect("server");
	expect("{");

	ServerConfig server;
	std::set<std::string> directives;

	while (!check("}"))
	{
		if (atEnd())
		{
			fail(
				"missing closing '}' "
				"for server block");
		}

		if (check("server"))
			fail("nested server blocks are not allowed");

		if (check("location"))
			parseLocation(server);
		else
			parseServerDirective(server, directives);
	}

	expect("}");

	return server;
}
