# TinyHTTP_CPP — Project Log

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

## Current learning area

### 3. Receiving a complete application-level message

**Status:** In progress — learning exercise

We are extending the basic receive operation to accumulate data across multiple `recv()` calls.

Current exercise:

- Store each individual read in a temporary `char` buffer.
- Append only the bytes actually received to a `std::string`.
- Use a newline as a temporary message delimiter.
- Enforce a maximum accumulated request size of 4096 bytes.
- Handle the basic `recv()` outcomes: error, peer sending-direction shutdown, or a positive byte count.

This is not yet a real HTTP request parser. We still need to study incomplete input, extra bytes after a delimiter, interrupted calls, blocking behaviour, and robust partial writes.

## Experiments and observations

- Ports below `1024` generally require elevated privileges or the appropriate Linux capability; development testing uses port `8080`.
- The `backlog` argument to `listen()` is not a direct maximum number of simultaneous clients.
- `accept()` returns a new connected descriptor and leaves the listening descriptor available.
- TCP is a byte stream and does not preserve `send()` call boundaries.

## Working principle

A milestone should represent a coherent piece of understanding and functionality. We prefer meaningful checkpoints over a long list of tiny tasks. A concept being documented does not by itself mean its implementation is finished or committed.
