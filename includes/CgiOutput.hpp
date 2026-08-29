#ifndef CGI_OUTPUT_HPP
#define CGI_OUTPUT_HPP

#include <string>


#define CGI_TIMEOUT_SECONDS 5

struct CgiOutput
{
	int         statusCode;
	std::string statusText;
	std::string contentType;
	std::string location;
	std::string setCookie;
	std::string body;

	CgiOutput();
};

#endif
