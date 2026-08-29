#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <string>
#include "HttpRequest.hpp"

class HttpUtils
{
public:
	static std::string	statusText(int code);
	static std::string	contentTypeFor(const std::string& path);
	static bool			determineKeepAlive(const HttpRequest& req);
	static bool			splitHeaderBody(const std::string& raw, std::string& headerBlock, std::string& body);
	static bool			urlDecode(const std::string& encoded, std::string& decoded);
	static std::string	htmlEscape(const std::string& raw);
	static std::string	insertSetCookie(const std::string& response, const std::string& cookie);

private:
	HttpUtils();
	~HttpUtils();
	HttpUtils(const HttpUtils& ref);
	HttpUtils&	operator=(const HttpUtils& ref);
};

#endif
