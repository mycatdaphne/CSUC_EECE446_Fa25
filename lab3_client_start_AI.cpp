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

/*
 * Lookup a host IP address and connect to it using service. Arguments match the first two
 * arguments to getaddrinfo(3).
 *
 * Returns a connected socket descriptor or -1 on error. Caller is responsible for closing
 * the returned socket.
 */
int lookup_and_connect( const char *host, const char *service );

int main( int argc, char **argv ) {
	int s = -1;
	const char *host_arg = NULL;
	const char *port = "80";
	const char *path = "/";

	/* Usage: program [host[/path]] [port]
	 * Example: ./lab3_client_start www.ecst.csuchico.edu/~kkredo/file.html 80
	 * The program will strip the path from the host when using getaddrinfo and
	 * use the path in the HTTP GET request. */

	if ( argc >= 2 ) {
		host_arg = argv[1];
	} else {
		fprintf( stderr, "Usage: %s host[/path] [port]\n", argv[0] );
		fprintf( stderr, "Example: %s www.ecst.csuchico.edu/~kkredo/file.html 80\n", argv[0] );
		return 1;
	}

	if ( argc >= 3 ) {
		port = argv[2];
	}

	/* If host_arg contains a path (slash), split it into host and path */
	char hostbuf[256];
	char pathbuf[1024];
	const char *slash = strchr( host_arg, '/' );
	if ( slash ) {
		size_t hostlen = slash - host_arg;
		if ( hostlen >= sizeof(hostbuf) ) hostlen = sizeof(hostbuf) - 1;
		memcpy( hostbuf, host_arg, hostlen );
		hostbuf[hostlen] = '\0';
		strncpy( pathbuf, slash, sizeof(pathbuf)-1 );
		pathbuf[sizeof(pathbuf)-1] = '\0';
		path = pathbuf;
	} else {
		strncpy( hostbuf, host_arg, sizeof(hostbuf)-1 );
		hostbuf[sizeof(hostbuf)-1] = '\0';
	}

	const char *host = hostbuf;

	/* Modify the program so it
	 *
	 * 1) connects to www.ecst.csuchico.edu on port 80 (mostly done above)
	 * 2) sends "GET /~kkredo/file.html HTTP/1.0\r\n\r\n" to the server
	 * 3) receives all the data sent by the server (HINT: "orderly shutdown" in recv(2))
	 * 4) prints the total number of bytes received
	 *
	 * */

	/* Connect to host:port */
	if ( ( s = lookup_and_connect( host, port ) ) < 0 ) {
		return 1;
	}

	/* Build GET request using the path we extracted */
	char http_get[1200];
	snprintf( http_get, sizeof(http_get), "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host );

	if ( send( s, http_get, strlen( http_get ), 0 ) == -1 ) {
		perror( "stream-talk-client: send" );
		close( s );
		return 1;
	}

	char buf[1024];
	int total_bytes = 0;
	ssize_t n;

	while ( ( n = recv( s, buf, sizeof( buf ) - 1, 0 ) ) > 0 ) {
		buf[n] = '\0';
		total_bytes += n;
	}

	if ( n < 0 ) {
		perror( "stream-talk-client: recv" );
	}

	printf( "Total bytes received: %d\n", total_bytes );

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
