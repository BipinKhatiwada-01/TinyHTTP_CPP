#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(){
    
        int socket_fd = socket(AF_INET, SOCK_STREAM , 0);   

        if (socket_fd == -1) {
            std::perror("socket");
            return 1;
        }

        std::cout << "Socket created: " << socket_fd << '\n';

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

        std::cout<<"Closing server...\n";

        close(client_fd);
        close(socket_fd);

}
