#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>

int main() {

    // 1. Create the listening socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        std::perror("socket");
        return 1;
    }

    std::cout << "Socket created, FD: " << socket_fd << '\n';

    // 2. Configure the server address
    sockaddr_in server_address{};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // 3. Bind the socket
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

    // 4. Listen for incoming connections
    if (listen(socket_fd, 10) == -1) {
        std::perror("listen");
        close(socket_fd);
        return 1;
    }

    std::cout << "Server is listening...\n";

    // 5. Accept a client connection
    int client_fd = accept(socket_fd, nullptr, nullptr);

    if (client_fd == -1) {
        std::perror("accept");
        close(socket_fd);
        return 1;
    }

    std::cout << "Client connected! FD: "
              << client_fd << '\n';

    // 6. Receive and accumulate the HTTP request head
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

        std::size_t remaining =
            max_request_size - request.size();

        std::size_t capacity =
            remaining < sizeof(chunk) ? remaining : sizeof(chunk);

        ssize_t more_bytes_received = recv(
            client_fd,
            chunk,
            capacity,
            0
        );

        if (more_bytes_received == -1) {
            std::perror("recv");
            close(client_fd);
            close(socket_fd);
            return 1;
        }
        else if (more_bytes_received == 0) {
            std::cout << "Client closed its sending side before "
                         "completing the request.\n";

            close(client_fd);
            close(socket_fd);
            return 0;
        }
        else {
            request.append(
                chunk,
                static_cast<std::size_t>(more_bytes_received)
            );

            std::cout << "recv() returned " << more_bytes_received
                      << " bytes. Total accumulated: "
                      << request.size() << " bytes\n";
        }
    }

    std::cout << "\nReceived HTTP request:\n"
              << request << '\n';

    // 7. Extract the HTTP request line
    std::size_t request_line_end = request.find("\r\n");

    if (request_line_end == std::string::npos) {
        std::cerr << "Invalid request: missing request line terminator.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    std::string request_line = request.substr(0, request_line_end);

    std::cout << "Request line: " << request_line << '\n';

    // 8. Find the first space
    std::size_t first_space = request_line.find(' ');

    if (first_space == std::string::npos || first_space == 0) {
        std::cerr << "Invalid request line: missing first separator.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    // 9. Find the second space
    std::size_t second_space = request_line.find(' ', first_space + 1);

    if (second_space == std::string::npos ||
        second_space == first_space + 1 ||
        second_space + 1 >= request_line.size()) {

        std::cerr << "Invalid request line: malformed separators.\n";
        close(client_fd);
        close(socket_fd);
        return 1;
    }

    // 10. Extract method, target and HTTP version
    std::string request_method = request_line.substr(
        0,
        first_space
    );

    std::string request_target = request_line.substr(
        first_space + 1,
        second_space - first_space - 1
    );

    std::string http_version = request_line.substr(second_space + 1);

    std::cout << "\nParsed request line:\n";
    std::cout << "Method: " << request_method << '\n';
    std::cout << "Target: " << request_target << '\n';
    std::cout << "Version: " << http_version << '\n';

    // 11. Determine the boundaries of the header section
    std::size_t header_start = request_line_end + 2;
    std::size_t headers_end = request.find("\r\n\r\n");

    // 12. Create a collection to store HTTP headers
    std::vector<std::pair<std::string, std::string>> headers;

    std::size_t current_header_start = header_start;

    // 13. Iterate through every HTTP header
    while (current_header_start < headers_end) {

        std::size_t header_line_end = request.find(
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

        // Extract the complete header line
        std::string header_line = request.substr(
            current_header_start,
            header_line_end - current_header_start
        );

        // Find the colon separating name and value
        std::size_t colon_position = header_line.find(':');

        if (colon_position == std::string::npos ||
            colon_position == 0) {

            std::cerr << "Invalid header.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        // Extract header name
        std::string header_name = header_line.substr(
            0,
            colon_position
        );

        // Find the beginning of the header value
        std::size_t value_start = header_line.find_first_not_of(
            " \t",
            colon_position + 1
        );

        std::string header_value;

        // Extract and trim the header value
        if (value_start != std::string::npos) {

            std::size_t value_end =
                header_line.find_last_not_of(" \t");

            header_value = header_line.substr(
                value_start,
                value_end - value_start + 1
            );
        }

        // Store the name and value together
        headers.emplace_back(header_name, header_value);

        // Move to the next header
        current_header_start = header_line_end + 2;
    }

    // 14. Display all parsed headers
    std::cout << "\nParsed HTTP headers:\n";

    for (const auto& header : headers) {
        std::cout << header.first << " = "
                  << header.second << '\n';
    }

    // 15. Send the response
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
            close(client_fd);
            close(socket_fd);
            return 1;
        }
        else if (bytes_sent == 0) {
            std::cerr << "Send made no progress.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }
        else {
            total_sent += static_cast<std::size_t>(bytes_sent);
        }
    }

    std::cout << "\nSent " << total_sent << " bytes\n";

    // 16. Close sockets
    std::cout << "Closing server...\n";

    close(client_fd);
    close(socket_fd);

    return 0;
}
