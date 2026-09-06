#include "ConfigParser.hpp"

#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>

/*
** Checks whether the parser has reached the end
** of the token vector.
*/
bool ConfigParser::atEnd() const
{
	return _pos >= _tokens.size();
}

/*
** Checks whether the current token is equal
** to the specified value.
*/
bool ConfigParser::check(const std::string &value) const
{
	return !atEnd() && _tokens[_pos] == value;
}

/*
** Returns the current token without moving
** the parser position.
*/
const std::string &ConfigParser::peek() const
{
	if (atEnd())
		fail("unexpected end of configuration");

	return _tokens[_pos];
}

/*
** Creates and throws a parser error.
*/
void ConfigParser::fail(const std::string &message) const
{
	std::ostringstream error;

	error << "Config error at token " << _pos;

	if (!atEnd())
		error << " ('" << _tokens[_pos] << "')";

	error << ": " << message;

	throw std::runtime_error(error.str());
}

/*
** Requires that the current token is equal
** to the expected value.
**
** If it is correct, the parser moves to
** the next token.
*/
void ConfigParser::expect(const std::string &expected)
{
	if (atEnd())
	{
		fail(
			"expected '" + expected +
			"', but reached end of file");
	}

	if (_tokens[_pos] != expected)
	{
		fail(
			"expected '" + expected +
			"', but found '" + _tokens[_pos] + "'");
	}

	++_pos;
}

/*
** Consumes one directive value.
**
** Example:
**
** root ./www ;
**      ^^^^^
*/
std::string ConfigParser::consumeValue(
	const std::string &directive)
{
	if (atEnd())
		fail("missing value for '" + directive + "'");

	const std::string value = _tokens[_pos];

	if (value == ";" || value == "{" || value == "}")
		fail("missing value for '" + directive + "'");

	++_pos;

	return value;
}

/*
** Consumes multiple values until a semicolon.
**
** Example:
**
** index index.html home.html ;
**       ^^^^^^^^^^^^^^^^^^^^
*/
std::vector<std::string> ConfigParser::consumeValueList(
	const std::string &directive)
{
	std::vector<std::string> values;

	while (!atEnd() && !check(";"))
	{
		if (check("{") || check("}"))
			fail("expected ';' after '" + directive + "'");

		values.push_back(_tokens[_pos]);
		++_pos;
	}

	if (values.empty())
	{
		fail(
			"'" + directive +
			"' requires at least one value");
	}

	expect(";");

	return values;
}

/*
** Checks that a directive appears only once
** inside the current block.
*/
void ConfigParser::ensureUnique(
	std::set<std::string> &directives,
	const std::string &directive)
{
	if (!directives.insert(directive).second)
		fail("duplicate directive '" + directive + "'");
}

/*
** Converts a string to a valid TCP port.
*/
int ConfigParser::parsePort(const std::string &value) const
{
	if (value.empty())
		fail("listen port cannot be empty");

	unsigned long port = 0;

	for (size_t i = 0; i < value.size(); ++i)
	{
		unsigned char character =
			static_cast<unsigned char>(value[i]);

		if (!std::isdigit(character))
			fail("listen port must contain only digits");

		port = port * 10 + (value[i] - '0');

		if (port > 65535)
		{
			fail(
				"listen port must be between "
				"1 and 65535");
		}
	}

	if (port == 0)
	{
		fail(
			"listen port must be between "
			"1 and 65535");
	}

	return static_cast<int>(port);
}

int ConfigParser::parseStatusCode(
	const std::string &value) const
{
	if (value.empty())
		fail("error_page status code cannot be empty");

	for (size_t i = 0; i < value.size(); ++i)
	{
		unsigned char character =
			static_cast<unsigned char>(value[i]);

		if (!std::isdigit(character))
			fail("error_page status code must contain only digits");
	}

	int code = 0;
	std::istringstream stream(value);
	stream >> code;

	if (code < 100 || code > 599)
		fail("error_page status code must be between 100 and 599");

	return code;
}

/*
** Converts body-size values such as:
**
** 100
** 10K
** 5M
** 1G
**
** into bytes.
*/
size_t ConfigParser::parseBodySize(
	const std::string &value) const
{
	if (value.empty())
		fail("client_max_body_size cannot be empty");

	size_t multiplier = 1;
	size_t numberLength = value.size();
	char suffix = value[value.size() - 1];

	if (suffix == 'k' || suffix == 'K')
	{
		multiplier = static_cast<size_t>(1024);
		--numberLength;
	}
	else if (suffix == 'm' || suffix == 'M')
	{
		multiplier =
			static_cast<size_t>(1024) *
			static_cast<size_t>(1024);

		--numberLength;
	}
	else if (suffix == 'g' || suffix == 'G')
	{
		multiplier =
			static_cast<size_t>(1024) *
			static_cast<size_t>(1024) *
			static_cast<size_t>(1024);

		--numberLength;
	}

	if (numberLength == 0)
	{
		fail(
			"client_max_body_size is "
			"missing its number");
	}

	size_t number = 0;
	size_t maxValue =
		std::numeric_limits<size_t>::max();

	for (size_t i = 0; i < numberLength; ++i)
	{
		unsigned char character =
			static_cast<unsigned char>(value[i]);

		if (!std::isdigit(character))
		{
			fail(
				"client_max_body_size must be a number "
				"with an optional K, M or G suffix");
		}

		size_t digit =
			static_cast<size_t>(value[i] - '0');

		if (number > (maxValue - digit) / 10)
			fail("client_max_body_size is too large");

		number = number * 10 + digit;
	}

	if (number > maxValue / multiplier)
		fail("client_max_body_size is too large");

	return number * multiplier;
}

/*
** ConfigParser constructor.
**
** The token vector must continue existing while
** the parser object is being used because _tokens
** is stored as a reference.
*/
ConfigParser::ConfigParser(
	const std::vector<std::string> &tokens)
	: _tokens(tokens),
	  _pos(0)
{
}

/*
** Parses the complete configuration file.
*/
std::vector<ServerConfig> ConfigParser::parse()
{
	std::vector<ServerConfig> servers;

	while (!atEnd())
	{
		if (!check("server"))
		{
			fail(
				"only server blocks are allowed "
				"at top level");
		}

		servers.push_back(parseServer());
	}

	if (servers.empty())
		fail("configuration contains no server blocks");

	return servers;
}
