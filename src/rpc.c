#define POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600

#include "rpc.h"
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BASE_10 10 
#define MAX_PORT_SIZE 16

int create_listening_socket(char* service);


// SERVER SIDE

struct rpc_server {
    /* Add variable(s) for server state */ // use primitive data types that don't require free() calls
    int listen_sockfd;
    char port[MAX_PORT_SIZE+1];


};

// Called before rpc_register, use for whatever is needed
// Should return a pointer to the rpc_server struct defined by you on success,
// and NULL on failure
rpc_server *rpc_init_server(int port) {

    rpc_server* srv = (rpc_server *) malloc(sizeof(rpc_server));
    // return NULL on failure
    if (port == NULL)
        fprintf(stderr, "ERROR: No port provided\n");
        return NULL;

	//if (argc < 2) {
	//	fprintf(stderr, "ERROR, no port provided\n");
	//	exit(EXIT_FAILURE);
	//}

	/* Create the listening socket */
    //char port_buffer[MAX_PORT_SIZE + 1];
    itoa(port, srv->port, BASE_10);
    printf("Server Port: %s\n", srv->port);
    //strcpy(srv->port, port_buffer);

	if ((srv->listen_sockfd = create_listening_socket(srv->port)) < 0) {
        fprintf(stderr, "ERROR: Failed to create listening socket\n");
        return NULL;
    }


    // if failure return NULL, else return srv
    return srv;
}

// Let the subsystem know what function to call when a request
// is received. Returns non-negative number on success (possibly an ID
// for this handler, but a constant is fine), and -1 on failure.
// If any of the arguments are NULL then -1 should be returned
// Refer to spec for things that should be considered in relation to name
// If there is already a function registered with name 'name', then
// the old function should be replaced by the new one
// Should be able to register at least 10 functions (or any amount really) for full marks, 
// but can start with only keeping track of one for testing purposes
int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    return -1;
}


