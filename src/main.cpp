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

        while (request.find('\n') == std::string::npos) {

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
                std::cout << "Client disconnected.\n";
                break;
            }
            else {
                request.append(
                    chunk,
                    static_cast<std::size_t>(more_bytes_received)
                );
            }
        }

        std::cout << "Received request: " << request << '\n';

        const char response[] = "Hello from TinyHTTP!\n";

        ssize_t bytes_sent = send(client_fd, response, sizeof(response) - 1, 0);

        if(bytes_sent == -1){
            std::perror("send");
            close(client_fd);
            close(socket_fd);
            return 1;
        }
        else if (bytes_sent == 0) {
            std::cout << "No bytes were sent\n";
        }
        else{
            std::cout<<"Sent "<< bytes_sent <<" bytes\n";
        }

        std::cout<<"Closing server...\n";
        close(client_fd);
        close(socket_fd);

}
