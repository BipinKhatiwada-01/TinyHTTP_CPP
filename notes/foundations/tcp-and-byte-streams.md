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

## 5. Experiment: observing multiple `recv()` calls

We tested whether one logical request could arrive at the server across more than one receiving operation.

The client was instructed to send the request in two writes, with a one-second pause between them:

```bash
{ printf 'Hello '; sleep 1; printf 'TinyHTTP!\n'; } | ncat 127.0.0.1 8080
```

The server logged:

```text
recv() returned 6 bytes. Total accumulated: 6 bytes
recv() returned 10 bytes. Total accumulated: 16 bytes
Received request: Hello TinyHTTP!
```

The first read supplied `Hello ` (6 bytes). The server appended those bytes to its `request` string, checked for the newline delimiter, and found that the request was not complete. It then called `recv()` again. The second read supplied `TinyHTTP!\n` (10 bytes), bringing the accumulated total to 16 bytes. This time the newline was present, so the loop ended.

This experiment demonstrated that our application can accumulate one logical message across multiple `recv()` calls. It also illustrated that a blocking `recv()` does not wait for its entire buffer to fill: it can return after some data becomes available.

### Important qualification

The experiment produced two reads, but this is not a guarantee that separate client writes become separate server reads. TCP does not preserve write boundaries. Even if a client sends all 16 bytes in one `send()` call, the server may receive them in one `recv()` call or in several. Conversely, data from multiple writes may be returned together by one `recv()`.

The delimiter—not the number of reads or writes—is what tells our current application-level exercise that the request is complete.

## Key takeaways

- TCP provides a reliable, ordered byte stream.
- TCP does not preserve the boundaries between separate `send()` calls.
- A `recv()` call may return fewer bytes than the application expects; it is not a message-receive operation.
- Applications need their own framing rules to identify complete messages.

## Not covered yet

We have not yet studied TCP flow control and congestion control in depth, packet-level behaviour, sequence numbers, acknowledgements, or how HTTP frames request and response bodies. Those belong in later notes when the project reaches them.
