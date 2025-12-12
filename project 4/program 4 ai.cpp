/*
 * Program 4: Peer-to-Peer Registry
 * Group Members: [Your Name], [Partner Name]
 * Class: EECE 446
 * Semester: Fall 2025
 *
 * Description: A single-threaded P2P registry using poll() for I/O multiplexing.
 * It manages peer connections, indexes files, and handles SEARCH requests.
 */

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

const uint8_t MSG_JOIN    = 1;
const uint8_t MSG_PUBLISH = 2;
const uint8_t MSG_SEARCH  = 3;

const int MAX_FILES = 10;
const int MAX_FILENAME_LEN = 100;
const int BACKLOG = 10;

struct PeerInfo {
    int socket_fd;
    uint32_t id;
    struct sockaddr_in addr;
    std::vector<std::string> files;
    bool has_joined;
};

struct SearchResponse {
    uint32_t peer_id;
    uint32_t ip_addr;
    uint16_t port;
}

void error_exit(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

bool recv_all(int sock, void* buffer, size_t length) {
    size_t total_received = 0;
    char* ptr = (char*)buffer;
    
    while (total_received < length) {
        ssize_t received = recv(sock, ptr + total_received, length - total_received, 0);
        if (received <= 0) {
            return false;
        }
        total_received += received;
    }
    return true;
}

std::string get_ip_str(const struct sockaddr_in& addr) {
    char ip_str[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &(addr.sin_addr), ip_str, INET_ADDRSTRLEN) == NULL) {
        return "Unknown";
    }
    return std::string(ip_str);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return EXIT_FAILURE;
    }
    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port number." << std::endl;
        return EXIT_FAILURE;
    }

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) error_exit("socket");

    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error_exit("setsockopt");
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        error_exit("bind");
    }

    if (listen(listen_sock, BACKLOG) < 0) {
        error_exit("listen");
    }

    std::vector<struct pollfd> pfds;
    std::vector<PeerInfo> peers;

    struct pollfd listener_pfd;
    listener_pfd.fd = listen_sock;
    listener_pfd.events = POLLIN;
    pfds.push_back(listener_pfd);

    while (true) {
        int poll_count = poll(pfds.data(), pfds.size(), -1);

        if (poll_count < 0) {
            error_exit("poll");
        }
        for (size_t i = 0; i < pfds.size(); ++i) {
            if (pfds[i].revents & POLLIN) {
                if (pfds[i].fd == listen_sock) {
                    struct sockaddr_in client_addr = {};
                    socklen_t addr_len = sizeof(client_addr);
                    int new_fd = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
                    
                    if (new_fd < 0) {
                        perror("accept");
                    } else {
                        struct pollfd client_pfd;
                        client_pfd.fd = new_fd;
                        client_pfd.events = POLLIN;
                        pfds.push_back(client_pfd);

                        PeerInfo new_peer;
                        new_peer.socket_fd = new_fd;
                        
                        struct sockaddr_in peer_addr_check = {};
                        socklen_t len = sizeof(peer_addr_check);
                        if (getpeername(new_fd, (struct sockaddr*)&peer_addr_check, &len) == 0) {
                            new_peer.addr = peer_addr_check;
                        }
                        
                        peers.push_back(new_peer);
                    }
                } else {
                    int peer_idx = i - 1;
                    PeerInfo& current_peer = peers[peer_idx];
                    int sock = current_peer.socket_fd;
                    uint8_t msg_type;
                    if (!recv_all(sock, &msg_type, sizeof(msg_type))) {
                        // Disconnect logic
                        close(sock);
                        pfds.erase(pfds.begin() + i);
                        peers.erase(peers.begin() + peer_idx);
                        i--;
                        continue;
                    }

                    if (msg_type == MSG_JOIN) {
                        uint32_t id_net;
                        if (recv_all(sock, &id_net, sizeof(id_net))) {
                            current_peer.id = ntohl(id_net);
                            current_peer.has_joined = true;
                            
                            std::cout << "TEST] JOIN " << current_peer.id << std::endl;
                        }

                    } else if (msg_type == MSG_PUBLISH) {
                        uint32_t count_net;
                        if (recv_all(sock, &count_net, sizeof(count_net))) {
                            uint32_t count = ntohl(count_net);
                            if (count > MAX_FILES) count = MAX_FILES;
                            std::cout << "TEST] PUBLISH " << count;

                            for (uint32_t k = 0; k < count; ++k) {
                                char filename_buf[MAX_FILENAME_LEN] = {0}; // Init to 0
                                if (recv_all(sock, filename_buf, sizeof(filename_buf))) {
                                    filename_buf[MAX_FILENAME_LEN - 1] = '\0';
                                    std::string fname(filename_buf);
                                    current_peer.files.push_back(fname);
                                    std::cout << " " << fname;
                                }
                            }
                            std::cout << std::endl;
                        }

                    } else if (msg_type == MSG_SEARCH) {
                        char search_buf[MAX_FILENAME_LEN] = {0};
                        if (recv_all(sock, search_buf, sizeof(search_buf))) {
                            search_buf[MAX_FILENAME_LEN - 1] = '\0';
                            std::string target_file(search_buf);

                            bool found = false;
                            uint32_t found_id = 0;
                            struct sockaddr_in found_addr = {};

                            for (const auto& p : peers) {
                                if (!p.has_joined) continue;
                                for (const auto& f : p.files) {
                                    if (f == target_file) {
                                        found = true;
                                        found_id = p.id;
                                        found_addr = p.addr;
                                        break;
                                    }
                                }
                                if (found) break;
                            }

                            SearchResponse resp;
                            if (found) {
                                resp.peer_id = htonl(found_id);
                                resp.ip_addr = found_addr.sin_addr.s_addr;
                                resp.port    = found_addr.sin_port;
                                
                                std::cout << "TEST] SEARCH " << target_file << " " 
                                          << found_id << " " 
                                          << get_ip_str(found_addr) << ":" 
                                          << ntohs(found_addr.sin_port) << std::endl;
                            } else {
                                resp.peer_id = 0;
                                resp.ip_addr = 0;
                                resp.port    = 0;

                                std::cout << "TEST] SEARCH " << target_file << " 0 0.0.0.0:0" << std::endl;
                            }

                            send(sock, &resp, sizeof(resp), 0);
                        }
                    } else {
                    }
                }
            }
        }
    }

    close(listen_sock);
    return 0;
}