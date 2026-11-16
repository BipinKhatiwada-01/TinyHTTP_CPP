#include <cstddef>
#include <iostream>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdio>
#include <string>

int main(){
    
        int socket_fd = socket(AF_INET, SOCK_STREAM , 0);   

        if (socket_fd == -1) {
            std::perror("socket");
            return 1;
        }

        std::cout << "Socket created, FD: " << socket_fd << '\n';

        sockaddr_in server_address{};

        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(8080);
        server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

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

        if (listen(socket_fd, 10) == -1) {
            std::perror("listen");
            close(socket_fd);
            return 1;
        }

        std::cout << "Server is listening...\n";

        int client_fd = accept(socket_fd, nullptr, nullptr);

        if (client_fd == -1) {
            std::perror("accept");
            close(socket_fd);
            return 1;
        }

        std::cout << "Client connected! FD: "
                << client_fd << '\n';

        std::string request;
        char chunk[256];

        constexpr std::size_t max_request_size = 4096;

        while (request.find("\r\n\r\n") == std::string::npos) {


            if (request.size() == max_request_size) {
                std::cerr << "Request too large.\n";
                close(client_fd);
                close(socket_fd);
                return 1;
            }

            std::size_t remaining = max_request_size - request.size();

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
        
        std::size_t request_line_end = request.find("\r\n");

        if (request_line_end == std::string::npos) {
            std::cerr << "Invalid request: missing request line terminator.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        std::string request_line = request.substr(0, request_line_end);

        std::cout << "Request line: " << request_line << '\n';

        // Find the first space
        std::size_t first_space = request_line.find(' ');

        if (first_space == std::string::npos) {
            std::cerr << "Invalid request line: missing first separator.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        // Find the second space
        std::size_t second_space = request_line.find(' ', first_space + 1);

        if (second_space == std::string::npos) {
            std::cerr << "Invalid request line: missing second separator.\n";
            close(client_fd);
            close(socket_fd);
            return 1;
        }

        // Extract the three components
        std::string request_method = request_line.substr(0, first_space);

        std::string request_target = request_line.substr(
            first_space + 1,
            second_space - first_space - 1
        );

        std::string http_version = request_line.substr(second_space + 1);

        // Display the results
        std::cout << "Request Method: " << request_method << '\n';
        std::cout << "Request Target: " << request_target << '\n';
        std::cout << "HTTP Version: " << http_version << '\n';

        std::cout << "Received request: " << request << '\n';

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

        std::cout << "Sent " << total_sent << " bytes\n";
        
        std::cout<<"Closing server...\n";
        close(client_fd);
        close(socket_fd);

}
