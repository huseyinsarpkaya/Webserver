#include "ListenTable.hpp"
#include "NetUtils.hpp"
#include "StringUtils.hpp"
#include <stdexcept>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>

ListenTable::ListenTable()
{
}

ListenTable::~ListenTable()
{
}

ListenTable::ListenTable(const ListenTable& ref)
{
	(void)ref;
}

ListenTable&	ListenTable::operator=(const ListenTable& ref)
{
	(void)ref; return (*this);
}

void	ListenTable::build(std::vector<ServerConfig>& servers)
{
	_buckets.clear();
	_realSockets.clear();

	_collectBuckets(servers);
	_checkServerNameConflicts();
	_buildRealSockets();
}

void	ListenTable::_collectBuckets(std::vector<ServerConfig>& servers)
{
	for (size_t s = 0; s < servers.size(); s++)
	{
		ServerConfig&	server = servers[s];

		for (size_t i = 0; i < server.listens.size(); i++)
		{
			const ListenConfig&	listen = server.listens[i];

			_buckets[listen.port][listen.ip].push_back(&server);
		}
	}
}

void	ListenTable::_checkServerNameConflicts() const
{
	std::map<uint16_t, std::map<uint32_t, std::vector<ServerConfig*> > >::const_iterator	portIt;

	for (portIt = _buckets.begin(); portIt != _buckets.end(); ++portIt)
	{
		std::map<uint32_t, std::vector<ServerConfig*> >::const_iterator	ipIt;

		for (ipIt = portIt->second.begin(); ipIt != portIt->second.end(); ++ipIt)
		{
			const std::vector<ServerConfig*>&	bucket = ipIt->second;

			for (size_t a = 0; a < bucket.size(); a++)
			{
				for (size_t b = a + 1; b < bucket.size(); b++)
				{
					if (bucket[a]->serverName == bucket[b]->serverName)
					{
						std::ostringstream	oss;
						oss << "Config: " << NetUtils::ipToString(ipIt->first)
							<< ":" << ntohs(portIt->first)
							<< " duplicate server_name '"
							<< bucket[a]->serverName << "' (server id="
							<< bucket[a]->id + 1 << " and id=" << bucket[b]->id + 1<< ")";
						throw std::runtime_error(oss.str());
					}
				}
			}
		}
	}
}

void	ListenTable::_buildRealSockets()
{
	std::map<uint16_t, std::map<uint32_t, std::vector<ServerConfig*> > >::iterator	portIt;

	for (portIt = _buckets.begin(); portIt != _buckets.end(); ++portIt)
	{
		uint16_t	port = portIt->first;
		bool		hasWildcard = (portIt->second.find(INADDR_ANY) != portIt->second.end());

		if (hasWildcard)
		{
			RealListen	realListen;

			realListen.ip = INADDR_ANY;
			realListen.port = port;
			realListen.isWildcard = true;
			_realSockets.push_back(realListen);
		}
		else
		{
			std::map<uint32_t, std::vector<ServerConfig*> >::iterator	ipIt;

			for (ipIt = portIt->second.begin(); ipIt != portIt->second.end(); ++ipIt)
			{
				RealListen	realListen;

				realListen.ip = ipIt->first;
				realListen.port = port;
				realListen.isWildcard = false;
				_realSockets.push_back(realListen);
			}
		}
	}
}

const std::vector<RealListen>&	ListenTable::realSockets() const
{
	return (_realSockets);
}

const std::vector<ServerConfig*>*	ListenTable::resolve(uint16_t port, uint32_t localIp) const
{
	std::map<uint16_t, std::map<uint32_t, std::vector<ServerConfig*> > >::const_iterator	portIt;

	portIt = _buckets.find(port);
	if (portIt == _buckets.end())
		return (NULL);

	std::map<uint32_t, std::vector<ServerConfig*> >::const_iterator	ipIt;
	ipIt = portIt->second.find(localIp);
	if (ipIt != portIt->second.end())
		return (&ipIt->second);

	ipIt = portIt->second.find(INADDR_ANY);
	if (ipIt != portIt->second.end())
		return (&ipIt->second);

	return (NULL);
}

const ServerConfig*	ListenTable::chooseByHost(const std::vector<ServerConfig*>& bucket,
											const std::string& hostHeader) const
{
	if (bucket.empty())
		return (NULL);

	if (!hostHeader.empty())
	{
		for (size_t i = 0; i < bucket.size(); i++)
		{
			if (StringUtils::toLower(bucket[i]->serverName) == StringUtils::toLower(hostHeader))
				return (bucket[i]);
		}
	}
	return (bucket[0]);
}

static void	printBucketServerNames(std::ostream &os, const std::vector<ServerConfig*> &bucket)
{
	for (size_t i = 0; i < bucket.size(); i++)
	{
		if (i > 0)
			os << ", ";
		os << (bucket[i]->serverName.empty() ? "(default)" : bucket[i]->serverName);
	}
}

void	ListenTable::printListenSummary(std::ostream &os) const
{
	for (size_t i = 0; i < _realSockets.size(); i++)
	{
		const RealListen	&realListen = _realSockets[i];
		std::map<uint16_t, std::map<uint32_t, std::vector<ServerConfig*> > >::const_iterator	portIt
			= _buckets.find(realListen.port);

		os << "Listening on "
			<< NetUtils::ipToString(realListen.ip) << ":"
			<< ntohs(realListen.port);

		if (portIt == _buckets.end())
		{
			os << std::endl;
			continue ;
		}

		if (!realListen.isWildcard)
		{
			std::map<uint32_t, std::vector<ServerConfig*> >::const_iterator	ipIt = portIt->second.find(realListen.ip);
			os << " -> ";
			if (ipIt != portIt->second.end())
				printBucketServerNames(os, ipIt->second);
			os << std::endl;
			continue;
		}

		if (portIt->second.size() == 1)
		{
			os << " -> ";
			printBucketServerNames(os, portIt->second.begin()->second);
			os << std::endl;
		}
		else
		{
			os << std::endl;
			std::map<uint32_t, std::vector<ServerConfig*> >::const_iterator	ipIt;
			for (ipIt = portIt->second.begin(); ipIt != portIt->second.end(); ++ipIt)
			{
				os << "    " << NetUtils::ipToString(ipIt->first) << ":" << ntohs(realListen.port) << " -> ";
				printBucketServerNames(os, ipIt->second);
				os << std::endl;
			}
		}
	}
}
