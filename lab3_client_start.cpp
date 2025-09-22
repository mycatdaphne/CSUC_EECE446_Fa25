/* This code is an updated version of the sample code from "Computer Networks: A Systems
 * Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis. Some code comes from
 * man pages, mostly getaddrinfo(3). */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect( const char *host, const char *service );

int main(int argc, char *argv[]) {

	if (argc < 2) {
		std::cout << "Incorrect number of arguments. Must enter number of bytes in one chunk" << std::endl;
		return 1;
	}

	int s;
	const char *host = "www.ecst.csuchico.edu";
	const char *port = "80";

	int bytes_received;
	int bytes_sent = 0;
	char buff[4115];
	char *bp = buff;
	int chunk_size = atoi(argv[1]);
	int total_tags = 0;
	
	/* Lookup IP and connect to server */
	if ( ( s = lookup_and_connect( host, port ) ) < 0 ) {
		exit( 1 );
	}

	/* Modify the program so it
	 *
	 * 1) connects to www.ecst.csuchico.edu on port 80 (mostly done above)
	 * 2) sends "GET /~kkredo/file.html HTTP/1.0\r\n\r\n" to the server
	 * 3) receives all the data sent by the server (HINT: "orderly shutdown" in recv(2))
	 * 4) prints the total number of bytes received
	 *
	 * */
	
	 //send
	const char *msg = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
	int len = strlen(msg);
	while (bytes_sent < len) {
		int n = send(s, msg + bytes_sent, len - bytes_sent, 0);
		if (n < 0) {
			perror("send");
			close(s);
			exit(1);
		}
		bytes_sent += n;
	}

	//recv
	int total_received = 0;

	while((bytes_received = recv(s, bp, chunk_size, 0)) > 0) {
		total_received += bytes_received;

		std::string chunk(buff, bytes_received);
		std::cout << chunk;
		int pos = 0;
		while ((pos = chunk.find("<h1>", pos)) != std::string::npos) {
			total_tags++;
			pos += 4;
		}
	}

	std::cout << "Number of <h1> tags: " << total_tags << std::endl;
	std::cout << "Number of bytes: " << total_received << std::endl;

	close( s );

	return 0;
}

int lookup_and_connect( const char *host, const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ( ( s = getaddrinfo( host, service, &hints, &result ) ) != 0 ) {
		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror( s ) );
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for ( rp = result; rp != NULL; rp = rp->ai_next ) {
		if ( ( s = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol ) ) == -1 ) {
			continue;
		}

		if ( connect( s, rp->ai_addr, rp->ai_addrlen ) != -1 ) {
			break;
		}

		close( s );
	}
	if ( rp == NULL ) {
		perror( "stream-talk-client: connect" );
		return -1;
	}
	freeaddrinfo( result );

	return s;
}
