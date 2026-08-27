#include "Location.hpp"
#include "ServerConfig.hpp"
#include "ConfigParser.hpp"
#include <fstream>
#include <cctype>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Forward declaration
void printServers(const std::vector<ServerConfig> &servers);

std::vector<ServerConfig> validation_config(
	const std::vector<std::string> &tokens)
{
	ConfigParser parser(tokens);
	return parser.parse();
}

std::vector<std::string> tokenizeConfig(std::istream &input)
{
	std::vector<std::string> tokens;
	std::string line;

	while (std::getline(input, line))
	{
		size_t commentPosition = std::string::npos;

		for (size_t i = 0; i < line.size(); ++i)
		{
			if (line[i] == '#')
			{
				if (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))
				{
					commentPosition = i;
					break;
				}
			}
		}

		if (commentPosition != std::string::npos)
			line.erase(commentPosition);

		std::string preparedLine;

		for (size_t i = 0; i < line.size(); ++i)
		{
			char character = line[i];

			if (character == '{' ||
				character == '}' ||
				character == ';')
			{
				preparedLine += ' ';
				preparedLine += character;
				preparedLine += ' ';
			}
			else
			{
				preparedLine += character;
			}
		}

		std::stringstream stream(preparedLine);
		std::string token;

		while (stream >> token)
			tokens.push_back(token);
	}

	return tokens;
}

std::vector<ServerConfig> parse_file(const std::string &filename)
{
	std::ifstream configFile(filename.c_str());

	if (!configFile.is_open())
		throw std::runtime_error(
			"Failed to open config file: " + filename);

	std::vector<std::string> tokens = tokenizeConfig(configFile);

	if (tokens.empty())
		throw std::runtime_error("Configuration file is empty");

	return validation_config(tokens);
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr
			<< "Usage: "
			<< argv[0]
			<< " <config_file>"
			<< std::endl;

		return 1;
	}

	try
	{
		std::vector<ServerConfig> servers = parse_file(argv[1]);

		printServers(servers);
	}
	catch (const std::exception &exception)
	{
		std::cerr
			<< "Error: "
			<< exception.what()
			<< std::endl;

		return 1;
	}

	return 0;
}