// Waits for incoming requests for any of the registered functions, or rpc_find, on the
// port specified in rpc_init_server of any interfance. If it is a function call request, it will
// call the requested function, send a reply to the caller, and resume waiting for new
// requests. If it is rpc_find, it will reply to the caller saying whether the name was found or not,
// or possibly an error code
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) {
        return;
    }

    char* port = srv->port;
    int sockfd = srv->listen_sockfd;
    int newsockfd, n, i;
	char buffer[256], ip[INET6_ADDRSTRLEN];
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;


    // Listen on socket - means we're ready to accept connections,
	// incoming connection requests will be queued, man 3 listen
	if (listen(sockfd, 5) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
	client_addr_size = sizeof client_addr;
	newsockfd =
		accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
	if (newsockfd < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	// Read characters from the connection, then process
	while (1) {
		n = read(newsockfd, buffer, 255); // n is number of characters read
		if (n < 0) {
			perror("ERROR reading from socket");
			exit(EXIT_FAILURE);
		}
		// Null-terminate string
		buffer[n] = '\0';

		// Disconnect
		if (n == 0) {
			break;
		}

		// Exit on Goodbye
		if (strncmp(buffer, "GOODBYE-CLOSE-TCP", 17) == 0) {
			break;
		}

		// Convert to uppercase
		for (i = 0; i < n; i++) {
			buffer[i] = toupper(buffer[i]);
		}

		// A rather ugly solution for the buffer
		char initial[] = "Here is the message in upper: ";
		// Move original text in buffer (with \0) forwards
		memmove(buffer + strlen(initial), buffer, n + 1);
		// Prepend "Here is message in upper: " without \0
		memmove(buffer, initial, strlen(initial));
		printf("%s\n", buffer);
		// strlen only because content of buffer is null-terminated string
		n = write(newsockfd, buffer, strlen(buffer));
		if (n < 0) {
			perror("write");
			exit(EXIT_FAILURE);
		}
	}

	close(sockfd);
	close(newsockfd);

    // Note that this function will not usually return (i.e. will not reach this line)
    return;
}






// CLIENT SIDE

// use primitive data types that don't require free() calls
struct rpc_client {
    /* Add variable(s) for client state */
    int sockfd;
    char port[MAX_PORT_SIZE+1];

};

// use primitive data types that don't require free() calls
struct rpc_handle { // handle into rpc like a file handle that lets the client
                    // invoke a function. Returned by rpc_find and then used by rpc_call
    /* Add variable(s) for handle */

};

// Called before rpc_find or rpc_call. Use this for whatever you need.
// The string addr and integer port are the text-based IP address and numeric
// port number passed in on the command line
// This function should return a non-NULL pointer to a rpc_client struct containing
// client state information on success and NULL on failure
rpc_client *rpc_init_client(char *addr, int port) {
    
    // Malloc client state
    rpc_client* cl = (rpc_client *) malloc(sizeof(rpc_client));

    int s;
	struct addrinfo hints, *servinfo, *rp;

	//if (argc < 3) {
	//	fprintf(stderr, "usage %s hostname port\n", argv[0]);
	//	exit(EXIT_FAILURE);
	//}

	// Create address
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	// Get addrinfo of server. From man page:
	// The getaddrinfo() function combines the functionality provided by the
	// gethostbyname(3) and getservbyname(3) functions into a single interface
    
    //char port_buffer[MAX_PORT_SIZE + 1];
    itoa(port, cl->port, BASE_10);
    printf("Client Port: %s\n", cl->port);
    //strcpy(cl->port, port_buffer);

	s = getaddrinfo(addr, cl->port, &hints, &servinfo);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		return NULL;
	}

	// Connect to first valid result
	// Why are there multiple results? see man page (search 'several reasons')
	// How to search? enter /, then text to search for, press n/N to navigate
	for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
		cl->sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (cl->sockfd == -1)
			continue;

		if (connect(cl->sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; // success

		close(cl->sockfd); // failure
	}
	if (rp == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return NULL;
	}

	freeaddrinfo(servinfo);

    return cl;
}


// At the client, tell the subsystem what details are required to place a call. The return value is a handle
// struct (not handler) for the remote procedure, which is passed to the rpc_call function.
// If name is not registered, it should return NULL. If any of the arguments are NULL then NULL
// should be returned. If the find operation fails, it returns NULL.
rpc_handle *rpc_find(rpc_client *cl, char *name) {

    char buffer[256];
    int n;

    // Persistent connection from client to server
	while (1) {
		// Read message from stdin
		printf("Please enter the message: ");
		if (fgets(buffer, 255, stdin) == NULL) {
			exit(EXIT_SUCCESS);
		}
		// Remove \n that was read by fgets
		buffer[strlen(buffer) - 1] = 0;

		// Send message to server
		n = write(cl->sockfd, buffer, strlen(buffer));
		if (n < 0) {
			perror("socket");
			exit(EXIT_FAILURE);
		}

		if (strncmp(buffer, "GOODBYE-CLOSE-TCP", 17) == 0)
			break;

		// Read message from server
		n = read(cl->sockfd, buffer, 255);
		if (n == 0) {
			break;
		}
		if (n < 0) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		// Null-terminate string
		buffer[n] = '\0';
		printf("%s\n", buffer);
	}

    return NULL;
}

// This function causes the subsystem to run the remote procedure, and returns the value.
// If the call fails, it returns NULL. NULL should be returned if any of the arguments are NULL. If
// this returns a non-NULL value, then it should dynamically alocate (by malloc) both the rpc_data structure
// and its data2 field. The client will free these by rpc_data_free (defined below)
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    char buffer[256];
    int n;

    // Persistent connection from client to server
	while (1) {
		// Read message from stdin
		printf("Please enter the message: ");
		if (fgets(buffer, 255, stdin) == NULL) {
			exit(EXIT_SUCCESS);
		}
		// Remove \n that was read by fgets
		buffer[strlen(buffer) - 1] = 0;

		// Send message to server
		n = write(cl->sockfd, buffer, strlen(buffer));
		if (n < 0) {
			perror("socket");
			exit(EXIT_FAILURE);
		}

		if (strncmp(buffer, "GOODBYE-CLOSE-TCP", 17) == 0)
			break;

		// Read message from server
		n = read(cl->sockfd, buffer, 255);
		if (n == 0) {
			break;
		}
		if (n < 0) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		// Null-terminate string
		buffer[n] = '\0';
		printf("%s\n", buffer);
	}

    return NULL;
}

// Called after the final rpc_call or rpc_find (i.e. final use of RPC system by client)
// Use for whatever is needed; it should at least free(cl)
// If it is (mistakenly) called on a client that has already been closed, or cl == NULL, it 
// should return without error. (Think: How can you tell if it has already been closed)
void rpc_close_client(rpc_client *cl) {
    // If client hasn't yet been closed, close it
    if (cl != NULL && (write(cl->sockfd, "test", strlen("test"))) != -1) {
        close(cl->sockfd);
        free(cl);
        cl = NULL;
    }
    
    // If client has already been closed or cl == NULL simply return without error
    else {
        return;
    }
}





// SHARED FUNCTION

// Frees the memory allocated for a dynamically allocated rpc_data struct
void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}






// Should return -1 in the case that there is a failure so that rpc_init_server knows to return NULL
int create_listening_socket(char* service) {
	int re, s, sockfd;
	struct addrinfo hints, *res;

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;       // IPv6
	hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	s = getaddrinfo(NULL, service, &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
	}

	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		return -1;
	}
  
	// Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		return -1;
	}
	freeaddrinfo(res);

	return sockfd;
}