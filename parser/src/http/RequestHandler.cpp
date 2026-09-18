#include "../../include/http/RequestHandler.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>

#include <cstddef>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>

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

	bool pathIsWritable(const std::string &path)
	{
		return access(path.c_str(), W_OK) == 0;
	}

	bool hasExtension(const std::string &path, const std::string &extension)
	{
		if (extension.empty() || path.size() < extension.size())
			return false;
		return path.compare(path.size() - extension.size(), extension.size(),
			extension) == 0;
	}

	std::string htmlEscape(const std::string &value)
	{
		std::string result;
		for (std::string::size_type i = 0; i < value.size(); ++i)
		{
			switch (value[i])
			{
				case '&': result += "&amp;"; break;
				case '<': result += "&lt;"; break;
				case '>': result += "&gt;"; break;
				case '"': result += "&quot;"; break;
				default: result += value[i];
			}
		}
		return result;
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
		case 409: return "Conflict";
		case 413: return "Payload Too Large";
		case 415: return "Unsupported Media Type";
		case 422: return "Unprocessable Entity";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
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

namespace
{
	std::string httpHeaderEnvName(const std::string &headerName)
	{
		std::string name = "HTTP_";

		for (std::string::size_type i = 0; i < headerName.size(); ++i)
		{
			char c = headerName[i];
			name += (c == '-') ? '_' : static_cast<char>(
				std::toupper(static_cast<unsigned char>(c)));
		}
		return name;
	}
}

CgiProcess *RequestHandler::startCgi(const HttpRequest &request,
	const ServerConfig &config, const Location &location,
	const std::string &path, const std::string &filePath,
	const std::string &clientIp, HttpResponse &errorOut)
{
	int inputPipe[2];
	int outputPipe[2];

	if (pipe(inputPipe) != 0)
	{
		errorOut = makeErrorResponse(500, config);
		return NULL;
	}
	if (pipe(outputPipe) != 0)
	{
		close(inputPipe[0]); close(inputPipe[1]);
		errorOut = makeErrorResponse(500, config);
		return NULL;
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		close(inputPipe[0]); close(inputPipe[1]);
		close(outputPipe[0]); close(outputPipe[1]);
		errorOut = makeErrorResponse(500, config);
		return NULL;
	}

	if (pid == 0)
	{
		std::string length;
		std::ostringstream lengthStream;
		lengthStream << request.getBody().size();
		length = lengthStream.str();

		std::string::size_type questionMark = request.getTarget().find('?');
		std::string queryString = (questionMark == std::string::npos)
			? "" : request.getTarget().substr(questionMark + 1);

		std::ostringstream portStream;
		portStream << config.getPort();

		setenv("REQUEST_METHOD", request.getMethod().c_str(), 1);
		setenv("QUERY_STRING", queryString.c_str(), 1);
		setenv("CONTENT_LENGTH", length.c_str(), 1);
		setenv("CONTENT_TYPE", request.getHeader("Content-Type").c_str(), 1);
		setenv("SCRIPT_NAME", path.c_str(), 1);
		setenv("SERVER_PROTOCOL", request.getVersion().c_str(), 1);
		setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
		setenv("SERVER_SOFTWARE", "webserv/1.0", 1);
		setenv("SERVER_NAME", config.getHost().c_str(), 1);
		setenv("SERVER_PORT", portStream.str().c_str(), 1);
		setenv("REQUEST_URI", path.c_str(), 1);
		setenv("PATH_INFO", path.c_str(), 1);

		setenv("REDIRECT_STATUS", "200", 1);
		if (!clientIp.empty())
			setenv("REMOTE_ADDR", clientIp.c_str(), 1);

		const std::multimap<std::string, std::string, HeaderNameLess> &headers =
			request.getHeaders();
		std::multimap<std::string, std::string, HeaderNameLess>::const_iterator headerIt;
		for (headerIt = headers.begin(); headerIt != headers.end(); ++headerIt)
			setenv(httpHeaderEnvName(headerIt->first).c_str(),
				headerIt->second.c_str(), 1);

		std::string scriptDir = ".";
		std::string scriptFile = filePath;
		std::string::size_type lastSlash = filePath.find_last_of('/');
		if (lastSlash != std::string::npos)
		{
			scriptDir = filePath.substr(0, lastSlash);
			scriptFile = filePath.substr(lastSlash + 1);
		}
		if (scriptDir.empty())
			scriptDir = "/";
		if (chdir(scriptDir.c_str()) != 0)
			_exit(127);

		dup2(inputPipe[0], STDIN_FILENO);
		dup2(outputPipe[1], STDOUT_FILENO);
		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
		execl(location.getCgiPath().c_str(), location.getCgiPath().c_str(),
			scriptFile.c_str(), static_cast<char *>(NULL));
		_exit(127);
	}

	close(inputPipe[0]);
	close(outputPipe[1]);

	return new CgiProcess(pid, inputPipe[1], outputPipe[0], request.getBody());
}

HttpResponse RequestHandler::finishCgi(CgiProcess *cgi, const ServerConfig &config)
{
	int status = cgi->getExitStatus();

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return makeErrorResponse(502, config);

	const std::string &output = cgi->getOutput();

	HttpResponse response;
	std::string::size_type separator = output.find("\r\n\r\n");
	std::string::size_type separatorLength = 4;
	if (separator == std::string::npos)
	{
		separator = output.find("\n\n");
		separatorLength = 2;
	}
	if (separator == std::string::npos)
		return makeErrorResponse(502, config);

	std::istringstream headers(output.substr(0, separator));
	std::string line;
	int statusCode = 200;
	std::string statusMessage = "OK";
	while (std::getline(headers, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		std::string::size_type colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
			value.erase(0, 1);
		if (name == "Status")
		{
			std::istringstream statusStream(value);
			statusStream >> statusCode;
			std::getline(statusStream, statusMessage);
			while (!statusMessage.empty() && statusMessage[0] == ' ')
				statusMessage.erase(0, 1);
		}
		else
			response.setHeader(name, value);
	}
	response.setStatus(statusCode, statusMessage);
	response.setBody(output.substr(separator + separatorLength));
	return response;
}

HttpResponse RequestHandler::handleGet(const HttpRequest &request,
	const ServerConfig &config, const Location &location,
	const std::string &path, const std::string &filePath,
	const std::string &clientIp, CgiProcess *&cgiOut)
{
	std::string selectedPath = filePath;
	if (pathIsDirectory(selectedPath))
	{
		const std::vector<std::string> &indexes = !location.getIndexes().empty()
			? location.getIndexes() : config.getIndexes();
		for (std::size_t i = 0; i < indexes.size(); ++i)
		{
			std::string candidate = joinPath(selectedPath, indexes[i]);
			if (pathExists(candidate) && !pathIsDirectory(candidate))
			{
				selectedPath = candidate;
				break;
			}
		}
		if (pathIsDirectory(selectedPath) && location.getAutoindex())
		{
			std::ostringstream body;
			body << "<html><body><h1>Index of " << htmlEscape(path)
				<< "</h1><ul>";
			DIR *directory = opendir(selectedPath.c_str());
			if (directory == NULL)
				return makeErrorResponse(403, config);
			struct dirent *entry;
			while ((entry = readdir(directory)) != NULL)
			{
				std::string name(entry->d_name);
				if (name == "." || name == "..")
					continue;
				body << "<li><a href=\"" << htmlEscape(name) << "\">"
					<< htmlEscape(name) << "</a></li>";
			}
			closedir(directory);
			body << "</ul></body></html>";
			HttpResponse response;
			response.setStatus(200, "OK");
			response.setHeader("Content-Type", "text/html");
			response.setBody(body.str());
			return response;
		}
		if (pathIsDirectory(selectedPath))
			return makeErrorResponse(404, config);
	}

	if (hasExtension(selectedPath, location.getCgiExtension()))
	{
		HttpResponse errorResponse;
		CgiProcess *cgi = startCgi(request, config, location, path,
			selectedPath, clientIp, errorResponse);
		if (!cgi)
			return errorResponse;
		cgiOut = cgi;
		return HttpResponse();
	}

	if (!pathExists(selectedPath))
		return makeErrorResponse(404, config);
	if (pathIsDirectory(selectedPath) || !pathIsReadable(selectedPath))
		return makeErrorResponse(403, config);
	std::string content;
	if (!readFile(selectedPath, content))
		return makeErrorResponse(500, config);
	HttpResponse response;
	response.setStatus(200, "OK");
	response.setHeader("Content-Type", getMimeType(selectedPath));
	response.setBody(content);
	return response;
}

HttpResponse RequestHandler::handlePost(const HttpRequest &request,
	const ServerConfig &config, const Location &location,
	const std::string &path, const std::string &filePath,
	const std::string &clientIp, CgiProcess *&cgiOut)
{
	if (hasExtension(filePath, location.getCgiExtension()))
	{
		HttpResponse errorResponse;
		CgiProcess *cgi = startCgi(request, config, location, path,
			filePath, clientIp, errorResponse);
		if (!cgi)
			return errorResponse;
		cgiOut = cgi;
		return HttpResponse();
	}

	std::string outputPath = filePath;
	if (!location.getUploadStore().empty())
	{
		std::string uploadName = path.substr(path.find_last_of('/') + 1);
		if (uploadName.empty())
		{
			std::ostringstream generatedName;
			generatedName << "upload_" << getpid() << "_" << time(NULL);
			uploadName = generatedName.str();
		}
		outputPath = joinPath(location.getUploadStore(), uploadName);
	}
	else if (pathIsDirectory(outputPath))
		outputPath = joinPath(outputPath, "upload");

	bool existed = pathExists(outputPath);
	std::ofstream file(outputPath.c_str(), std::ios::out | std::ios::binary |
		std::ios::trunc);
	if (!file.is_open())
		return makeErrorResponse(403, config);
	file.write(request.getBody().data(), request.getBody().size());
	if (!file.good())
		return makeErrorResponse(500, config);
	HttpResponse response;
	response.setStatus(existed ? 200 : 201, existed ? "OK" : "Created");
	response.setHeader("Content-Type", "text/plain");
	response.setBody("Request body stored.\n");
	return response;
}

HttpResponse RequestHandler::handleDelete(const ServerConfig &config,
	const std::string &filePath)
{
	if (!pathExists(filePath))
		return makeErrorResponse(404, config);
	if (pathIsDirectory(filePath) || !pathIsWritable(filePath))
		return makeErrorResponse(403, config);
	if (unlink(filePath.c_str()) != 0)
		return makeErrorResponse(errno == EACCES ? 403 : 500, config);
	HttpResponse response;
	response.setStatus(204, "No Content");
	response.setBody("");
	return response;
}

std::size_t RequestHandler::resolveMaxBodySize(const std::string &target,
	const ServerConfig &config)
{
	std::string path = stripQuery(target);
	const Location *location = matchLocation(config.getLocations(), path);

	if (location && location->hasOwnClientMaxBodySize())
		return location->getClientMaxBodySize();
	return config.getClientMaxBodySize();
}

RequestHandler::HandlerResult RequestHandler::handle(const HttpRequest &request,
	const ServerConfig &config, const std::string &clientIp)
{
	HandlerResult result;
	std::string path = stripQuery(request.getTarget());

	if (hasDotDotSegment(path))
	{
		result.response = makeErrorResponse(400, config);
		return result;
	}

	const Location *location = matchLocation(config.getLocations(), path);

	if (!location)
	{
		result.response = makeErrorResponse(404, config);
		return result;
	}

	if (location->getRedirectionCode() != 0)
	{
		result.response = makeRedirectResponse(*location);
		return result;
	}

	const std::vector<std::string> &methods = location->getMethods();

	if (!methods.empty() &&
		std::find(methods.begin(), methods.end(), request.getMethod()) == methods.end())
	{
		HttpResponse response = makeErrorResponse(405, config);
		std::string allow;
		for (std::size_t i = 0; i < methods.size(); ++i)
		{
			if (!allow.empty())
				allow += ", ";
			allow += methods[i];
		}
		response.setHeader("Allow", allow);
		result.response = response;
		return result;
	}

	if (request.getMethod() != "GET" && request.getMethod() != "POST"
		&& request.getMethod() != "DELETE")
	{
		result.response = makeErrorResponse(501, config);
		return result;
	}
	std::size_t maxBodySize = location->hasOwnClientMaxBodySize()
		? location->getClientMaxBodySize() : config.getClientMaxBodySize();

	if (request.getBody().size() > maxBodySize)
	{
		result.response = makeErrorResponse(413, config);
		return result;
	}

	std::string root = resolveRoot(*location, config);
	std::string relativePath = path;
	if (!location->getRoot().empty() && location->getPath() != "/")
		relativePath = path.substr(location->getPath().size());
	std::string filePath = joinPath(root, relativePath);

	CgiProcess *cgi = NULL;
	if (request.getMethod() == "GET")
		result.response = handleGet(request, config, *location, path, filePath, clientIp, cgi);
	else if (request.getMethod() == "POST")
		result.response = handlePost(request, config, *location, path, filePath, clientIp, cgi);
	else
		result.response = handleDelete(config, filePath);

	result.cgi = cgi;
	return result;
}
