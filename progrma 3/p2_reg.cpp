#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace fs = std::filesystem;

struct PeerInfo {
    uint32_t id;
    std::string ip;   
    uint16_t port;
    bool found;
};

int lookup_and_connect(const char *host, const char *service) {
    struct addrinfo addr{}; 
    addr.ai_family = AF_UNSPEC;
    addr.ai_socktype = SOCK_STREAM;
    addr.ai_flags = 0;
    addr.ai_protocol = 0;

    struct addrinfo *result = nullptr;
    int s = getaddrinfo(host, service, &addr, &result);
    if (s != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(s) << "\n";
        return -1;
    }

    int sock = -1;
    for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) {
            break; // success
        }
        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);
    if (sock == -1) {
        std::perror("connect");
    }
    return sock;
}

ssize_t SEND_single_call(int sock, const uint8_t *buf, size_t len) {
    ssize_t n = send(sock, buf, len, 0); 
    if (n < 0) {
        std::perror("send");
        return -1;
    }
    if ((size_t)n != len) {
        std::cerr << "Warning: partial send (" << n << " of " << len << " bytes)."
                  << " Assignment asks that requests be sent in a single send() call.\n";
    }
    return n;
}

// ssize_t recv(int sock, uint8_t *buf, size_t len) {
//     size_t total = 0;
//     while (total < len) {
//         ssize_t r = recv(sock, buf + total, len - total, 0);
//         if (r < 0) {
//             if (errno == EINTR) continue;
//             std::perror("recv");
//             return -1;
//         }
//         if (r == 0) return static_cast<ssize_t>(total); 
//         total += static_cast<size_t>(r);
//     }
//     return static_cast<ssize_t>(total);
// }

bool do_join(int sock, uint32_t peer_id) {
    std::vector<uint8_t> buf;
    buf.reserve(1 + 4);
    buf.push_back(0); // Action = 0 (JOIN)
    uint32_t net_id = htonl(peer_id);
    uint8_t *p = reinterpret_cast<uint8_t *>(&net_id);
    buf.insert(buf.end(), p, p + 4);

    ssize_t sent = SEND_single_call(sock, buf.data(), buf.size());
    return (sent == static_cast<ssize_t>(buf.size()));
}

bool do_publish(int sock) {
    const fs::path shared = "SharedFiles";
    if (!fs::exists(shared) || !fs::is_directory(shared)) {
        std::cout << "Warning: SharedFiles directory does not exist or is not a directory. No files to publish.\n";
    }

    std::vector<std::string> filenames;
    for (const auto &entry : fs::directory_iterator(shared, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        if (fname.size() + 1 > 100) {
            std::cerr << "Skipping file '" << fname << "' (name > 99 chars).\n";
            continue;
        }
        filenames.push_back(fname);
    }

    uint32_t count = static_cast<uint32_t>(filenames.size());
    std::vector<uint8_t> buf;
    buf.reserve(1200);
    buf.push_back(1);

    uint32_t net_count = htonl(count);
    uint8_t *pc = reinterpret_cast<uint8_t *>(&net_count);
    buf.insert(buf.end(), pc, pc + 4);

    for (const auto &f : filenames) {
        buf.insert(buf.end(), f.begin(), f.end());
        buf.push_back('\0');
    }

    if (buf.size() > 1200) {
        std::cerr << "Error: PUBLISH request would exceed 1200 bytes (" << buf.size() << "). Aborting.\n";
        return false;
    }

    ssize_t sent = SEND_single_call(sock, buf.data(), buf.size());
    return (sent == static_cast<ssize_t>(buf.size()));
}

PeerInfo search_file(int sock, const std::string &filename) {
    PeerInfo ret{};
    ret.found = false;

    std::vector<uint8_t> buf;
    buf.reserve(1 + filename.size() + 1);
    buf.push_back(2);
    buf.insert(buf.end(), filename.begin(), filename.end());
    buf.push_back('\0');

    ssize_t snt = SEND_single_call(sock, buf.data(), buf.size());
    if (snt < 0) {
        return ret;
    }
    if ((size_t)snt != buf.size()) {
        std::cerr << "Partial SEARCH request sent; continuing to await response (may fail).\n";
    }

    uint8_t resp[10];
    ssize_t got = recv(sock, resp, sizeof(resp), 0);
    if (got < 0) {
        return ret;
    }
    if (got == 0) {
        std::cerr << "Connection closed by registry while waiting for SEARCH response.\n";
        return ret;
    }
    if (static_cast<size_t>(got) < sizeof(resp)) {
        std::cerr << "Incomplete SEARCH response (" << got << " bytes). Treating as file not found.\n";
        return ret;
    }

    uint32_t net_peer_id;
    uint32_t net_ip;
    uint16_t net_port;

    std::memcpy(&net_peer_id, resp + 0, 4);
    std::memcpy(&net_ip, resp + 4, 4);
    std::memcpy(&net_port, resp + 8, 2);

    uint32_t peer_id = ntohl(net_peer_id);
    uint32_t ip_addr = net_ip;
    uint16_t port = ntohs(net_port);

    if (peer_id == 0 && ip_addr == 0 && port == 0) {
        ret.found = false;
        return ret;
    }

    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &ip_addr, ip_str, sizeof(ip_str)) == nullptr) {
        std::perror("inet_ntop");
        ret.found = false;
        return ret;
    }

    ret.id = peer_id;
    ret.ip = std::string(ip_str);
    ret.port = port;
    ret.found = true;
    return ret;
}

