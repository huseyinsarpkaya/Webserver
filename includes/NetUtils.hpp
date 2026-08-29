#ifndef NET_UTILS_HPP
#define NET_UTILS_HPP

#include <string>
#include <stdint.h>

class NetUtils
{
public:
	static std::string	ipToString(uint32_t ip);

	static bool			resolveIPv4(const std::string& host, uint32_t& outIp);

	static bool			setNonBlocking(int fd);

private:
	NetUtils();
	~NetUtils();
	NetUtils(const NetUtils& ref);
	NetUtils&	operator=(const NetUtils& ref);
};

#endif
