# Data Transmission: `recv()`, `send()`, and Accumulation

> **Project:** TinyHTTP_CPP
> **Area:** Linux / POSIX sockets  
> **Language:** C++20  
> **Status:** Basic request accumulation, incomplete-request rejection, and a send-all loop implemented in the current working code; commit status not confirmed

## 1. Sending and receiving use the connected socket

After `accept()` succeeds, the server has a connected client socket descriptor. In our current server, we use that descriptor for both receiving and sending data:

- `recv()` reads bytes from the connection.
- `send()` writes bytes to the connection.

The original listening socket is not used for the actual data exchange with an accepted client.

## 2. Receiving data with `recv()`

The basic function signature is:

```cpp
ssize_t recv(int sockfd, void* buffer, size_t length, int flags);
```

Example:

```cpp
char buffer[1024];

ssize_t bytes_received = recv(
    client_fd,
    buffer,
    sizeof(buffer),
    0
);
```

The arguments are:

| Argument | Purpose |
|---|---|
| `sockfd` | The connected socket to read from. |
| `buffer` | Memory where received bytes are placed. |
| `length` | Maximum number of bytes to receive in this call. |
| `flags` | Optional behaviour flags; `0` requests the normal operation. |

The `length` argument is a maximum, not a promise that this many bytes will be returned. A successful call may return fewer bytes.

### 2.1 Interpreting the return value

| Return value | Meaning |
|---|---|
| `-1` | An error occurred; inspect `errno`. |
| `0` | The peer has orderly shut down its sending direction. |
| Greater than `0` | The number of bytes received. |

The result uses `ssize_t`, a signed type, because it needs to represent both positive byte counts and the error value `-1`.

A return value of zero is not necessarily proof that the entire TCP connection has vanished: TCP is full-duplex, and the peer may have shut down only its sending direction.

### 2.2 Buffer capacity and null termination

`recv()` writes bytes into the supplied buffer. It does not automatically append the C-string null terminator (`'\0'`).

If we specifically want to treat the bytes as a null-terminated C string, we must reserve space and add the terminator ourselves:

```cpp
char buffer[1024];
ssize_t bytes_received = recv(
    client_fd,
    buffer,
    sizeof(buffer) - 1,
    0
);

if (bytes_received > 0) {
    buffer[bytes_received] = '\0';
}
```

Here, `sizeof(buffer) - 1` leaves one byte for the terminator.

Null termination is not required when handling data with an explicit length, such as with `std::string::append()` or `std::cout.write()`.

## 3. Sending data with `send()`

The basic function signature is:

```cpp
ssize_t send(int sockfd, const void* buffer, size_t length, int flags);
```

Our basic response example is:

```cpp
const char response[] = "Hello from TinyHTTP!\n";

ssize_t bytes_sent = send(
    client_fd,
    response,
    sizeof(response) - 1,
    0
);
```

`sizeof(response) - 1` excludes the string literal's terminating null byte. We send the visible characters and newline, not the C-string terminator.

A successful `send()` call returns the number of bytes accepted for sending. It can be fewer than the requested length. A positive return value does not mean the remote application has already processed the data; it reports how many bytes the local socket accepted for sending. Therefore, sending code must be prepared to handle partial writes.

## 4. Handling partial sends with a send-all loop

A single `send()` call may accept fewer bytes than we requested. If we immediately close the connection after a short positive result, the remainder of the response will never be submitted by our application.

Our current implementation tracks how much of the response has been accepted by `send()` and continues from the correct position:

```cpp
const char response[] = "Hello from TinyHTTP!\n";

const std::size_t response_size = sizeof(response) - 1;
std::size_t total_sent = 0;

while (total_sent < response_size) {
    ssize_t bytes_sent = send(
        client_fd,
        response + total_sent,
        response_size - total_sent,
        0
    );

    if (bytes_sent == -1) {
        std::perror("send");
        // Handle the error and clean up.
    }
    else if (bytes_sent == 0) {
        // No progress was made; stop rather than looping forever.
    }
    else {
        total_sent += static_cast<std::size_t>(bytes_sent);
    }
}
```

The important pieces are:

- `total_sent` records how many response bytes have been accepted so far.
- `response + total_sent` points to the first byte not yet submitted.
- `response_size - total_sent` is the number of bytes still remaining.
- A positive `bytes_sent` advances the progress counter.
- A result of `-1` indicates an error. A result of `0` makes no progress, so our implementation treats it as a reason to stop rather than retry indefinitely.

For example, if the response is 21 bytes and one call returns 12, the next call starts 12 bytes into the response and requests the remaining 9 bytes. The loop finishes when `total_sent == response_size`.

This pattern is often called a *send-all loop*. It handles short positive writes, but it does not by itself make a blocking socket non-blocking or handle every possible operating-system condition. We have not yet studied interrupted calls, `EAGAIN`/`EWOULDBLOCK`, or `SIGPIPE` handling.

Our current test used a small response and confirmed that it was transmitted successfully. It did not force `send()` to return a partial positive count, so the retry path has been implemented but not directly demonstrated by that test.

