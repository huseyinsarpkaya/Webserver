#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <ctime>
#include <stdint.h>
#include "HttpRequest.hpp"
#include "ConfigStructs.hpp"

struct ListenSocket;

class Client
{
public:
	Client();
	~Client();
	Client(const Client& ref);
	Client&	operator=(const Client& ref);

	void	resetForNextRequest();

	ListenSocket	*listenSocket;
	uint32_t		localIp;
	uint32_t		remoteIp;
	time_t			lastActivity;

	std::string		readBuffer;
	std::string		writeBuffer;
	size_t			writeOffset;

	bool			headersParsed;
	HttpRequest		request;
	size_t			contentLength;
	bool			isChunked;
	bool			keepAlive;
	ServerConfig	*matchedConfig;
	bool			closeAfterWrite;
	size_t			chunkParseOffset;
	std::string		chunkedBody;
	time_t			requestStartTime;
	bool			cgiStreaming;
	std::string		pendingSetCookie;
};

#endif
