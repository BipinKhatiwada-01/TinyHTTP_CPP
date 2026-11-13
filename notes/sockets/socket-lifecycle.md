# Socket Lifecycle

> **Project:** TinyHTTP_CPP
> **Area:** Linux / POSIX sockets  
> **Language:** C++20  
> **Status:** Implemented and tested at the basic level

## 1. What is a socket?

A socket is an operating-system communication endpoint. On Linux, the socket APIs used by TinyHTTP return file descriptors that our program passes to later system calls.

A file descriptor is a small integer identifying an open resource in the process. The first commonly available descriptors are:

| Descriptor | Conventional use |
|---:|---|
| `0` | Standard input |
| `1` | Standard output |
| `2` | Standard error |

A newly created socket often receives descriptor `3` or a higher number, depending on which descriptors are already open.

## 2. The server-side lifecycle

Our basic IPv4 TCP server follows this sequence:

```mermaid
flowchart TD
    A[ socket() ] --> B[ bind() ]
    B --> C[ listen() ]
    C --> D[ accept() ]
    D --> E[ Communicate using client socket ]
    E --> F[ close(client_fd) ]
    F --> G[ Continue accepting or close listening socket ]
```

The listening socket and the connected client socket have different jobs.

- The **listening socket** waits for incoming connections.
- The **connected socket** is returned by `accept()` and is used to exchange data with that client.

## 3. Creating a socket — `socket()`

```cpp
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
```

The arguments specify:

| Argument | Meaning |
|---|---|
| `AF_INET` | Use IPv4 addresses. |
| `SOCK_STREAM` | Request a stream-oriented socket; with this address family and protocol selection, this is TCP. |
| `0` | Let the system select the appropriate default protocol for the chosen family and socket type. |

On success, `socket()` returns a file descriptor. On failure, it returns `-1` and sets `errno`.

## 4. Preparing the server address

Our server uses `sockaddr_in` to describe an IPv4 address and port:

```cpp
sockaddr_in server_address{};
server_address.sin_family = AF_INET;
server_address.sin_port = htons(8080);
server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
```

Important details:

- `sin_family` identifies the address family.
- `sin_port` stores the port in network byte order. `htons()` converts a 16-bit value from host byte order to network byte order.
- `sin_addr.s_addr` stores the IPv4 address in network byte order. `htonl()` converts a 32-bit value.
- `INADDR_LOOPBACK` selects the loopback address (`127.0.0.1`), so this configuration is intended for local testing.

When passing `sockaddr_in` to APIs that accept the general `sockaddr` type, we use a cast such as:

```cpp
reinterpret_cast<const sockaddr*>(&server_address)
```

## 5. Assigning the address — `bind()`

```cpp
bind(
    socket_fd,
    reinterpret_cast<const sockaddr*>(&server_address),
    sizeof(server_address)
);
```

`bind()` assigns a local address to the socket. For our server, that means selecting the local IP address and port on which it will listen.

It returns `0` on success and `-1` on failure.

A port may already be in use, or the process may lack permission to bind to a privileged port. On typical Linux configurations, ports below `1024` require elevated privileges or an appropriate capability. For development, we use a port such as `8080` rather than running the server as root.

## 6. Marking the socket as listening — `listen()`

```cpp
listen(socket_fd, 10);
```

`listen()` marks a stream socket as passive: it is now prepared to accept incoming connection requests.

The second argument is the backlog, which influences the queue of connections waiting to be accepted. It is **not** a direct setting for the total number of clients the server can handle simultaneously.

## 7. Accepting a connection — `accept()`

```cpp
int client_fd = accept(socket_fd, nullptr, nullptr);
```

`accept()` waits for a connection and, when one is available, returns a **new file descriptor** for the connected socket.

This distinction is fundamental:

- `socket_fd` remains the listening socket.
- `client_fd` represents the accepted client connection.

We use `client_fd` for `recv()` and `send()` with that client. The listening socket remains available to accept other connections when our program's control flow returns to `accept()`.

On a blocking socket, `accept()` normally waits when no connection is available. It returns `-1` if an error occurs.

## 8. Closing sockets — `close()`

```cpp
close(client_fd);
close(socket_fd);
```

`close()` releases the process's reference to the file descriptor. We close the connected client socket when we are finished with that connection, and close the listening socket when the server is shutting down.

TCP connection shutdown is more nuanced than simply deleting a connection instantly. TCP is full-duplex, so each direction can be shut down independently, and connection states can persist during teardown. We have encountered this while experimenting with `ncat`; we will study it in more detail if connection management requires it.

## 9. Basic error-checking pattern

Most of the system calls in this lifecycle report failure with `-1`:

```cpp
if (socket_fd == -1) {
    std::perror("socket");
    return 1;
}
```

`perror()` prints a description based on the current `errno` value. Include the appropriate header when using it (commonly `<cstdio>`).

Always check the return value before using a descriptor returned by a system call.

## Key takeaways

- `socket()` creates the socket endpoint.
- `bind()` assigns its local address and port.
- `listen()` makes the socket passive and ready for incoming connections.
- `accept()` returns a separate connected socket descriptor.
- Use the connected descriptor for client communication; keep the listening descriptor for accepting connections.
- Close descriptors when they are no longer needed.

## Not covered yet

This note describes our basic blocking, single-client learning server. It does not yet cover non-blocking sockets, `poll()`/`epoll()`, concurrent clients, graceful server shutdown, address resolution, IPv6, or robust handling of every possible system-call error.