## 5. TCP is a stream: accumulate data across reads

A single `recv()` call does not correspond to one complete application message. The sender and receiver may observe different chunk boundaries.

For example, a client may send:

```text
Hello from the client!\n
```

The server might receive it in several calls. If we need the complete application message, we must preserve earlier bytes and keep reading until our protocol's completion rule is satisfied.

We use two variables for the current exercise:

```cpp
std::string request;
char chunk[256];
```

- `chunk` is temporary storage for one `recv()` call.
- `request` accumulates the data across calls.

After a positive return from `recv()`, append exactly the number of bytes received:

```cpp
request.append(
    chunk,
    static_cast<std::size_t>(more_bytes_received)
);
```

The explicit length matters. The buffer may have capacity for 256 bytes even when the call received only a few bytes. We append the bytes actually received, not the entire buffer.

## 6. A first message-framing exercise: newline delimiter

For the current learning exercise, we treat a newline (`'\n'`) as the end of a simple text request:

```cpp
while (request.find('\n') == std::string::npos) {
    // Receive more data and append it to request.
}
```

`std::string::find()` returns the position of the character if it is found. If it is not found, it returns `std::string::npos`.

Therefore, the loop continues while the accumulated request does not contain a newline.

This is a deliberately simple application-level framing rule. It is **not** how we will determine the end of a real HTTP request. HTTP has its own message-framing rules, which we will cover when we reach HTTP parsing.

## 7. Limiting how much we accumulate

Our exercise sets a maximum request size:

```cpp
constexpr std::size_t max_request_size = 4096;
```

This is an application-defined safety limit for the exercise. It prevents the server from accumulating unlimited data if a client never sends the delimiter.

Before receiving more data, calculate how much space remains:

```cpp
std::size_t remaining = max_request_size - request.size();
```

Our temporary buffer has its own capacity:

```cpp
sizeof(chunk) // 256 bytes
```

We choose the smaller of the remaining request allowance and the buffer capacity:

```cpp
std::size_t capacity =
    remaining < sizeof(chunk) ? remaining : sizeof(chunk);
```

This ternary expression means:

```cpp
condition ? value_if_true : value_if_false
```

So we request no more than the temporary buffer can hold and no more than the remaining request allowance.

The resulting receive operation is:

```cpp
ssize_t more_bytes_received = recv(
    client_fd,
    chunk,
    capacity,
    0
);
```

We then handle its three possible outcomes:

```cpp
if (more_bytes_received == -1) {
    std::perror("recv");
    // Handle the error.
}
else if (more_bytes_received == 0) {
    // The peer shut down its sending direction.
    break;
}
else {
    request.append(
        chunk,
        static_cast<std::size_t>(more_bytes_received)
    );
}
```

The `break` matters: without it, a loop could repeatedly call `recv()` after the peer has shut down its sending direction and keep receiving zero without making progress. However, simply breaking out of the loop would allow execution to continue as though the request were complete.

In our current implementation, if `recv()` returns zero before the newline delimiter has arrived, we report that the client closed its sending direction before completing the request, close the client and listening sockets, and return from `main()`. We therefore do not proceed to send the normal response for an incomplete request.

The full exercise also checks whether the maximum size has been reached before the next receive operation. This is a learning implementation, not yet a complete or production-ready request reader.

## 8. Important limitations to revisit

The current exercise is intentionally small. It has some limitations that we should understand before extending it:

1. **Bytes after the delimiter:** one `recv()` can return data that includes the newline and additional bytes after it. We currently append all returned bytes, so the extra bytes remain in `request` even though the loop stops after finding the first newline.
2. **Interrupted system calls:** `recv()` can fail with `errno == EINTR`; our current example treats all `-1` results as fatal. We have not studied retry handling yet.
3. **Interrupted and non-blocking operations:** our current send-all loop handles short positive writes, errors, and a zero-progress result, but we have not studied retrying `EINTR` or handling `EAGAIN`/`EWOULDBLOCK` on non-blocking sockets. We have also not studied `SIGPIPE`.
4. **Blocking behaviour:** our sockets are currently blocking. A `recv()` can wait until data arrives, the peer shuts down its sending direction, or an error occurs.

These are not reasons to abandon the exercise. They are the next layers of understanding we can add when we study connection and request handling more deeply.

## Key takeaways

- Use the connected client socket for `recv()` and `send()`.
- `recv()` returns the number of bytes received, not a guarantee of a complete message.
- `recv()` does not null-terminate data.
- Use the actual returned byte count when appending data to a string.
- TCP does not preserve application message boundaries; the application needs a framing rule.
- Our newline delimiter and 4096-byte cap are exercise choices, not TCP or HTTP requirements.
- `send()` can accept fewer bytes than requested. A send-all loop tracks progress and continues from the first byte not yet submitted.

## Next topics (when we reach them)

- Handling interrupted sends/receives, non-blocking sockets, and `SIGPIPE`.
- Blocking socket behaviour and peer shutdown in more depth.
- Preserving bytes after a delimiter when reading successive messages.
- HTTP request structure and HTTP/1.1 message framing.
