*This project has been created as part of the 42 curriculum by rbarkhud, apatvaka, mabaghda.*

# webserv

## Description

`webserv` is a from-scratch implementation of a lightweight HTTP/1.1 server in C++98, built as part of the 42 core curriculum. The goal of the project is to understand how a web server actually works under the hood — parsing raw HTTP requests, managing client connections over non-blocking sockets, serving static files, running CGI scripts, and behaving in a way compatible with a real browser and tools like `curl`.

The server is entirely single-threaded and event-driven: all client sockets are multiplexed through a single `poll()` loop, so the server can handle many simultaneous connections without blocking on any one of them. Behavior (listening ports, virtual hosts, routes, allowed methods, redirections, CGI, uploads, error pages, etc.) is fully driven by an NGINX-style configuration file supplied on the command line.

Key features:
- Non-blocking I/O event loop built on `poll()`
- Custom NGINX-inspired configuration file parser (`server` / `location` blocks)
- Multiple virtual servers and multiple `listen` directives
- GET, POST and DELETE methods
- Static file serving, directory listing (`autoindex`), custom `index` files
- Custom error pages per status code
- Client body size limits (`client_max_body_size`)
- File uploads (`upload_store`)
- URL redirections (`return`)
- CGI execution (e.g. Python, PHP, or any executable) via `cgi_extension` / `cgi_path`
- Keep-alive connections and chunked/streamed request bodies

## Instructions

### Compilation

```sh
make        # builds the "webserv" binary
make clean  # removes object files
make fclean # removes object files and the binary
make re     # fclean + all
```

The project is built with `c++` targeting the `-std=c++98` standard, with `-Wall -Wextra -Werror` enabled.

### Running

```sh
./webserv <config_file>
```

Example configuration files can be found in [configs/](configs/) (covering allowed methods, redirects, error pages, multiple ports, location matching, and edge cases). A minimal example:

```nginx
server {
    listen 8080;
    server_name localhost;
    root ./www/site1;

    location / {
        allow_methods GET;
        index first.html;
    }

    location /cgi-bin {
        allow_methods GET POST;
        cgi_extension .py;
        cgi_path /usr/bin/python3;
    }

    location /upload {
        allow_methods POST;
        upload_store ./www/uploads;
    }
}
```

Once running, the server can be reached with a browser or `curl`, e.g.:

```sh
curl http://localhost:8080/
```

### Testing

- [tester](tester) — an integration test binary exercising static files, CGI, redirections, error handling and concurrent load, following the setup described by the tool itself (`./tester <scheme://host:port>`).

## Resources

- [RFC 7230](https://datatracker.ietf.org/doc/html/rfc7230) — HTTP/1.1: Message Syntax and Routing
- [RFC 7231](https://datatracker.ietf.org/doc/html/rfc7231) — HTTP/1.1: Semantics and Content
- [RFC 3875](https://datatracker.ietf.org/doc/html/rfc3875) — The Common Gateway Interface (CGI) Specification
- [NGINX documentation](https://nginx.org/en/docs/) — reference for the configuration file syntax and directive semantics
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) — sockets and non-blocking I/O fundamentals
- [`poll(2)` man page](https://man7.org/linux/man-pages/man2/poll.2.html) — I/O multiplexing used by the event loop
- [MDN HTTP docs](https://developer.mozilla.org/en-US/docs/Web/HTTP) — general HTTP reference (methods, status codes, headers)

### AI usage

AI assistance was used exclusively for:
- Drafting and structuring this README.
- Recommending learning materials and reference documentation on HTTP, sockets, and CGI to guide the team's own research (the resources listed above).