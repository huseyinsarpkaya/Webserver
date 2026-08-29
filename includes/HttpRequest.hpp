#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>

class HttpRequest
{
public:
	HttpRequest();
	~HttpRequest();
	HttpRequest(const HttpRequest& ref);
	HttpRequest&	operator=(const HttpRequest& ref);

	bool	parseRequestLine(const std::string& line);
	bool	parseHeaderLines(const std::string& headerBlock);

	const std::string&	getMethod() const;
	const std::string&	getPath() const;
	const std::string&	getQuery() const;
	const std::string&	getVersion() const;
	const std::map<std::string, std::string>&	getHeaders() const;
	std::string	getHeader(const std::string& name) const;

private:
	std::string	_method;
	std::string	_path;
	std::string	_query;
	std::string	_version;
	std::map<std::string, std::string>	_headers;
};

#endif
