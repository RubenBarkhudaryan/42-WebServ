#pragma once

#include "./HttpRequest.hpp"
#include "./HttpResponse.hpp"
#include "./CgiProcess.hpp"
#include "../ServerConfig.hpp"
#include "../Location.hpp"

class RequestHandler
{
public:
	/*
	** When cgi is non-NULL, the request triggered a CGI execution that has
	** been started (forked) but not finished yet; `response` is a
	** placeholder and must be ignored until the caller drives `cgi` to
	** completion via poll() and calls finishCgi().
	*/
	struct HandlerResult
	{
		HttpResponse	response;
		CgiProcess		*cgi;

		HandlerResult() : response(), cgi(NULL) {}
	};

	static HandlerResult handle(const HttpRequest &request,
		const ServerConfig &config, const std::string &clientIp);
	static HttpResponse finishCgi(CgiProcess *cgi, const ServerConfig &config);
	static HttpResponse makeErrorResponse(int code, const ServerConfig &config);

private:
	static const Location *matchLocation(const std::vector<Location> &locations, const std::string &target);
	static std::string resolveRoot(const Location &location, const ServerConfig &config);
	static std::string joinPath(const std::string &root, const std::string &target);
	static std::string stripQuery(const std::string &target);
	static bool hasDotDotSegment(const std::string &path);
	static std::string getMimeType(const std::string &path);
	static std::string reasonPhrase(int code);
	static bool readFile(const std::string &path, std::string &contentOut);
	static HttpResponse makeRedirectResponse(const Location &location);
	static HttpResponse handleGet(const HttpRequest &request,
		const ServerConfig &config, const Location &location,
		const std::string &path, const std::string &filePath,
		const std::string &clientIp, CgiProcess *&cgiOut);
	static HttpResponse handlePost(const HttpRequest &request,
		const ServerConfig &config, const Location &location,
		const std::string &path, const std::string &filePath,
		const std::string &clientIp, CgiProcess *&cgiOut);
	static HttpResponse handleDelete(const ServerConfig &config,
		const std::string &filePath);
	static CgiProcess *startCgi(const HttpRequest &request,
		const ServerConfig &config, const Location &location,
		const std::string &path, const std::string &filePath,
		const std::string &clientIp, HttpResponse &errorOut);
};
