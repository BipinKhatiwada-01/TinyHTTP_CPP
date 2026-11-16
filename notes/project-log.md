# TinyHTTP — Project Log

This log records meaningful project milestones and experiments. It is not intended to be a line-by-line diary.

## Completed milestones

### 1. Basic TCP server and connection acceptance

**Status:** Completed and committed

- Created an IPv4 TCP socket with `socket()`.
- Configured a local address and port using `sockaddr_in`.
- Bound the socket with `bind()`.
- Put the socket into listening mode with `listen()`.
- Accepted a client connection with `accept()`.
- Confirmed that the listening socket and connected client socket have separate file descriptors.

### 2. Basic TCP data exchange

**Status:** Completed and committed

- Received client data with `recv()`.
- Sent a response with `send()`.
- Tested the exchange locally using `ncat`.
- Observed that the returned byte count must be used when interpreting received data.
- Explored connection closing and the fact that TCP shutdown is not necessarily instantaneous.

## Current implementation checkpoint

### 3. Basic request accumulation and response handling

**Status:** Implemented and tested locally; commit status not confirmed

We are extending the basic receive operation to accumulate data across multiple `recv()` calls.

Current exercise:

- Store each individual read in a temporary `char` buffer.
- Append only the bytes actually received to a `std::string`.
- Use a newline as a temporary message delimiter.
- Enforce a maximum accumulated request size of 4096 bytes.
- Handle the basic `recv()` outcomes: error, peer sending-direction shutdown, or a positive byte count.
- Reject a request if the client closes its sending direction before the newline delimiter arrives, rather than sending the normal response for incomplete input.
- Use a send-all loop that tracks the number of response bytes accepted by `send()` and resumes from the first unsent byte when a call returns a short positive count.

The current implementation is still an educational newline-framed TCP server, not an HTTP request parser. We still need to study extra bytes after a delimiter, interrupted calls, blocking behaviour in more depth, non-blocking I/O, and connection management.

## Experiments and observations

- Ports below `1024` generally require elevated privileges or the appropriate Linux capability; development testing uses port `8080`.
- The `backlog` argument to `listen()` is not a direct maximum number of simultaneous clients.
- `accept()` returns a new connected descriptor and leaves the listening descriptor available.
- TCP is a byte stream and does not preserve `send()` call boundaries.


### Experiment: data arriving across multiple reads

**Status:** Completed — observation recorded

- Ran the client command `{ printf 'Hello '; sleep 1; printf 'TinyHTTP!\n'; } | ncat 127.0.0.1 8080` to send a line in two writes separated by a delay.
- Observed the server receive 6 bytes and then 10 bytes in separate `recv()` calls, accumulating 16 bytes before finding the newline delimiter.
- Confirmed that the receiving loop can assemble one logical request across multiple reads.
- Important caveat: the observed two-read result is not guaranteed. TCP does not preserve the boundaries between client writes and server reads.


### Experiment: incomplete request handling

**Status:** Completed — behaviour observed

- Sent `Hello TinyHTTP` without a newline, then shut down the client's sending direction using `ncat -N`.
- The server received the 14 bytes, then `recv()` returned `0`.
- Updated the server to treat this as an incomplete request and exit before reaching the normal response-sending section.
- Confirmed the server did not send the normal response for this incomplete input.

### Implementation: send-all loop

**Status:** Implemented and tested locally; partial-write retry path not forced by the test

- Replaced the single `send()` call with a loop tracking `total_sent`.
- Each iteration starts at `response + total_sent` and requests `response_size - total_sent` bytes.
- A positive result advances the counter; `-1` is treated as an error; `0` is treated as no progress so the loop cannot spin forever.
- Tested the normal request-response exchange successfully. The small response was transmitted successfully, but the test did not force a partial positive return from `send()`.

## Learning checkpoint: HTTP/1.1 fundamentals

**Status:** Studied and documented; not implemented

Created [HTTP/1.1 Fundamentals](http/http-fundamentals.md) to consolidate the first HTTP lessons. Topics covered:

- HTTP as an application-layer protocol layered over TCP for HTTP/1.1.
- Request-line structure: method, request target, and HTTP version.
- Common method semantics, including safe and idempotent properties.
- Request-target forms, with origin-form selected as the initial TinyHTTP scope.
- Header fields, common headers (`Host`, `Content-Type`, `Content-Length`, `Accept`, and `User-Agent`), and case-insensitive field names.
- CRLF line endings and `\r\n\r\n` as the end-of-header-section delimiter.
- Request body framing with `Content-Length` and chunked transfer coding.
- The risks of conflicting `Content-Length` and `Transfer-Encoding` framing.
- Separation between socket receiving, HTTP parsing, and application routing.

The next implementation step is to evolve the current newline-based TCP receiver to collect an HTTP/1.1 request head. Body framing will be addressed as a separate step after request-line and header parsing are understood.

**Example correction:** The body `{"name":"Karl","age":21}` is 24 bytes, not 26. The HTTP note uses the corrected `Content-Length: 24`.

## Working principle

A milestone should represent a coherent piece of understanding and functionality. We prefer meaningful checkpoints over a long list of tiny tasks. A concept being documented does not by itself mean its implementation is finished or committed.
