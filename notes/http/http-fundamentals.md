# HTTP/1.1 Fundamentals

> **Project:** TinyHTTP  
> **Area:** Application-layer protocol  
> **Scope studied:** HTTP/1.1 request structure, request-line components, header fields, and message framing  
> **Implementation status:** Concepts studied; HTTP parsing not implemented yet

## 1. What HTTP does

HTTP (Hypertext Transfer Protocol) is an application-layer protocol used for communication between clients and servers. It defines the meaning and structure of requests and responses: methods, request targets, status codes, header fields, and message content.

HTTP does not replace TCP's transport responsibilities. In our planned HTTP/1.1 implementation, TCP provides an ordered byte stream between the client and TinyHTTP. HTTP defines how our application interprets those bytes.

A useful division of responsibility is:

- **TCP:** transports an ordered stream of bytes between endpoints.
- **HTTP/1.1:** defines how those bytes form request and response messages and what the message elements mean.
- **TinyHTTP:** reads bytes from its socket, recognizes HTTP message boundaries, parses the message, decides what to do, and writes an HTTP response.

HTTP/1.1 normally runs over TCP. HTTP/2 also uses TCP, while HTTP/3 uses QUIC over UDP. TinyHTTP is beginning with HTTP/1.1 over TCP because its wire format is comparatively human-readable.

## 2. HTTP/1.1 message structure

An HTTP/1.1 message has this general structure:

```text
start-line\r\n
header-field\r\n
header-field\r\n
...\r\n
\r\n
[message-body]
```

For a request:

1. The **request line** identifies the method, request target, and HTTP version.
2. The **header section** contains zero or more header fields.
3. An **empty line** marks the end of the header section.
4. An optional **message body** carries content.

Example:

```http
POST /users HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Content-Length: 24
Accept: application/json

{"name":"Karl","age":21}
```

The line breaks shown above represent HTTP/1.1 CRLF line endings (`\r\n`). The empty line is therefore the byte sequence `\r\n\r\n` after the final header field.

**Example correction:** The JSON body above is 24 bytes in UTF-8/ASCII, not 26. `Content-Length` counts octets (bytes), not an assumed number of displayed characters. Always calculate the byte length of the actual encoded body.

## 3. The request line

The request-line grammar is conceptually:

```text
method SP request-target SP HTTP-version CRLF
```

`SP` means one space character. For example:

```http
GET /products HTTP/1.1
```

| Component | Example | Meaning |
|---|---|---|
| Method | `GET` | The operation's semantics |
| Request target | `/products` | The target resource or authority |
| HTTP version | `HTTP/1.1` | The protocol version used by the message |

### 3.1 Methods and their semantics

A method is not merely an arbitrary function name. HTTP assigns semantics to standard methods.

| Method | General purpose | Safe? | Idempotent? |
|---|---|---:|---:|
| `GET` | Retrieve a representation | Yes | Yes |
| `HEAD` | Like GET, but no response content | Yes | Yes |
| `POST` | Submit content for processing | No | Not guaranteed |
| `PUT` | Create or replace the target resource's state | No | Yes |
| `DELETE` | Remove the association between the target and its current functionality | No | Yes |
| `PATCH` | Apply partial modifications | No | Not guaranteed |

- **Safe** means the client is not requesting a state change as the method's intended effect. It does not promise that the server performs literally zero logging or incidental state changes.
- **Idempotent** means multiple identical requests have the same intended effect as one request. This does not require every response to be identical.

These properties matter when clients, proxies, or applications decide whether retrying a request is appropriate. A method's semantics do not guarantee that every real-world implementation behaves correctly.

### 3.2 Request-target forms

HTTP/1.1 defines four request-target forms:

| Form | Example | Typical use |
|---|---|---|
| Origin-form | `/products?sort=price` | Ordinary request to an origin server |
| Absolute-form | `http://example.com/products` | Request to a forward proxy |
| Authority-form | `example.com:443` | `CONNECT` request |
| Asterisk-form | `*` | Server-wide `OPTIONS` request |

For an origin-form target such as:

```text
/products?category=books&sort=price
```

the path is `/products` and the query is `category=books&sort=price`.

For the first TinyHTTP HTTP implementation, origin-form is the intended initial scope. The other forms should be recognized as part of HTTP/1.1's grammar, but we do not need to implement them immediately.

### 3.3 Parsing is not routing

Parsing answers: "What method, target, version, and headers did the client send?"

Routing answers: "Given that parsed request, which application handler or resource should run?"

These are separate responsibilities. TinyHTTP should first turn incoming bytes into a structured request; routing can use that structure later.

## 4. Header fields

A header field has this general shape:

```text
field-name: field-value\r\n
```

Examples:

```http
Host: localhost:8080
Content-Type: application/json
Content-Length: 24
Accept: application/json
User-Agent: curl
```

Header fields carry metadata that helps the recipient interpret or process the message.

| Header | What it communicates |
|---|---|
| `Host` | The target host and optional port. Required in HTTP/1.1 requests. |
| `Content-Type` | The media type of the message content, such as `application/json` or `text/plain`. |
| `Content-Length` | The content length in octets when this field is used for message framing. |
| `Accept` | Media types the client is willing to receive in the response. |
| `User-Agent` | Information about the client software. |

### 4.1 Content-Type versus Accept

These are easy to confuse:

- `Content-Type` describes the content being sent in this message.
- `Accept` communicates which response media types the client can accept.