int fetch_file_from_peer(const PeerInfo &peer, const std::string &filename) {
    int ip = inet_addr(peer.ip.c_str());
    int port = peer.port;
    int peer_sock = lookup_and_connect(peer.ip.c_str(), std::to_string(port).c_str());


    std::vector<uint8_t> buf;
    buf.reserve(1 + filename.size() + 1);
    buf.push_back(3);
    buf.insert(buf.end(), filename.begin(), filename.end());
    buf.push_back('\0');

    ssize_t snt = SEND_single_call(peer_sock, buf.data(), buf.size());
    if (snt < 0) {
        std::cerr << "no" << std::endl;
        return -1;
    }
    FILE *fp = fopen(filename.c_str(), "wb");

    uint8_t rec_buf[8192];
bool first_block = true;
while (true) {
    ssize_t bytes_fetched = recv(peer_sock, rec_buf, sizeof(rec_buf), 0);
    if (bytes_fetched > 0) {
        size_t offset = 0;
        if (first_block && rec_buf[0] == '\0') {
            offset = 1;
        }
        fwrite(rec_buf + offset, 1, bytes_fetched - offset, fp);
        first_block = false;
    } else if (bytes_fetched == 0) {
        break;
    } else {
        std::cerr << "Error receiving file data from peer.\n";
        fclose(fp);
        close(peer_sock);
        return -1;
    }
}

    fclose(fp);
    close(peer_sock);
    return 0;
    printf("yes\n");
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <registryHost> <registryPort> <peerID>\n";
        return 1;
    }

    const char *host = argv[1];
    const char *port = argv[2];
    char *endp = nullptr;
    unsigned long long id_ll = strtoull(argv[3], &endp, 10);
    if (endp == argv[3] || *endp != '\0' || id_ll == 0 || id_ll > 0xFFFFFFFFULL) {
        std::cerr << "Error: peerID must be a positive integer < 2^32.\n";
        return 1;
    }
    uint32_t peer_id = static_cast<uint32_t>(id_ll);

    int sock = lookup_and_connect(host, port);
    if (sock < 0) {
        std::cerr << "Failed to connect to registry " << host << ":" << port << "\n";
        return 1;
    }

    std::string cmd;
    while (true) {
        std::cout << "Enter a command: ";
        if (!std::getline(std::cin, cmd)) {
            break;
        }

        std::string up = cmd;
        for (char &c : up) c = static_cast<char>(toupper((unsigned char)c));

        if (up == "JOIN") {
            if (do_join(sock, peer_id)) {
            } else {
                std::cerr << "Failed to send JOIN request.\n";
            }

        } else if (up == "PUBLISH") {
            if (!do_publish(sock)) {
                std::cerr << "PUBLISH failed.\n";
            } else {
            }

        } else if (up == "SEARCH") {
            std::cout << "Enter a file name: ";
            std::string fname;
            if (!std::getline(std::cin, fname)) {
                std::cerr << "No filename input. Returning to command prompt.\n";
                continue;
            }
            PeerInfo pi = search_file(sock, fname);
            if (!pi.found) {
                std::cout << "File not indexed by registry\n";
            } else {
                std::cout << "File found at\n";
                std::cout << "Peer " << pi.id << "\n";
                std::cout << pi.ip << ":" << pi.port << "\n";
            }
        } else if (up == "FETCH") {
            std::cout << "Enter a file name";
            std::string fname;
            if (!std::getline(std::cin, fname)) {
                std::cerr << "No filename input.\n";
                continue;
            }
            PeerInfo pi = search_file(sock, fname);
            fetch_file_from_peer(pi, fname);
        } else if (up == "EXIT") {
            close(sock);
            break;
        } else {
            std::cout << "Unknown command. Use JOIN, PUBLISH, SEARCH, EXIT.\n";
        }
    

}
    return 0;
}
