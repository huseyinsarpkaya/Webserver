#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <map>
#include <ctime>
#include <poll.h>
#include <sys/types.h>
#include "ConfigStructs.hpp"
#include "ListenTable.hpp"
#include "Client.hpp"
#include "Router.hpp"
#include "RequestHandler.hpp"

enum ChunkResult
{
    CHUNK_INCOMPLETE,
    CHUNK_COMPLETE,
    CHUNK_INVALID
};

struct ListenSocket
{
    int         fd;
    uint32_t    ip;
    uint16_t    port;
};

struct CgiSession
{
	pid_t       pid;
	int         clientFd;
	int         inFd;
	int         outFd;
	std::string body;
	size_t      written;
	std::string output;
	time_t      start;
	time_t      lastActivity;
	bool        headersDone;
};

class Server
{
public:
    Server(const ListenTable &listenTable);
    ~Server();
    void setupSockets();
    void run();

private:
    std::vector<struct pollfd> mPollFds;
    const ListenTable &mListenTable;
    std::vector<ListenSocket> mListenSockets;
    std::map<int, Client> mClients;
    Router mRouter;
    std::map<int, CgiSession> mCgiSessions;
    std::map<int, int> mCgiPipeOwner;

    Server();
    Server(const Server &other);
    Server &operator=(const Server &other);

    int createListenSocket(const RealListen &rl);

    bool isListeningSocket(int fd) const;
    ListenSocket *findListenSocket(int fd);
    void acceptNewClient(int listenFd);
    bool readClientData(int fd);
    bool writeClientData(int fd);
    void addListeningSockets();
    void listeningSockets();
	void processClient(int fd);
	bool tryParseHeaders(int fd);
	bool isBodyComplete(int fd);
	void finalizeRequest(int fd);
	ServerConfig *selectServerConfig(int fd);
    void sendErrorAndCleanup(int fd, int statusCode);
    void sendResponseAndCleanup(int fd, const std::string &response);
    void closeClient(size_t i);
	void removePollFd(int fd);
	void ensurePollout(int fd);
	bool isPollin(size_t i);
	bool isPollout(size_t i);
	bool isClientTimedOut(size_t i);
	bool updateDrainState(bool &draining, time_t &drainStart);
	void controlMethod(Client &client, LocationConfig *loc, int fd);
	bool isCgiRequest(LocationConfig *loc, const std::string &path);
	void cgiHandle(Client &client, LocationConfig *loc, int fd);
	void registerCgiSession(pid_t pid, int fd, int inPipe[2], int outPipe[2],
			std::string &body);
	bool isCgiPipe(int pollFd) const;
	bool handleCgiPipeEvent(size_t i);
	void closeCgiFds(CgiSession &session);
	void removePipeFromPoll(int pipeFd);
	void setClientPollEvents(int clientFd, short events);
	void checkCgiTimeouts();
	void abortCgi(int clientFd);
	void writeBodyToCgi(CgiSession &session, short revents);
	void readCgiOutput(std::map<int, CgiSession>::iterator sessIt);
	void finishCgiSession(std::map<int, CgiSession>::iterator sessIt);
	void killCgiSession(std::map<int, CgiSession>::iterator sessIt);
	void getHandle(Client &client, LocationConfig *loc, int fd);
	void postHandle(Client &client, LocationConfig *loc, int fd);
	void deleteHandle(Client &client, LocationConfig *loc, int fd);
	bool contentLengthCheck(Client &client, int fd);
	size_t effectiveMaxBodySize(Client &client);
	ChunkResult tryUnchunk(Client &client, size_t &consumedLength);

};

#endif
