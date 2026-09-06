#pragma once

#include "./HttpRequest.hpp"
#include "./HttpResponse.hpp"
#include "../ServerConfig.hpp"
#include "../Location.hpp"

class RequestHandler
{
public:
	static HttpResponse handle(const HttpRequest &request, const ServerConfig &config);

private:
	static const Location *matchLocation(const std::vector<Location> &locations, const std::string &target);
	static std::string resolveRoot(const Location &location, const ServerConfig &config);
	static std::string joinPath(const std::string &root, const std::string &target);
	static std::string stripQuery(const std::string &target);
	static bool hasDotDotSegment(const std::string &path);
	static std::string getMimeType(const std::string &path);
	static std::string reasonPhrase(int code);
	static bool readFile(const std::string &path, std::string &contentOut);
	static HttpResponse makeErrorResponse(int code, const ServerConfig &config);
	static HttpResponse makeRedirectResponse(const Location &location);
};
