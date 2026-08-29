#ifndef LISTEN_TABLE_HPP
#define LISTEN_TABLE_HPP

#include <vector>
#include <map>
#include <string>
#include <ostream>
#include <stdint.h>
#include "ConfigStructs.hpp"

struct RealListen
{
	uint32_t	ip;
	uint16_t	port;
	bool		isWildcard;
};

class ListenTable
{
private:
	std::map<uint16_t, std::map<uint32_t, std::vector<ServerConfig*> > >	_buckets;
	std::vector<RealListen>	_realSockets;

	ListenTable(const ListenTable& ref);
	ListenTable&	operator=(const ListenTable& ref);

	void	_collectBuckets(std::vector<ServerConfig>& servers);
	void	_collectBucketsExplicit(std::vector<ServerConfig>& servers);
	void	_checkServerNameConflicts() const;
	void	_buildRealSockets();

public:
	ListenTable();
	~ListenTable();

	void	build(std::vector<ServerConfig>& servers);
	const std::vector<RealListen>&		realSockets() const;
	const std::vector<ServerConfig*>*	resolve(uint16_t port, uint32_t localIp) const;
	const ServerConfig*					chooseByHost(const std::vector<ServerConfig*>& bucket,
										const std::string& hostHeader) const;
	void								printListenSummary(std::ostream &os) const;
};

#endif