For example, a client can send JSON with `Content-Type: application/json` while also asking for a JSON response with `Accept: application/json`.

### 4.2 Header names are case-insensitive

These field names are equivalent in HTTP/1.1:

```http
Content-Type: application/json
content-type: application/json
CONTENT-TYPE: application/json
```

A parser must not treat differently cased spellings of a field name as unrelated fields. This rule applies to field names; individual field values have their own rules.

## 5. Line endings and the end of the header section

HTTP/1.1 uses CRLF (`\r\n`) to terminate the start line and each header field line.

The header section ends with an empty line:

```text
\r\n\r\n
```

This consists of:

1. CRLF terminating the last header field line.
2. Another CRLF representing the empty line.

Finding `\r\n\r\n` tells the parser where the header section ends. It does **not** automatically mean the entire HTTP message has ended: a message body can follow.

Because TCP is a byte stream, this four-byte delimiter can be split across multiple `recv()` calls. TinyHTTP must search the accumulated input, not assume the complete delimiter arrives in one read.

A single `recv()` can also return bytes beyond the header terminator. Those bytes may already be the beginning (or all) of the body. The implementation must preserve and account for them rather than discard them.

## 6. Request-body framing

For HTTP/1.1 requests, message framing determines where the request body ends. The presence and length of request content are signaled by `Content-Length` or `Transfer-Encoding`.

### 6.1 Content-Length

When `Transfer-Encoding` is not present, `Content-Length` can specify the expected body length as a decimal number of octets.

Example:

```http
POST /users HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Content-Length: 24

{"name":"Karl","age":21}
```

After finding the end of the headers, the server reads until it has collected 24 body bytes. If only 10 body bytes have arrived so far, it still needs 14 more.

`Content-Length` is a byte count, not a count of Unicode characters or a count of `std::string` code points. For UTF-8 text, a character outside ASCII can occupy multiple bytes.

If neither `Content-Length` nor `Transfer-Encoding` frames a request body, HTTP/1.1 request framing treats the request as having no body; a request is not delimited by waiting for the client to close the connection.

### 6.2 Chunked transfer coding

`Transfer-Encoding: chunked` allows a message body to be sent in chunks when its total size is not known in advance.

A simplified valid chunk sequence is:

```text
4\r\n
Wiki\r\n
5\r\n
pedia\r\n
0\r\n
\r\n
```

Chunk sizes are written in hexadecimal. Each chunk consists of a size line, the indicated number of data octets, and CRLF. A zero-sized final chunk marks the end of the chunked content (with optional trailer fields possible before the final empty line).

Chunked transfer coding is a message-framing mechanism. It is not the same thing as TCP packet boundaries or separate calls to `send()` and `recv()`.

TinyHTTP has not implemented chunked decoding. It is documented here because it is an important HTTP/1.1 framing mechanism.

### 6.3 Content-Length and Transfer-Encoding together

A sender must not send both `Content-Length` and `Transfer-Encoding` in the same message. The HTTP/1.1 specification says `Transfer-Encoding` takes precedence if both are received, and warns that inconsistent handling can enable request-smuggling or response-splitting attacks. A server may reject such a request; TinyHTTP's conservative initial policy should be to reject conflicting framing and close the connection after responding.

This is not merely a formatting concern. If two HTTP components disagree about where one message ends and the next begins, attacker-controlled bytes can be interpreted differently by each component.

## 7. How this changes our TCP receiving code

Our current TinyHTTP receive loop uses a newline (`\n`) as a temporary delimiter. That was a useful exercise for learning TCP accumulation, but it is not sufficient for HTTP/1.1.

The conceptual HTTP receiving/parsing sequence will be:

1. Read bytes from the connected socket and accumulate them.
2. Find `\r\n\r\n` to identify the end of the header section.
3. Parse the request line and header fields.
4. Determine request-body framing from `Content-Length` or `Transfer-Encoding`.
5. Read and preserve the required body bytes, if any.
6. Produce a structured request for later application logic.

The receiving mechanism and HTTP parser have separate responsibilities:

- **Receiving:** obtains bytes from the socket and accumulates them across reads.
- **Parsing:** interprets those bytes according to HTTP syntax and semantics.
- **Routing/application logic:** decides what the request should do.

## 8. Current TinyHTTP scope

### Studied

- HTTP as an application-layer protocol.
- HTTP/1.1 over TCP and the separation between transport and application semantics.
- Request-line components: method, request target, and HTTP version.
- Common methods and the meanings of safe and idempotent.
- The four request-target forms, with origin-form as our initial focus.
- Header fields, common examples, and case-insensitive field names.
- CRLF line endings and the `\r\n\r\n` header terminator.
- `Content-Length` and chunked transfer coding as body-framing mechanisms.
- Why conflicting framing information is a security concern.

### Not implemented yet

- HTTP request-line parsing.
- HTTP header parsing.
- HTTP message framing.
- Request-body accumulation.
- HTTP response formatting and status codes.
- Routing requests to application handlers.

Understanding a concept does not mean its implementation is complete. TinyHTTP is still at the TCP request-response stage.

## 9. References

- [RFC 9110 — HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110.html)
- [RFC 9112 — HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112.html), especially Sections 2 (message syntax), 5 (fields), 6 (message body), and 7.1 (chunked transfer coding).
- [MDN — Overview of HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP/Guides/Overview)
