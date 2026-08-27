#pragma once

#include <string>
enum ServerNameType
{
	EXACT,			// "example.com"
	WILDCARD_START, // "*.example.com"
	WILDCARD_END,	// "www.example.*"
	REGEX			// "~^www\d+\.example\.com$"`
};
struct ServerName
{
	std::string name;
	ServerNameType type;
};
