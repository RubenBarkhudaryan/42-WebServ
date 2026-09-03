#include "./sockets/engine/engine.hpp"
#include "./sockets/server/server.hpp"
#include "./parser/include/ServerConfig.hpp"
#include "./parser/include/Location.hpp"
#include "./parser/include/ConfigParser.hpp"

#include <fstream>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <vector>

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
		if (argc > 2)
			std::cerr << "[WebServ]: too many arguments for running." << std::endl;
		else
			std::cerr << "[WebServ]: too few arguments for running." << std::endl;

		std::cerr << "[WebServ]: Usage: ./webserv <config_file>" << std::endl;
		return (EXIT_FAILURE);
	}
	try
	{
		std::vector<ServerConfig>	configs = parse_file(argv[1]);

		Engine	engine;

		for (std::size_t i = 0; i < configs.size(); ++i)
			engine.addServer(new Server(configs[i]));

		engine.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "[WebServ]: fatal error: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}