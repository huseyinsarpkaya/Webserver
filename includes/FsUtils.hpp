#ifndef FS_UTILS_HPP
#define FS_UTILS_HPP

#include <string>

class FsUtils
{
public:
	static bool	readWholeFile(const std::string& path, std::string& content);
	static bool	hasDotDotSegment(const std::string& path);

	static bool	exists(const std::string& path);
	static bool	isDirectory(const std::string& path);
	static bool	isRegularFile(const std::string& path);
	static bool	isReadable(const std::string& path);

private:
	FsUtils();
	~FsUtils();
	FsUtils(const FsUtils& ref);
	FsUtils&	operator=(const FsUtils& ref);
};

#endif
