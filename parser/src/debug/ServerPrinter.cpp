#include "../../include/Location.hpp"
#include "../../include/ServerConfig.hpp"

#include <fstream>
#include <cctype>
#include <limits>
#include <sstream>

void printServers(const std::vector<ServerConfig> &servers)
{
	for (size_t i = 0; i < servers.size(); ++i)
	{
		std::cout << "==================================" << std::endl;
		std::cout << "Server " << i + 1 << std::endl;
		std::cout << "==================================" << std::endl;

		std::cout << "Host: "
				  << servers[i].getHost()
				  << std::endl;

		std::cout << "Port: "
				  << servers[i].getPort()
				  << std::endl;

		std::cout << "Root: "
				  << servers[i].getRoot()
				  << std::endl;

		std::cout << "Client max body size: "
				  << servers[i].getClientMaxBodySize()
				  << " bytes"
				  << std::endl;

		std::cout << "\nServer names:" << std::endl;

		const std::vector<ServerName> &serverNames =
			servers[i].getServerNames();

		if (serverNames.empty())
		{
			std::cout << "  No server names" << std::endl;
		}
		else
		{
			for (size_t j = 0; j < serverNames.size(); ++j)
			{
				std::cout << "  [" << j << "] "
						  << serverNames[j].name
						  << std::endl;
			}
		}

		std::cout << "\nIndexes:" << std::endl;

		const std::vector<std::string> &indexes =
			servers[i].getIndexes();

		if (indexes.empty())
		{
			std::cout << "  No indexes" << std::endl;
		}
		else
		{
			for (size_t j = 0; j < indexes.size(); ++j)
			{
				std::cout << "  [" << j << "] "
						  << indexes[j]
						  << std::endl;
			}
		}

		std::cout << "\nError pages:" << std::endl;

		const std::map<int, std::string> &errorPages =
			servers[i].getErrorPages();

		if (errorPages.empty())
		{
			std::cout << "  No error pages" << std::endl;
		}
		else
		{
			std::map<int, std::string>::const_iterator it =
				errorPages.begin();

			for (; it != errorPages.end(); ++it)
			{
				std::cout << "  [" << it->first << "] "
						  << it->second
						  << std::endl;
			}
		}

		std::cout << "\nLocations:" << std::endl;

		const std::vector<Location> &locations =
			servers[i].getLocations();

		if (locations.empty())
		{
			std::cout << "  No locations" << std::endl;
		}
		else
		{
			for (size_t j = 0; j < locations.size(); ++j)
			{
				std::cout << "\n  ------------------------------"
						  << std::endl;

				std::cout << "  Location " << j + 1
						  << std::endl;

				std::cout << "  ------------------------------"
						  << std::endl;

				std::cout << "  Path: "
						  << locations[j].getPath()
						  << std::endl;

				std::cout << "  Root: "
						  << locations[j].getRoot()
						  << std::endl;

				if (!locations[j].getCgiExtension().empty())
				{
					std::cout << "  CGI extension: "
							  << locations[j].getCgiExtension()
							  << std::endl;
				}
				else
				{
					std::cout << "  CGI extension: none"
							  << std::endl;
				}

				if (!locations[j].getCgiPath().empty())
				{
					std::cout << "  CGI path: "
							  << locations[j].getCgiPath()
							  << std::endl;
				}
				else
				{
					std::cout << "  CGI path: none"
							  << std::endl;
				}

				std::cout << "  Indexes:" << std::endl;

				const std::vector<std::string> &locationIndexes =
					locations[j].getIndexes();

				if (locationIndexes.empty())
				{
					std::cout << "    No indexes"
							  << std::endl;
				}
				else
				{
					for (size_t k = 0; k < locationIndexes.size(); ++k)
					{
						std::cout << "    [" << k << "] "
								  << locationIndexes[k]
								  << std::endl;
					}
				}

				if (locations[j].getRedirectionCode() > 0 &&
					!locations[j].getRedirection().empty())
				{
					std::cout << "  Redirect code: "
							  << locations[j].getRedirectionCode()
							  << std::endl;

					std::cout << "  Redirect: "
							  << locations[j].getRedirection()
							  << std::endl;
				}
				else
				{
					std::cout << "  Redirect: none" << std::endl;
				}

				std::cout << "  Methods:" << std::endl;

				const std::vector<std::string> &methods =
					locations[j].getMethods();

				if (methods.empty())
				{
					std::cout << "    No methods"
							  << std::endl;
				}
				else
				{
					for (size_t m = 0; m < methods.size(); ++m)
					{
						std::cout << "    [" << m << "] "
								  << methods[m]
								  << std::endl;
					}
				}
			}
		}

		std::cout << std::endl;
	}
}
