#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

static const uint32_t MSG_JOIN = 1
static const uint32_t MSG_PUBLISH = 2
static const uint32_t MSG_SEARCH = 3
static const uint32_t MSG_SEARCH_REPLY = 4

struct peer_entry {
	uint32_t id;
	int socket_descriptor;
	struct sockaddr_in address;
	vector<string> files;
	bool peer_joined;
}

int bind_and_listen(const char *port_str)



bool recv_all(int sock, void *buf, size_t len) {
	size_t total = 0;
	auto *p = static_cast<uint8_t*>(buf);

	while (total < len) {
		size_t n = recv(soc, p+total, len-total, 0);
		if (n < 0) {
			if (errno == EINTR) continue;
			std::perror("recv");
			return false;
		}
		if (n == 0) {
			return false;
		}
	}
	total += static_cast<size_t>(n);
}

