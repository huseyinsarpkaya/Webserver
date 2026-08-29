#ifndef ROUTER_HPP
#define ROUTER_HPP

#include <string>
#include <stdint.h>
#include "ConfigStructs.hpp"
#include "ListenTable.hpp"

class Router
{
public:
	Router(const ListenTable &listenTable);
	~Router();

	ServerConfig *selectServerConfig(uint16_t port, uint32_t localIp, const std::string &hostHeader) const;

	LocationConfig *selectLocation(ServerConfig *cfg, const std::string &path) const;

	bool isMethodAllowed(LocationConfig *loc, const std::string &method) const;

private:
	const ListenTable &mListenTable;

	Router();
	Router(const Router &other);
	Router &operator=(const Router &other);

	bool isPathPrefixMatch(const std::string &path, const std::string &locPath) const;
};

#endif
