#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include <string>
#include "ConfigStructs.hpp"
#include "Client.hpp"

class RequestHandler
{
public:
	static bool handleGet(Client &client, LocationConfig *loc, std::string &response, int &errorCode);
	static bool handlePost(Client &client, LocationConfig *loc, std::string &response, int &errorCode);
	static bool handleDelete(Client &client, LocationConfig *loc, std::string &response, int &errorCode);

	static std::string resolveFilePath(LocationConfig *loc, const std::string &path);

private:
	RequestHandler();
	~RequestHandler();
	RequestHandler(const RequestHandler &other);
	RequestHandler &operator=(const RequestHandler &other);

	static std::string joinPath(const std::string &base, LocationConfig *loc, const std::string &path);
	static std::string resolveUploadPath(LocationConfig *loc, const std::string &path);
	static bool writeUploadFile(const std::string &fullPath, const std::string &body);
	static bool buildAutoindex(const std::string &dirPath, const std::string &requestPath, std::string &out);
};

#endif
