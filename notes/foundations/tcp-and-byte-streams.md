# TCP and Byte Streams

> **Project:** TinyHTTP_CPP
> **Area:** Networking fundamentals  
> **Status:** Introduced

## 1. What TCP provides

TCP (Transmission Control Protocol) provides a reliable, ordered stream of bytes between two endpoints of a connection.

For an application, this means bytes arrive in the order they were sent, or the connection reports a failure. TCP also handles details such as retransmission and ordering so the application does not have to implement those mechanisms itself.

The important word for TinyHTTP is **stream**.

## 2. TCP does not preserve message boundaries

TCP does not treat each call to `send()` as a separate message.

For example, a client might do this:

```cpp
send(socket_fd, "Hello", 5, 0);
send(socket_fd, "World", 5, 0);
```

The receiving program is not guaranteed to get `Hello` and `World` in two separate `recv()` calls. It might receive `HelloWorld` at once, or it might receive the bytes in several smaller pieces.

Likewise, one call to `send()` does not guarantee that the other side receives all those bytes in one call to `recv()`.

The sender's write boundaries and the receiver's read boundaries are independent.

## 3. Why this matters to an application

An application protocol usually needs to know where one message ends and the next begins. TCP does not provide that information for us.

The application must define and implement a **message-framing** strategy. Common approaches include:

- **Delimiter-based framing:** a marker such as a newline indicates the end of a message.
- **Fixed-length framing:** every message has a predetermined size.
- **Length-prefixed framing:** a header states how many bytes belong to the message.

These are application-level conventions built on top of TCP's byte stream.

## 4. How this applies to TinyHTTP

In our current receiving exercise, we use a newline (`\n`) as a simple delimiter. The server keeps accumulating bytes until it finds that delimiter.

This is a teaching exercise for understanding TCP stream handling. It is **not** HTTP message framing. HTTP has its own rules for identifying message boundaries, which we will study when we reach HTTP parsing.

## Key takeaways

- TCP provides a reliable, ordered byte stream.
- TCP does not preserve the boundaries between separate `send()` calls.
- A `recv()` call may return fewer bytes than the application expects; it is not a message-receive operation.
- Applications need their own framing rules to identify complete messages.

## Not covered yet

We have not yet studied TCP flow control and congestion control in depth, packet-level behaviour, sequence numbers, acknowledgements, or how HTTP frames request and response bodies. Those belong in later notes when the project reaches them.
