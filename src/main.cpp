#include <cstddef>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
    // 1. Create the listening socket.
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        std::perror("socket");
        return 1;
    }

    std::cout << "Socket created, FD: " << socket_fd << '\n';

    // 2. Configure the server address.
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 3. Bind the socket.
    int bind_result = bind(
        socket_fd,
        reinterpret_cast<const sockaddr*>(&server_address),
        sizeof(server_address)
    );

    if (bind_result == -1) {
        std::perror("bind");
        close(socket_fd);
        return 1;
    }

    // 4. Listen for incoming connections.
    if (listen(socket_fd, 10) == -1) {
        std::perror("listen");
        close(socket_fd);
        return 1;
    }

    std::cout << "Server is listening...\n";

    // 5. Accept one client connection.
    int client_fd = accept(socket_fd, nullptr, nullptr);

    if (client_fd == -1) {
        std::perror("accept");
        close(socket_fd);
        return 1;
    }

    std::cout << "Client connected! FD: " << client_fd << '\n';

    // 6. Receive and accumulate the HTTP request head.
    std::string request;
    char chunk[256];

    constexpr std::size_t max_request_size = 4096;

    while (request.find("\r\n\r\n") == std::string::npos) {
        if (request.size() >= max_request_size) {
            std::cerr << "Request too large.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        const std::size_t remaining = max_request_size - request.size();
        const std::size_t capacity =
            remaining < sizeof(chunk) ? remaining : sizeof(chunk);

        const ssize_t bytes_received = recv(
            client_fd,
            chunk,
            capacity,
            0
        );

        if (bytes_received == -1) {
            std::perror("recv");
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        if (bytes_received == 0) {
            std::cout << "Client closed its sending side before "
                         "completing the request.\n";
            close(client_fd);
            close(socket_fd);
            return 0;
        }

        request.append(
            chunk,
            static_cast<std::size_t>(bytes_received)
        );

        std::cout << "recv() returned " << bytes_received
                  << " bytes. Total accumulated: "
                  << request.size() << " bytes\n";
    }

    std::cout << "\nReceived HTTP request:\n" << request << '\n';

    // 7. Extract the HTTP request line.
    const std::size_t request_line_end = request.find("\r\n");

    if (request_line_end == std::string::npos) {
        std::cerr << "Invalid request: missing request line terminator.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    const std::string request_line = request.substr(0, request_line_end);
    std::cout << "Request line: " << request_line << '\n';

    // 8. Find the first and second spaces in the request line.
    const std::size_t first_space = request_line.find(' ');

    if (first_space == std::string::npos || first_space == 0) {
        std::cerr << "Invalid request line: missing first separator.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    const std::size_t second_space = request_line.find(' ', first_space + 1);

    if (second_space == std::string::npos ||
        second_space == first_space + 1 ||
        second_space + 1 >= request_line.size()) {
        std::cerr << "Invalid request line: malformed separators.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    // 9. Extract the method, request target, and HTTP version.
    const std::string request_method = request_line.substr(0, first_space);

    const std::string request_target = request_line.substr(
        first_space + 1,
        second_space - first_space - 1
    );

    const std::string http_version = request_line.substr(second_space + 1);

    std::cout << "\nParsed request line:\n"
              << "Method: " << request_method << '\n'
              << "Target: " << request_target << '\n'
              << "Version: " << http_version << '\n';

    // 10. Determine the boundaries of the header section.
    const std::size_t header_start = request_line_end + 2;
    const std::size_t headers_end = request.find("\r\n\r\n");

    // 11. Parse and store the HTTP headers.
    std::vector<std::pair<std::string, std::string>> headers;
    std::size_t current_header_start = header_start;

    while (current_header_start < headers_end) {
        const std::size_t header_line_end = request.find(
            "\r\n",
            current_header_start
        );

        if (header_line_end == std::string::npos ||
            header_line_end > headers_end) {
            std::cerr << "Invalid header line.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        // Extract the complete header line.
        const std::string header_line = request.substr(
            current_header_start,
            header_line_end - current_header_start
        );

        // Find the colon separating the header name and value.
        const std::size_t colon_position = header_line.find(':');

        if (colon_position == std::string::npos || colon_position == 0) {
            std::cerr << "Invalid header.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        // Extract the header name.
        const std::string header_name = header_line.substr(0, colon_position);

        // Find the first non-space, non-tab character in the header value.
        const std::size_t value_start = header_line.find_first_not_of(
            " \t",
            colon_position + 1
        );

        std::string header_value;

        // Extract the header value, excluding optional surrounding whitespace.
        if (value_start != std::string::npos) {
            const std::size_t value_end = header_line.find_last_not_of(" \t");

            header_value = header_line.substr(
                value_start,
                value_end - value_start + 1
            );
        }

        // Store the header name and value as one pair.
        headers.emplace_back(header_name, header_value);

        // Advance past this line's CRLF to the next header.
        current_header_start = header_line_end + 2;
    }

    // 12. Display all parsed headers.
    std::cout << "\nParsed HTTP headers:\n";

    for (const auto& header : headers) {
        std::cout << header.first << " = " << header.second << '\n';
    }

    // 13. Find the Content-Length header.
    std::string content_length_value;
    bool content_length_found = false;

    for (const auto& header : headers) {
        if (header.first == "Content-Length") {
            content_length_value = header.second;
            content_length_found = true;
            break;
        }
    }

    if (!content_length_found) {
        std::cerr << "Content-Length header not found.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    // 14. Convert Content-Length from a string to an integer.
    int integer_content_length = 0;

    try {
        integer_content_length = std::stoi(content_length_value);
    }
    catch (const std::invalid_argument& error) {
        std::cerr << "Invalid Content-Length: " << error.what() << '\n';
        close(client_fd);
        close(socket_fd);
        return 1;
    }
    catch (const std::out_of_range& error) {
        std::cerr << "Content-Length is out of range: " << error.what() << '\n';
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    std::cout << "Content-Length: " << integer_content_length << '\n';

    // 15. Extract the body bytes that have already been received.
    const std::size_t body_start = headers_end + 4;
    const std::size_t received_body_bytes = request.size() - body_start;
    const std::string body = request.substr(body_start, received_body_bytes);

    std::cout << "Body: " << body << '\n';

    // 16. Send the response, accounting for partial sends.
    const char response[] = "Hello from TinyHTTP!\n";
    const std::size_t response_size = sizeof(response) - 1;
    std::size_t total_sent = 0;

    while (total_sent < response_size) {
        const ssize_t bytes_sent = send(
            client_fd,
            response + total_sent,
            response_size - total_sent,
            0
        );

        if (bytes_sent == -1) {
            std::perror("send");
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        if (bytes_sent == 0) {
            std::cerr << "Send made no progress.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    std::cout << "\nSent " << total_sent << " bytes\n";

    // 17. Close the client and listening sockets.
    std::cout << "Closing server...\n";
    close(client_fd);
    close(socket_fd);

    return 0;
}
