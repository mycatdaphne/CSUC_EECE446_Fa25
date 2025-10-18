// EECE 446 - Program 2: Peer-to-Peer Introduction
// Names: <Your Names>
// Semester: Fall 2025
// Description:
// Simple peer application that communicates with the registry.
// Supports JOIN, PUBLISH, SEARCH, and EXIT commands.

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace std;

// Function prototypes
int connect_to_registry(const char *host, const char *port);
void send_join(int sock, uint32_t peer_id);
void send_publish(int sock);
void send_search(int sock);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "Usage: ./peer <registry_host> <registry_port> <peer_id>\n";
        return 1;
    }

    const char *host = argv[1];
    const char *port = argv[2];
    uint32_t peer_id = atoi(argv[3]);

    int sock = connect_to_registry(host, port);
    if (sock < 0) {
        cerr << "Could not connect to registry.\n";
        return 1;
    }

    string cmd;
    while (true) {
        cout << "Enter a command: ";
        if (!(cin >> cmd)) break;

        if (cmd == "JOIN")        send_join(sock, peer_id);
        else if (cmd == "PUBLISH") send_publish(sock);
        else if (cmd == "SEARCH")  send_search(sock);
        else if (cmd == "EXIT")    break;
        else cout << "Unknown command.\n";
    }

    close(sock);
    return 0;
}

/* ==================== CONNECT TO REGISTRY ==================== */
int connect_to_registry(const char *host, const char *port) {
    int sock;
    struct sockaddr_in serv;
    int port_num = atoi(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    serv.sin_family = AF_INET;
    serv.sin_port   = htons(port_num);

    if (inet_pton(AF_INET, host, &serv.sin_addr) <= 0) {
        cerr << "Invalid address.\n";
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

/* ==================== JOIN ==================== */
void send_join(int sock, uint32_t peer_id) {
    uint8_t action = 0;              // JOIN = 0
    uint32_t id_net = htonl(peer_id);
    unsigned char msg[5];
    msg[0] = action;
    memcpy(msg + 1, &id_net, 4);

    send(sock, msg, 5, 0);
}

/* ==================== PUBLISH ==================== */
void send_publish(int sock) {
    uint8_t action = 1;              // PUBLISH = 1
    vector<string> files;

    DIR *dir = opendir("SharedFiles");
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
#ifdef DT_REG
        if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN)
            files.push_back(entry->d_name);
#else
        files.push_back(entry->d_name);
#endif
    }
    closedir(dir);

    uint32_t count = htonl(files.size());
    vector<unsigned char> msg;
    msg.push_back(action);
    unsigned char *p = (unsigned char*)&count;
    msg.insert(msg.end(), p, p + 4);

    for (string &name : files) {
        for (char c : name) msg.push_back((unsigned char)c);
        msg.push_back('\0');
    }

    send(sock, msg.data(), msg.size(), 0);
}

/* ==================== SEARCH ==================== */
void send_search(int sock) {
    uint8_t action = 2;              // SEARCH = 2
    string filename;
    cout << "Enter a file name: ";
    cin >> filename;

    vector<unsigned char> msg;
    msg.push_back(action);
    for (char c : filename) msg.push_back((unsigned char)c);
    msg.push_back('\0');

    send(sock, msg.data(), msg.size(), 0);

    unsigned char buf[10];
    int n = recv(sock, buf, 10, 0);
    if (n <= 0) {
        cerr << "Error receiving response.\n";
        return;
    }

    uint32_t peer_id, ip_raw;
    uint16_t port;
    memcpy(&peer_id, buf, 4);
    memcpy(&ip_raw,  buf + 4, 4);
    memcpy(&port,    buf + 8, 2);

    peer_id = ntohl(peer_id);
    ip_raw  = ntohl(ip_raw);
    port    = ntohs(port);

    if (peer_id == 0 && ip_raw == 0 && port == 0) {
        cout << "File not indexed by registry\n";
        return;
    }

    struct in_addr addr;
    addr.s_addr = htonl(ip_raw);
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));

    cout << "File found at\n";
    cout << "Peer " << peer_id << endl;
    cout << ip_str << ":" << port << endl;
}
