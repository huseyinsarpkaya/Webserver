*This project has been created as part of the 42 curriculum by tuzan, husarpka, merilhan.*

# webserv

## Description

**webserv** is our own HTTP/1.1 web server. We wrote it from scratch in C++98, for the 42 school project called "webserv". We do not use any outside library for networking, HTTP, or CGI. Everything is built by us:

- our own event loop, using one `poll()` call for all sockets (clients, listening sockets, and CGI pipes)
- our own HTTP request and response parser
- our own configuration file format (it looks like NGINX)
- our own CGI/1.1 gateway (with `fork`, `pipe`, and `execve`)

The goal of this project is to understand HTTP well. We want our server to work correctly with real tools like a browser, `curl`, and CGI scripts (PHP, Python, and more). Our server supports keep-alive, chunked transfer-encoding, virtual hosts, static files, file upload and delete, directory listing, custom error pages, and CGI. All of this runs on one non-blocking event loop.

### Project layout

- **sources/ & includes/** — `Utils` (helpers), `Http` (parsing), `Config` (config file parsing), `Cgi` (CGI execution), `Core` (`Server`, `Client`, `Router`, the `poll()` loop)
- **configs/** — example and test configuration files
- **www/** — sample website served by the default configuration


## Instructions

### Build

```sh
make            # builds ./webserv
make re         # clean rebuild
make clean      # removes object files (objs/)
make fclean     # removes object files AND the binary
```

We compile with `c++ -Wall -Wextra -Werror -std=c++98`. Object files go into `objs/`, with the same folder structure as `sources/`. This keeps `sources/` and `includes/` clean.

### Run

```sh
./webserv [configuration file]
```

If you don't give a configuration file, the server uses `configs/default.conf`. Open that file to see many `curl` examples for every required feature. `configs/tester.conf` is a second config we used for extra manual testing.

## Main features

- **HTTP/1.1 connection handling** — keep-alive (many requests on one TCP connection), pipelining (several requests sent back-to-back without waiting), and chunked transfer-encoding, decoded incrementally so a large upload does not slow down the server.
- **HTTP methods** — GET, POST, and DELETE are supported, and each `location` block can allow or block specific methods (a blocked method returns 405).
- **Static content** — serves files from disk, uses a configured index file for directories, and can show a generated directory listing (autoindex) when no index file is found.
- **File upload and delete** — POST can save a request body to a configured upload folder, and DELETE can remove a file from it.
- **Redirection** — a `location` can redirect to another internal path or to a full external URL, with any of the common redirect status codes (301, 302, 303, 307, 308).
- **CGI/1.1** — scripts are matched by file extension and run with `fork` + `execve`, get the full CGI environment variable set (`REQUEST_METHOD`, `PATH_INFO`, `QUERY_STRING`, ...), and are killed if they run past a timeout.
- **Networking and virtual hosts** — the server can listen on several `ip:port` pairs at once, merges wildcard and specific-IP listeners like NGINX does, and picks a virtual host using `server_name` and the `Host` header.
- **Limits and error pages** — `client_max_body_size` is enforced per location (a request over the limit gets 413), and each status code can have its own custom error page, with a default page as a fallback.
- **Security hardening** — protection against path traversal, null-byte injection, unsafe percent-encoding, HTTP response-splitting, and XSS in the autoindex output, plus a timeout for slow (Slowloris-style) requests and strict `Content-Length`/`Transfer-Encoding` checks to avoid request smuggling.

## Resources

- [RFC 7230](https://www.rfc-editor.org/rfc/rfc7230) — HTTP/1.1: Message Syntax and Routing, our main reference for the HTTP/1.1 protocol
- [RFC 7231](https://www.rfc-editor.org/rfc/rfc7231) — HTTP/1.1: Semantics and Content, our main reference for methods, status codes, and headers
- [W3C HTTP/1.1 (RFC 2616, historic)](https://www.w3.org/Protocols/rfc2616/rfc2616.html) — the original HTTP/1.1 spec, useful side-by-side with RFC 7230/7231
- [RFC 6585](https://www.rfc-editor.org/rfc/rfc6585) — Additional HTTP Status Codes
- [RFC 3875](https://www.rfc-editor.org/rfc/rfc3875) — The Common Gateway Interface (CGI) Version 1.1, our main reference for CGI/1.1
- [NCSA CGI Specification](https://hoohoo.ncsa.illinois.edu/cgi/) — the original CGI 1.1 documentation, useful alongside RFC 3875
- [RFC 1945](https://www.rfc-editor.org/rfc/rfc1945) — HTTP/1.0
- [MDN Web Docs — HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP) — headers, status codes, and message format explained in plain language
- [HTTP Made Really Easy](https://www.jmarshall.com/easy/http/) — a short, clear walkthrough of the HTTP/1.1 protocol we used early on
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — sockets, `poll()`, non-blocking I/O
- [NGINX documentation](https://nginx.org/en/docs/) — we compared our config syntax and behaviour with NGINX
- `man` pages for every syscall we used (`poll`, `fcntl`, `execve`, `waitpid`, `getaddrinfo`, ...)

### About AI help

We used an AI assistant during this project, mainly to write some of our manual test scripts. Every change was reviewed, compiled, and tested by hand against real `curl`/browser/CGI behaviour before we kept it.