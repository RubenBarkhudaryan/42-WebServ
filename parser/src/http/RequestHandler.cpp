#include "../../include/http/RequestHandler.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace
{
	bool pathIsDirectory(const std::string &path)
	{
		struct stat info;

		if (stat(path.c_str(), &info) != 0)
			return false;
		return S_ISDIR(info.st_mode);
	}

	bool pathExists(const std::string &path)
	{
		struct stat info;

		return stat(path.c_str(), &info) == 0;
	}

	bool pathIsReadable(const std::string &path)
	{
		return access(path.c_str(), R_OK) == 0;
	}
}

const Location *RequestHandler::matchLocation(const std::vector<Location> &locations, const std::string &target)
{
	const Location *best = NULL;
	std::string::size_type bestLen = 0;

	for (std::size_t i = 0; i < locations.size(); ++i)
	{
		const std::string &path = locations[i].getPath();

		if (path.empty() || target.compare(0, path.size(), path) != 0)
			continue;

		bool boundaryOk = (path.size() == target.size())
			|| (path[path.size() - 1] == '/')
			|| (target[path.size()] == '/');

		if (!boundaryOk)
			continue;

		if (best == NULL || path.size() >= bestLen)
		{
			bestLen = path.size();
			best = &locations[i];
		}
	}
	return best;
}

std::string RequestHandler::resolveRoot(const Location &location, const ServerConfig &config)
{
	std::string root = location.getRoot();

	if (root.empty())
		root = config.getRoot();
	return root;
}

std::string RequestHandler::joinPath(const std::string &root, const std::string &target)
{
	std::string result = root;

	if (!result.empty() && result[result.size() - 1] == '/')
		result.erase(result.size() - 1);

	if (target.empty() || target[0] != '/')
		result += '/';

	result += target;
	return result;
}

std::string RequestHandler::stripQuery(const std::string &target)
{
	std::string::size_type question = target.find('?');

	return (question == std::string::npos) ? target : target.substr(0, question);
}

bool RequestHandler::hasDotDotSegment(const std::string &path)
{
	std::string::size_type start = 0;

	while (start <= path.size())
	{
		std::string::size_type slash = path.find('/', start);
		std::string segment = (slash == std::string::npos)
			? path.substr(start)
			: path.substr(start, slash - start);

		if (segment == "..")
			return true;
		if (slash == std::string::npos)
			break;
		start = slash + 1;
	}
	return false;
}

std::string RequestHandler::getMimeType(const std::string &path)
{
	std::string::size_type dot = path.find_last_of('.');
	std::string ext = (dot == std::string::npos) ? "" : path.substr(dot + 1);

	if (ext == "html" || ext == "htm")
		return "text/html";
	if (ext == "css")
		return "text/css";
	if (ext == "js")
		return "application/javascript";
	if (ext == "json")
		return "application/json";
	if (ext == "png")
		return "image/png";
	if (ext == "jpg" || ext == "jpeg")
		return "image/jpeg";
	if (ext == "gif")
		return "image/gif";
	if (ext == "svg")
		return "image/svg+xml";
	if (ext == "ico")
		return "image/x-icon";
	if (ext == "txt")
		return "text/plain";
	if (ext == "pdf")
		return "application/pdf";
	return "application/octet-stream";
}

std::string RequestHandler::reasonPhrase(int code)
{
	switch (code)
	{
		case 200: return "OK";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		default: return "Unknown";
	}
}

bool RequestHandler::readFile(const std::string &path, std::string &contentOut)
{
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);

	if (!file.is_open())
		return false;

	std::ostringstream buffer;
	buffer << file.rdbuf();
	contentOut = buffer.str();
	return true;
}

HttpResponse RequestHandler::makeErrorResponse(int code, const ServerConfig &config)
{
	HttpResponse response;
	std::string reason = reasonPhrase(code);

	response.setStatus(code, reason);
	response.setHeader("Content-Type", "text/html");

	std::string body;
	bool loaded = false;

	const std::map<int, std::string> &errorPages = config.getErrorPages();
	std::map<int, std::string>::const_iterator it = errorPages.find(code);

	if (it != errorPages.end())
		loaded = readFile(joinPath(config.getRoot(), it->second), body);

	if (!loaded)
	{
		std::ostringstream fallback;
		fallback << "<html><body><h1>" << code << " " << reason << "</h1></body></html>";
		body = fallback.str();
	}

	response.setBody(body);
	return response;
}

HttpResponse RequestHandler::makeRedirectResponse(const Location &location)
{
	HttpResponse response;
	int code = location.getRedirectionCode();

	response.setStatus(code, reasonPhrase(code));

	std::string target = location.getRedirection();
	std::ostringstream prefix;
	prefix << code << " ";

	if (target.compare(0, prefix.str().size(), prefix.str()) == 0)
		target = target.substr(prefix.str().size());

	response.setHeader("Location", target);
	response.setBody("");
	return response;
}

HttpResponse RequestHandler::handle(const HttpRequest &request, const ServerConfig &config)
{
	std::string path = stripQuery(request.getTarget());

	if (hasDotDotSegment(path))
		return makeErrorResponse(400, config);

	const Location *location = matchLocation(config.getLocations(), path);

	if (!location)
		return makeErrorResponse(404, config);

	if (location->getRedirectionCode() != 0)
		return makeRedirectResponse(*location);

	const std::vector<std::string> &methods = location->getMethods();

	if (!methods.empty() &&
		std::find(methods.begin(), methods.end(), request.getMethod()) == methods.end())
		return makeErrorResponse(405, config);

	if (request.getMethod() != "GET")
		return makeErrorResponse(501, config);

	std::string root = resolveRoot(*location, config);
	std::string filePath = joinPath(root, path);

	if (pathIsDirectory(filePath))
	{
		const std::vector<std::string> &indexes = !location->getIndexes().empty()
			? location->getIndexes()
			: config.getIndexes();

		bool found = false;

		for (std::size_t i = 0; i < indexes.size() && !found; ++i)
		{
			std::string candidate = joinPath(filePath, indexes[i]);

			if (pathExists(candidate) && !pathIsDirectory(candidate))
			{
				filePath = candidate;
				found = true;
			}
		}

		if (!found)
			return makeErrorResponse(404, config);
	}

	if (!pathExists(filePath))
		return makeErrorResponse(404, config);

	if (!pathIsReadable(filePath))
		return makeErrorResponse(403, config);

	std::string content;

	if (!readFile(filePath, content))
		return makeErrorResponse(500, config);

	HttpResponse response;

	response.setStatus(200, "OK");
	response.setHeader("Content-Type", getMimeType(filePath));
	response.setBody(content);
	return response;
}
