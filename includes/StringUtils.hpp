#ifndef STRING_UTILS_HPP
#define STRING_UTILS_HPP

#include <string>

class StringUtils
{
public:
	static std::string	toLower(const std::string& str);
	static std::string	toUpper(const std::string& str);
	static std::string	trimLeadingSpaces(const std::string& str);
	static bool			isNumeric(const std::string& str);
	static bool			toSizeT(const std::string& str, size_t& result);

private:
	StringUtils();
	~StringUtils();
	StringUtils(const StringUtils& ref);
	StringUtils&	operator=(const StringUtils& ref);
};

#endif
