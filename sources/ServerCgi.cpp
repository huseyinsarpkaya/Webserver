#include "Server.hpp"
#include "CgiOutput.hpp"
#include "RequestHandler.hpp"
#include "HttpResponseBuilder.hpp"
#include "HttpUtils.hpp"

#include <netinet/in.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <csignal>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <ctime>

#define CGI_STREAM_THRESHOLD 1048576
#define CGI_STREAM_BACKPRESSURE 1048576

static std::string numToString(long value)
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

static std::string toHttpEnvName(const std::string &headerName)
{
	std::string out = "HTTP_";
	for (size_t i = 0; i < headerName.size(); ++i)
	{
		char c = headerName[i];
		out += (c == '-') ? '_' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return out;
}

static void addEnv(std::vector<std::string> &env, const std::string &key, const std::string &value)
{
	env.push_back(key + "=" + value);
}

static void buildCgiEnv(const HttpRequest &req, ServerConfig *cfg, const std::string &scriptPath,
						size_t bodySize, std::vector<std::string> &env)
{
	const char *pathEnv = std::getenv("PATH");
	addEnv(env, "PATH", pathEnv ? pathEnv : "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin");

	addEnv(env, "GATEWAY_INTERFACE", "CGI/1.1");
	addEnv(env, "SERVER_PROTOCOL", req.getVersion().empty() ? "HTTP/1.1" : req.getVersion());
	addEnv(env, "REQUEST_METHOD", req.getMethod());
	addEnv(env, "SCRIPT_NAME", req.getPath());
	addEnv(env, "SCRIPT_FILENAME", scriptPath);
	addEnv(env, "PATH_INFO", req.getPath());
	addEnv(env, "PATH_TRANSLATED", scriptPath);
	addEnv(env, "QUERY_STRING", req.getQuery());
	addEnv(env, "REQUEST_URI", req.getQuery().empty() ? req.getPath() : req.getPath() + "?" + req.getQuery());
	addEnv(env, "REDIRECT_STATUS", "200");
	addEnv(env, "CONTENT_LENGTH", numToString(static_cast<long>(bodySize)));
	addEnv(env, "CONTENT_TYPE", req.getHeader("Content-Type"));

	if (cfg)
	{
		addEnv(env, "SERVER_NAME", cfg->hasServerName ? cfg->serverName : "localhost");
		if (!cfg->listens.empty())
			addEnv(env, "SERVER_PORT", numToString(ntohs(cfg->listens[0].port)));
	}

	const std::map<std::string, std::string> &headers = req.getHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
	{
		if (it->first == "Content-Type" || it->first == "Content-Length")
			continue;
		addEnv(env, toHttpEnvName(it->first), it->second);
	}
}

CgiOutput::CgiOutput() : statusCode(200), statusText("OK"), contentType("text/html")
{
}

static void parseCgiOutput(const std::string &raw, CgiOutput &out)
{
	size_t sep = raw.find("\r\n\r\n");
	size_t sepLen = 4;
	if (sep == std::string::npos)
	{
		sep = raw.find("\n\n");
		sepLen = 2;
	}

	std::string headerBlock = (sep == std::string::npos) ? "" : raw.substr(0, sep);
	out.body = (sep == std::string::npos) ? raw : raw.substr(sep + sepLen);

	std::istringstream headerStream(headerBlock);
	std::string line;
	while (std::getline(headerStream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		size_t valueStart = value.find_first_not_of(' ');
		value = (valueStart == std::string::npos) ? "" : value.substr(valueStart);

		if (name == "Status")
		{
			out.statusCode = std::atoi(value.c_str());
			size_t sp = value.find(' ');
			if (sp != std::string::npos)
				out.statusText = value.substr(sp + 1);
		}
		else if (name == "Content-Type")
			out.contentType = value;
		else if (name == "Location")
			out.location = value;
		else if (name == "Set-Cookie")
			out.setCookie = value;
	}
}

static int checkCgiScript(const std::string &scriptPath, bool useInterpreter)
{
	if (scriptPath.empty())
		return 400;

	if (useInterpreter)
		return 0;

	struct stat st;
	if (stat(scriptPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
		return 404;

	if (access(scriptPath.c_str(), R_OK) != 0)
		return 403;

	return 0;
}

static std::string findInterpreter(LocationConfig *loc, const std::string &path, bool &useInterpreter)
{
	size_t dot = path.find_last_of('.');
	std::string ext = (dot == std::string::npos) ? "" : path.substr(dot);
	std::string interpreter = loc->cgiExtension[ext];
	useInterpreter = !interpreter.empty();
	return interpreter;
}

static void splitScriptPath(const std::string &scriptPath, std::string &dir, std::string &file)
{
	size_t slash = scriptPath.find_last_of('/');
	dir  = (slash == std::string::npos) ? "." : scriptPath.substr(0, slash);
	file = (slash == std::string::npos) ? scriptPath : scriptPath.substr(slash + 1);
}

static bool openCgiPipes(int inPipe[2], int outPipe[2])
{
	if (pipe(inPipe) == -1)
		return false;
	if (pipe(outPipe) == -1)
	{
		close(inPipe[0]);
		close(inPipe[1]);
		return false;
	}
	fcntl(inPipe[1], F_SETPIPE_SZ, 1048576);
	fcntl(outPipe[0], F_SETPIPE_SZ, 1048576);
	return true;
}

static void execCgiChild(int inPipe[2], int outPipe[2],
						const std::string &interpreter, const std::string &scriptDir,
						const std::string &scriptFile, char **envp)
{
	dup2(inPipe[0], STDIN_FILENO);
	dup2(outPipe[1], STDOUT_FILENO);
	close(inPipe[0]); close(inPipe[1]);
	close(outPipe[0]); close(outPipe[1]);

	if (chdir(scriptDir.c_str()) != 0)
		_exit(1);

	char *argv[] = { const_cast<char *>("env"), const_cast<char *>(interpreter.c_str()),
					  const_cast<char *>(scriptFile.c_str()), NULL };
	execve("/usr/bin/env", argv, envp);
	_exit(1);
}

void Server::registerCgiSession(pid_t pid, int fd, int inPipe[2], int outPipe[2], std::string &body)
{
	close(inPipe[0]);
	close(outPipe[1]);
	fcntl(inPipe[1], F_SETFL, O_NONBLOCK);
	fcntl(outPipe[0], F_SETFL, O_NONBLOCK);

	CgiSession session;
	session.pid      = pid;
	session.clientFd = fd;
	session.inFd     = inPipe[1];
	session.outFd    = outPipe[0];
	session.body.swap(body);
	session.written  = 0;
	session.start    = time(NULL);
	session.lastActivity = session.start;
	session.headersDone = false;

	mCgiSessions[session.outFd] = session;
	mCgiPipeOwner[session.outFd] = session.outFd;

	struct pollfd outPfd;
	outPfd.fd = session.outFd;
	outPfd.events = POLLIN;
	outPfd.revents = 0;
	mPollFds.push_back(outPfd);

	if (!session.body.empty())
	{
		mCgiPipeOwner[session.inFd] = session.outFd;

		struct pollfd inPfd;
		inPfd.fd = session.inFd;
		inPfd.events = POLLOUT;
		inPfd.revents = 0;
		mPollFds.push_back(inPfd);
	}
	else
	{
		close(session.inFd);
		mCgiSessions[session.outFd].inFd = -1;
	}

	setClientPollEvents(fd, 0);
}

void Server::cgiHandle(Client &client, LocationConfig *loc, int fd)
{
	std::string scriptPath = RequestHandler::resolveFilePath(loc, client.request.getPath());

	bool useInterpreter = false;
	std::string interpreter = findInterpreter(loc, client.request.getPath(), useInterpreter);

	int scriptError = checkCgiScript(scriptPath, useInterpreter);
	if (scriptError != 0)
	{
		sendErrorAndCleanup(fd, scriptError);
		return;
	}

	std::string scriptDir;
	std::string scriptFile;
	splitScriptPath(scriptPath, scriptDir, scriptFile);

	int inPipe[2];
	int outPipe[2];
	if (!openCgiPipes(inPipe, outPipe))
	{
		sendErrorAndCleanup(fd, 500);
		return;
	}

	std::vector<std::string> envStorage;
	buildCgiEnv(client.request, client.matchedConfig, scriptFile, client.contentLength, envStorage);

	std::vector<char *> envp;
	for (size_t i = 0; i < envStorage.size(); ++i)
		envp.push_back(const_cast<char *>(envStorage[i].c_str()));
	envp.push_back(NULL);

	pid_t pid = fork();
	if (pid == -1)
	{
		close(inPipe[0]); close(inPipe[1]);
		close(outPipe[0]); close(outPipe[1]);
		sendErrorAndCleanup(fd, 500);
		return;
	}

	if (pid == 0)
	{
		execCgiChild(inPipe, outPipe, interpreter, scriptDir, scriptFile, &envp[0]);
		return;
	}

	std::string body;
	if (client.readBuffer.size() == client.contentLength)
		body.swap(client.readBuffer);
	else
		body = client.readBuffer.substr(0, client.contentLength);

	registerCgiSession(pid, fd, inPipe, outPipe, body);
}

bool Server::isCgiPipe(int pollFd) const
{
	return mCgiPipeOwner.find(pollFd) != mCgiPipeOwner.end();
}

void Server::removePipeFromPoll(int pipeFd)
{
	for (size_t i = 0; i < mPollFds.size(); ++i)
	{
		if (mPollFds[i].fd == pipeFd)
		{
			mPollFds.erase(mPollFds.begin() + i);
			return;
		}
	}
}

void Server::setClientPollEvents(int clientFd, short events)
{
	for (size_t i = 0; i < mPollFds.size(); ++i)
	{
		if (mPollFds[i].fd == clientFd)
		{
			mPollFds[i].events = events;
			return;
		}
	}
}

void Server::closeCgiFds(CgiSession &session)
{
	if (session.inFd != -1)
	{
		removePipeFromPoll(session.inFd);
		close(session.inFd);
		mCgiPipeOwner.erase(session.inFd);
	}
	removePipeFromPoll(session.outFd);
	close(session.outFd);
	mCgiPipeOwner.erase(session.outFd);
}

void Server::writeBodyToCgi(CgiSession &session, short revents)
{
	if (revents & POLLOUT)
	{
		size_t chunk = std::min(session.body.size() - session.written, static_cast<size_t>(1048576));
		ssize_t w = write(session.inFd, session.body.c_str() + session.written, chunk);
		if (w > 0)
		{
			session.written += static_cast<size_t>(w);
			session.lastActivity = time(NULL);
		}
	}
	if ((revents & (POLLHUP | POLLERR)) || session.written >= session.body.size())
	{
		removePipeFromPoll(session.inFd);
		close(session.inFd);
		mCgiPipeOwner.erase(session.inFd);
		session.inFd = -1;
	}
}

static void commitCgiStreaming(CgiSession &session, Client &client, int clientFd, Server &server)
{
	(void)clientFd;
	(void)server;
	CgiOutput cgiOut;
	parseCgiOutput(session.output, cgiOut);
	session.output.clear();
	session.headersDone = true;

	std::string cookieToSend = !cgiOut.setCookie.empty() ? cgiOut.setCookie : client.pendingSetCookie;
	client.writeBuffer += HttpResponseBuilder::buildStreamingHeader(
		cgiOut.statusCode, cgiOut.statusText, cgiOut.contentType, cookieToSend);
	client.writeBuffer += cgiOut.body;
	client.keepAlive = false;
	client.closeAfterWrite = true;
	client.cgiStreaming = true;
}

void Server::finishCgiSession(std::map<int, CgiSession>::iterator sessIt)
{
	CgiSession &session = sessIt->second;
	int clientFd = session.clientFd;
	bool wasStreaming = session.headersDone;
	int status = 0;
	waitpid(session.pid, &status, 0);

	std::map<int, Client>::iterator cIt = mClients.find(clientFd);
	bool keepAlive = (cIt != mClients.end()) && cIt->second.keepAlive;

	closeCgiFds(session);

	if (wasStreaming)
	{
		mCgiSessions.erase(sessIt);
		if (cIt != mClients.end())
		{
			cIt->second.cgiStreaming = false;
			setClientPollEvents(clientFd, POLLOUT);
		}
		return;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	{
		mCgiSessions.erase(sessIt);
		sendErrorAndCleanup(clientFd, 500);
	}
	else
	{
		CgiOutput cgiOut;
		parseCgiOutput(session.output, cgiOut);
		mCgiSessions.erase(sessIt);

		if (!cgiOut.setCookie.empty() && cIt != mClients.end())
			cIt->second.pendingSetCookie.clear();

		if (!cgiOut.location.empty() && cgiOut.statusCode == 200)
			sendResponseAndCleanup(clientFd, HttpUtils::insertSetCookie(
				HttpResponseBuilder::buildRedirect(302, cgiOut.location, keepAlive), cgiOut.setCookie));
		else
			sendResponseAndCleanup(clientFd, HttpUtils::insertSetCookie(
				HttpResponseBuilder::build(cgiOut.statusCode, cgiOut.statusText, cgiOut.body,
					keepAlive, cgiOut.contentType), cgiOut.setCookie));
	}
	setClientPollEvents(clientFd, POLLIN | POLLOUT);
}

void Server::readCgiOutput(std::map<int, CgiSession>::iterator sessIt)
{
	CgiSession &session = sessIt->second;
	int clientFd = session.clientFd;
	std::map<int, Client>::iterator cIt = mClients.find(clientFd);

	if (session.headersDone && cIt != mClients.end())
	{
		size_t pending = cIt->second.writeBuffer.size() - cIt->second.writeOffset;
		if (pending > CGI_STREAM_BACKPRESSURE)
		{
			session.lastActivity = time(NULL);
			return;
		}
	}

	char buf[1048576];
	ssize_t r = read(session.outFd, buf, sizeof(buf));
	if (r <= 0)
	{
		finishCgiSession(sessIt);
		return;
	}

	session.lastActivity = time(NULL);

	if (session.headersDone)
	{
		if (cIt != mClients.end())
		{
			cIt->second.writeBuffer.append(buf, static_cast<size_t>(r));
			setClientPollEvents(clientFd, POLLOUT);
		}
		return;
	}

	session.output.append(buf, static_cast<size_t>(r));
	if (session.output.size() > CGI_STREAM_THRESHOLD && cIt != mClients.end())
	{
		commitCgiStreaming(session, cIt->second, clientFd, *this);
		setClientPollEvents(clientFd, POLLOUT);
	}
}

bool Server::handleCgiPipeEvent(size_t i)
{
	int curFd = mPollFds[i].fd;
	short revents = mPollFds[i].revents;

	std::map<int, int>::iterator ownerIt = mCgiPipeOwner.find(curFd);
	if (ownerIt == mCgiPipeOwner.end())
		return false;

	std::map<int, CgiSession>::iterator sessIt = mCgiSessions.find(ownerIt->second);
	if (sessIt == mCgiSessions.end())
		return false;
	CgiSession &session = sessIt->second;

	if (time(NULL) - session.lastActivity > CGI_TIMEOUT_SECONDS)
	{
		killCgiSession(sessIt);
		return (i >= mPollFds.size() || mPollFds[i].fd != curFd);
	}

	if (curFd == session.inFd && (revents & (POLLOUT | POLLHUP | POLLERR)))
		writeBodyToCgi(session, revents);
	else if (curFd == session.outFd && (revents & (POLLIN | POLLHUP | POLLERR)))
		readCgiOutput(sessIt);

	return (i >= mPollFds.size() || mPollFds[i].fd != curFd);
}

void Server::killCgiSession(std::map<int, CgiSession>::iterator sessIt)
{
	CgiSession &session = sessIt->second;
	int clientFd = session.clientFd;
	bool wasStreaming = session.headersDone;

	kill(session.pid, SIGKILL);
	waitpid(session.pid, NULL, 0);
	closeCgiFds(session);
	mCgiSessions.erase(sessIt);

	std::map<int, Client>::iterator cIt = mClients.find(clientFd);
	if (cIt == mClients.end())
		return;

	if (wasStreaming)
	{
		cIt->second.cgiStreaming = false;
		setClientPollEvents(clientFd, POLLOUT);
	}
	else
	{
		sendErrorAndCleanup(clientFd, 500);
		setClientPollEvents(clientFd, POLLIN | POLLOUT);
	}
}

void Server::checkCgiTimeouts()
{
	time_t now = time(NULL);
	std::vector<int> timedOut;

	for (std::map<int, CgiSession>::iterator it = mCgiSessions.begin(); it != mCgiSessions.end(); ++it)
	{
		if (now - it->second.lastActivity > CGI_TIMEOUT_SECONDS)
			timedOut.push_back(it->first);
	}

	for (size_t i = 0; i < timedOut.size(); ++i)
	{
		std::map<int, CgiSession>::iterator sessIt = mCgiSessions.find(timedOut[i]);
		if (sessIt != mCgiSessions.end())
			killCgiSession(sessIt);
	}
}

void Server::abortCgi(int clientFd)
{
	std::map<int, CgiSession>::iterator sessIt = mCgiSessions.begin();
	while (sessIt != mCgiSessions.end() && sessIt->second.clientFd != clientFd)
		++sessIt;
	if (sessIt == mCgiSessions.end())
		return;

	CgiSession &session = sessIt->second;
	kill(session.pid, SIGKILL);
	waitpid(session.pid, NULL, 0);
	closeCgiFds(session);
	mCgiSessions.erase(sessIt);
}
