#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <dirent.h>
#include <string>

using namespace std;

int reg_connect(const char* host, const char* port);
void join(int sock, uint32_t peer_id);
void search(int sock);
void publish(int sock);

int main(int argc, char *argv[]) {
	if (argc != 4) {
		cerr << "args" << endl;
		return 1;
	}

	const char *host = argv[1];
	const char *port = argv[2];
	uint32_t peer_id = atoi(argv[3]);

	int sock = reg_connect(host, port);
	if (sock < 0) {
		cerr << "Could not connect to registry" << endl;
		return 1;
	}

	string input;
	while (true) {
		cout << "Enter a command: ";
		if (!(cin >> input)) break;
		else if (input == "SEARCH") search(sock);
		else if (input == "JOIN") join(sock, peer_id);
		else if (input == "PUBLISH") publish(sock);
		else if (input == "EXIT") break;
		else cout << "Unknown command" << endl;
	}

	close(sock);
	return 0;
}

int reg_connect(const char *host, const char *port) {
	int sock;
	struct sockaddr_in reg = {};
	int port_num = atoi(port);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		cerr << "socket" << endl;
		return -1;
	}

	reg.sin_family = AF_INET;
	reg.sin_port   = htons(port_num);

	if (inet_pton(AF_INET, host, &reg.sin_addr) <= 0) {
		cerr << "address" << endl;
		close(sock);
		return -1;
	}

	if (::connect(sock, (struct sockaddr*)&reg, sizeof(reg)) < 0) {
		cerr << "connect" << endl;
		close(sock);
		return -1;
	}

	return sock;

}

void join(int sock, uint32_t peer_id) {
	uint8_t action = 0;
	uint32_t id_net = htonl(peer_id);

	char msg[5];
	msg[0] = action;

	memcpy(msg + 1, &id_net, 4);

	send (sock, msg, 5, 0);
}

void search(int sock) {
	uint8_t action = 2;
	string filename;
	cout << "Enter file name: " << endl;
	cin >> filename;

	vector<unsigned char> msg;
	msg.push_back(action);
	for (size_t i = 0; i < filename.length(); i++) {
		msg.push_back((unsigned char)filename[i]);
	}
	msg.push_back('\0');

	send(sock, msg.data(), msg.size(), 0);

	char buf[10];
	int n = recv(sock, buf, 10, 0);
	if (n <= 0) {
		cerr << "response" << endl;
		return;
	}

	uint32_t peer_id, ip_raw;
	uint16_t port;
	memcpy(&peer_id, buf, 4);
	memcpy(&ip_raw, buf + 4, 4);
	memcpy(&port, buf + 8, 2);

	peer_id = ntohl(peer_id);
	ip_raw = ntohl(ip_raw);
	port = ntohs(port);

	if (peer_id == 0 && ip_raw == 0 && port == 0) {
		cout << "File not indexed by registry" << endl;
		return;
	}

	struct in_addr address;
	address.s_addr = ip_raw;
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &address, ip, sizeof(ip));

	cout << "File found at" << endl;
	cout << "Peer " << peer_id << endl;
	cout << ip << ":" << port << endl;
}

void publish(int sock) {
	uint8_t action = 1;

	vector<string> files;

	DIR *dir = opendir("SharedFiles");
	if (dir == nullptr) {
		cerr << "cant open directory" << endl;
		return;
	}

	struct dirent *file;
	while ((file = readdir(dir)) != nullptr) {
		if (file->d_type == DT_REG) {
			files.push_back(string(file->d_name));
		}
	}

	closedir(dir);

	vector<unsigned char> msg;
	msg.push_back(action);

    uint32_t count_net = htonl(files.size());
    unsigned char bytes[4];
    memcpy(bytes, &count_net, 4);
    msg.insert(msg.end(), bytes, bytes + 4);

    for (size_t i = 0; i < files.size(); i++) {
        string name = files[i];
        for (size_t j = 0; j < name.size(); j++) {
            msg.push_back((unsigned char)name[j]);
        }
        msg.push_back('\0');
    }

	send(sock, msg.data(), msg.size(), 0);
}