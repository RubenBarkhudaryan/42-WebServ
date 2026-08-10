#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <poll.h>

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int port = 8080;

    // ==========================================
    // STAGE 1: CORE SOCKET SETUP
    // ==========================================
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Error: Socket creation failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed" << std::endl;
        return EXIT_FAILURE;
    }

    // ==========================================
    // STAGE 3: NON-BLOCKING ENFORCEMENT
    // ==========================================
    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl(O_NONBLOCK) failed" << std::endl;
        return EXIT_FAILURE;
    }
    if (fcntl(server_fd, F_SETFD, FD_CLOEXEC) < 0) {
        std::cerr << "Error: fcntl(FD_CLOEXEC) failed" << std::endl;
        return EXIT_FAILURE;
    }

    // ==========================================
    // BIND AND LISTEN
    // ==========================================
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(port);       

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Error: Bind failed on port " << port << std::endl;
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 128) < 0) {
        std::cerr << "Error: Listen failed" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Engine listening on port " << port << "..." << std::endl;

    // ==========================================
    // STAGE 2: THE MULTIPLEXER LOOP
    // ==========================================
    std::vector<struct pollfd> poll_fds;

    struct pollfd server_pollfd;
    server_pollfd.fd = server_fd;
    server_pollfd.events = POLLIN; 
    server_pollfd.revents = 0;
    poll_fds.push_back(server_pollfd);

    while (true) {
        int ready = poll(&poll_fds[0], poll_fds.size(), -1); 
        
        if (ready < 0) {
            std::cerr << "Error: poll() crashed" << std::endl;
            break;
        }

        size_t current_size = poll_fds.size();
        for (size_t i = 0; i < current_size; i++) {
            
            if (poll_fds[i].revents == 0) {
                continue;
            }

            // A. NEW CONNECTION
            if (poll_fds[i].fd == server_fd) {
                if (poll_fds[i].revents & POLLIN) {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd >= 0) {
                        std::cout << "New client connected! FD: " << client_fd << std::endl;
                        
                        fcntl(client_fd, F_SETFL, O_NONBLOCK);
                        fcntl(client_fd, F_SETFD, FD_CLOEXEC);

                        struct pollfd client_pollfd;
                        client_pollfd.fd = client_fd;
                        client_pollfd.events = POLLIN | POLLOUT;
                        client_pollfd.revents = 0;
                        poll_fds.push_back(client_pollfd);
                    }
                }
            } 
            // B. EXISTING CLIENT ACTIVITY
            else {
                bool disconnect_client = false;

                // Client sent data
                if (poll_fds[i].revents & POLLIN) {
                    char buffer[1024] = {0};
                    int bytes_read = read(poll_fds[i].fd, buffer, sizeof(buffer) - 1);
                    
                    if (bytes_read > 0) {
                        std::cout << "Data from FD " << poll_fds[i].fd << ":\n" << buffer << std::endl;
                        
                        // Let's queue a simple HTTP response so the browser doesn't hang
                        const char* http_response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello Webserv";
                        write(poll_fds[i].fd, http_response, std::strlen(http_response));
                        disconnect_client = true; // Close connection after one response for basic testing
                        
                    } else if (bytes_read == 0) {
                        disconnect_client = true;
                    }
                }

                // Handle errors or disconnections
                if (disconnect_client || (poll_fds[i].revents & (POLLERR | POLLHUP))) {
                    std::cout << "Client disconnected on FD: " << poll_fds[i].fd << std::endl;
                    close(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    
                    i--;
                    current_size--;
                }
            }
        }
    }
    
    close(server_fd);
    return EXIT_SUCCESS;
}