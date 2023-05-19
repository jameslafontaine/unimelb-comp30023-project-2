#define POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600 
#define _DEFAULT_SOURCE         

#include "rpc.h"
#include "dict.h"
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <endian.h>

#define htonll htobe64
#define ntohll be64toh

#define BASE_10 10 
#define MAX_PORT_DIGITS 5
#define MAX_PORT_NUM 65535
#define MAX_NAME_LEN 1000
#define MIN_NAME_LEN 1
#define MIN_ASCII_CHAR 32
#define MAX_ASCII_CHAR 126
#define MAX_DATA1_BYTES 8
#define MAX_DATA2_BYTES 100000
#define MAX_REQUEST_BYTES 102000
#define FIND_REQUEST_BYTES 1005
#define NAME_LEN_PREFIX 2

#define REG_SUCCESS 21

#define ERROR_MSG_SIZE 2

#define ERRCODE_NAME_LEN 11
#define ERRCODE_PROC_NOTFOUND 12
#define ERRCODE_DATA2_PREFIX 13
#define ERRCODE_DATA2_LEN 14
#define ERRCODE_DATA2_MALFORMED 15
#define ERRCODE_READ_WRITE 16
#define ERRCODE_NULL_RESULT 17
#define ERRCODE_SOCKET 18
#define ERRCODE_MSG_PREFIX 19

#define RESP_MSG_SIZE 100100

int create_listening_socket(char* service);

int send_error_code(int sockfd, char error_code);

int receive_function(int sockfd, char func_name[MAX_NAME_LEN + 1]);

int send_function(int sockfd, char func_name[MAX_NAME_LEN + 1]);

rpc_data* receive_rpc_data(int sockfd);

int send_rpc_data(int sockfd, rpc_data* payload);

void send_find_response(int sockfd, const char status);





int receive_function(int sockfd, char func_name[MAX_NAME_LEN + 1]) {
	char buffer[MAX_NAME_LEN+1];
	int n;

	memset(buffer, 0, MAX_NAME_LEN+1);
	// read in name length prefix
	n = read(sockfd, buffer, NAME_LEN_PREFIX);
    while (n < NAME_LEN_PREFIX) {
		printf("Haven't received full name length prefix yet - reading more...\n");
		n += read(sockfd, buffer+n, 1);
		printf("Have now received %d bytes in total\n", n);
	}
	uint16_t func_name_len = ntohs(*(uint16_t*)&buffer);
	printf("Function name length received: %hu\n", func_name_len);
	// should probably move this check to rpc_find but doesn't hurt to have it here as well
	// if it doesn't mess anything up (except for the fact that it is clogging up the code)
	if (func_name_len < MIN_NAME_LEN || func_name_len > MAX_NAME_LEN) {
		fprintf(stderr, "Please provide a function name between 1 and 1000 characters in length\n");
		// reply with error code
		send_error_code(sockfd, ERRCODE_NAME_LEN);
		return -1;
	}
	// read in the actual function name + '\0'
	n = read(sockfd, buffer, func_name_len);
	while (n < func_name_len) {
		printf("Haven't received full function name yet - reading more...\n");
		n += read(sockfd, buffer+n, func_name_len - n);
		printf("Have now received %d bytes in total\n", n);
	}
	//printf("%c\n", buffer[0]);
	//printf("%c\n", buffer[1]);
	//printf("%c\n", buffer[2]);
	//printf("%c\n", buffer[3]);
	//printf("%c\n", buffer[4]);
	//memset(func_name, 0, MAX_NAME_LEN+1);
	snprintf(func_name, func_name_len + 1, "%s", buffer);
	printf("Received function name: %s\n", func_name);

	return 0;
}

int send_function(int sockfd, char func_name[MAX_NAME_LEN + 1]) {
	char buffer[MAX_NAME_LEN+1];
	int n;

	memset(buffer, 0, MAX_NAME_LEN+1);

	// Send the length of the provided name
	uint16_t name_len = (uint16_t) strlen(func_name);
	printf("Length of name sent: %hu\n", name_len);
	if (name_len < MIN_NAME_LEN || name_len > MAX_NAME_LEN) {
		fprintf(stderr, "Invalid name length - please provide a name between 1 and 1000 characters long\n");
		return -1;
	}
	uint16_t* buffer_cast = (uint16_t*) malloc(sizeof(uint16_t));
	*buffer_cast = htons(name_len);
	if ((n = write(sockfd, buffer_cast, 2)) < 2) {
		perror("socket");
		return -1;
	}
	free(buffer_cast);

	// Send the actual name to the server
	snprintf(buffer, name_len + 1, "%s", func_name);
	printf("Name sent: %s\n", func_name);
	if ((n = write(sockfd, buffer, name_len)) < name_len) {
		perror("socket");
		return -1;
	}
	return 0;
}


void send_find_response(int sockfd, const char status) {
	int n;
	char buffer[2];

	memset(buffer, 0, 2);

	buffer[0] = 'r';
	buffer[1] = status;
	n = write(sockfd, buffer, 2);
	if (n < 0) {
		perror("socket");
		//exit(EXIT_FAILURE);
	}	
	return;
}


int send_error_code(int sockfd, char error_code) {
	char buffer[2];
	int n;

	memset(buffer, 0, 2);

	buffer[0] = 'e';
	buffer[1] = error_code;
	n = write(sockfd, buffer, 2);
		if (n < 0) {
			perror("socket");
			return -1;
			//exit(EXIT_FAILURE);
		}
				
	return 0;
}


rpc_data* receive_rpc_data(int sockfd) {

	char buffer[MAX_DATA2_BYTES];
	//char* buffer = (char *) malloc(MAX_DATA2_BYTES);
	int n;

	memset(buffer, 0, MAX_DATA2_BYTES);

	if ((n = read(sockfd, buffer, MAX_DATA1_BYTES)) < MAX_DATA1_BYTES) {
		perror("socket");
		return NULL;
	}
	if (n < 0) {
		perror("socket");
		return NULL;
	}
	while (n < MAX_DATA1_BYTES) {
		printf("Haven't received all of data 1 yet - reading more...\n");
		n += read(sockfd, buffer+n, 1);
		printf("Have now received %d bytes in total\n", n);
	}
	int64_t data1 = ntohll(*(int64_t *)&buffer);
	rpc_data* results = malloc(sizeof(rpc_data));
	results->data1 = data1;
	printf("Data 1 received: %ld\n", data1);

	// Read in data2_len
	if ((n = read(sockfd, buffer, 8)) < 8) {
		perror("socket");
		return NULL;
	}
	uint64_t data2_len = ntohll(*(uint64_t *)&buffer);
	printf("Data 2 length received: %lu\n", data2_len);

	// Read in data2 if data2_len != 0
	if (data2_len > 0) {
		memset(buffer, 0, MAX_DATA2_BYTES);
		n = read(sockfd, buffer, data2_len);
		//if ((n = read(sockfd, buffer, MAX_DATA2_BYTES)) < MAX_DATA2_BYTES) {
		//	perror("socket");
		//	return NULL;
		//}
		if (n < 0) {
			perror("socket");
			return NULL;
		}
		//while (n < MAX_DATA2_BYTES) {
		//	printf("Haven't received all of data 2 yet - reading more...\n");
		//	n += read(sockfd, buffer+n, 1);
		//	printf("Have now received %d bytes in total\n", n);
		//}
		results->data2_len = data2_len;
		results->data2 = malloc(data2_len);
		memcpy(results->data2, buffer, data2_len);
		printf("Data 2 received, %d byte(s)\n", n);
		return results;
	}
	// otherwise set data2_len to 0 and data2 to null and return results
	else if (data2_len == 0) {
		results->data2_len = 0;
		results->data2 = NULL;
		return results;
	}
	return NULL;
}


 
int send_rpc_data(int sockfd, rpc_data* payload) {

	int n;

	// Send data 1 to the server
	int64_t data1 = (int64_t) payload->data1;  
	
	int64_t* buffer_cast = (int64_t *) malloc(sizeof(int64_t));
	*buffer_cast = htonll(data1);
	if ((n = write(sockfd, buffer_cast, MAX_DATA1_BYTES)) < MAX_DATA1_BYTES) {
		perror("socket");
		return -1;
	}
	printf("Data 1 sent: %ld \n", ntohll(*buffer_cast));
	free(buffer_cast);

	// Check if data 2 exists and send the prefix indicating such (or data2_len converted to uint64)
	uint64_t data2_len = (uint64_t) payload->data2_len;
	void* data2 = payload->data2;

	// Check validity of data2
	if (data2_len > MAX_DATA2_BYTES) {
		fprintf(stderr, "Overlength error\n");
		return -1;
	}
	else if ((data2_len == 0 && data2 != NULL) || (data2_len != 0 && data2 == NULL)) {
		fprintf(stderr, "Malformed input - data 2 and data2_len don't match\n");
		return -1;
	}
	// Check if data 2 needs to be sent
	else if (data2_len == 0 && data2 == NULL) {

		uint64_t* buffer_cast2 = (uint64_t *) malloc(sizeof(uint64_t));
		*buffer_cast2 = htonll(data2_len);
		if ((n = write(sockfd, buffer_cast2, 8)) < 8) {
			perror("socket");
			return -1;
		}
		free(buffer_cast2);
		printf("No data 2 to send\n");
	}
	// Send data 2 if data2 len > 0
	else {
		//buffer[0] = 'y';
		//if ((n = write(sockfd, buffer, 1)) < 1) {
		//	perror("socket");
		//}

		// Send data2_len to the server  
		printf("Actual data 2 length: %lu \n", data2_len);
		uint64_t* buffer_cast2 = (uint64_t *) malloc(sizeof(uint64_t));
		*buffer_cast2 = htonll(data2_len);
		printf("Data 2 length sent: %lu \n", ntohll(*buffer_cast2));
		if ((n = write(sockfd, buffer_cast2, 8)) < 8) {
			perror("socket");
			return -1;
		}
		
		free(buffer_cast2);

		//if (n < 0) {
		//	perror("socket");
		//	return NULL;
		//}
		//memset(buffer, 0, sizeof(buffer));
		//memcpy(buffer, data2, MAX_DATA2_BYTES);
		n = write(sockfd, data2, data2_len);
		//if ((n = write(sockfd, data2, MAX_DATA2_BYTES)) < MAX_DATA2_BYTES) { // may have to change to while loop
		//	perror("socket");
		//	return NULL;
		//}
		if (n < 0) {
			perror("socket");
			return -1;
		}
		printf("Data 2 sent, %d byte(s)\n", n);
	}
	return 0;
}


// SERVER SIDE

struct rpc_server {
    /* Add variable(s) for server state */ // use primitive data types that don't require free() calls
    int listen_sockfd;
    char port[MAX_PORT_DIGITS+1];
	dict_ptr function_dict;

};

// Called before rpc_register, use for whatever is needed
// Should return a pointer to the rpc_server struct defined by you on success,
// and NULL on failure
rpc_server *rpc_init_server(int port) {

    rpc_server* srv = (rpc_server *) malloc(sizeof(rpc_server));
    // return NULL on failure
	/*
    if (port == NULL)
        fprintf(stderr, "ERROR: No port provided\n");
		free(srv);
        return NULL;
	*/
	if (port > MAX_PORT_NUM || port < 0) {
		fprintf(stderr, "ERROR: Please provide a valid port number between 0 and 65535\n");
		free(srv);
        return NULL;
	}

	//if (argc < 2) {
	//	fprintf(stderr, "ERROR, no port provided\n");
	//	exit(EXIT_FAILURE);
	//}

	/* Store the provided port number in server state as a string*/
    //char port_buffer[MAX_PORT_DIGITS + 1];
	sprintf(srv->port,"%d",port);
    //itoa(port, srv->port, BASE_10);
    printf("Server Provided Port: %s\n", srv->port);
    //strcpy(srv->port, port_buffer);

	/* Create the listening socket */
	if ((srv->listen_sockfd = create_listening_socket(srv->port)) < 0) {
        fprintf(stderr, "ERROR: Failed to create listening socket\n");
		free(srv);
        return NULL;
    }

	srv->function_dict = dict_new();

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

	printf("Now registering function %s\n", name);

	if (srv == NULL || name == NULL || handler == NULL) {
		return -1;
	}

	/* Validate the provided name */

	// Check that name only contains printable ASCII characters between 32 and 126
	for (int i=0; i < strlen(name); i++) {
		if ((int) name[i] < MIN_ASCII_CHAR || (int) name[i] > MAX_ASCII_CHAR) {
			fprintf(stderr, "ERROR: Invalid characters used in name\n");
			return -1;
		}
	}
	// Check that strlen is greater than 0 and less than 1001
	if (strlen(name) < MIN_NAME_LEN || strlen(name) > MAX_NAME_LEN) {
		fprintf(stderr, "ERROR: Name too short or long\n");
		return -1;
	}
	
	/* Name is valid so we can register the function */

	// If there is already a function registered with name, replace the old function with the new one
	// Otherwise, simply regiser the new function under the new name.
	dict_add(srv->function_dict, name, handler);

	printf("Function %s successfully registered\n", name);
	printf("Function pointer for %s: %p\n", name, dict_find(srv->function_dict, name));

	return REG_SUCCESS;
}


// Waits for incoming requests for any of the registered functions, or rpc_find, on the
// port specified in rpc_init_server of any interface. If it is a function call request, it will
// call the requested function, send a reply to the caller, and resume waiting for new
// requests. If it is rpc_find, it will reply to the caller saying whether the name was found or not,
// or possibly an error code
void rpc_serve_all(rpc_server *srv) {


	printf("Now serving client requests...\n");

    if (srv == NULL) {
        return;
    }

    //char* port = srv->port;
	int n;
    int sockfd = srv->listen_sockfd;
    int newsockfd;
	char buffer[MAX_REQUEST_BYTES];//, ip[INET6_ADDRSTRLEN];
	struct sockaddr_in client_addr;
	socklen_t client_addr_size;

	char func_name[MAX_NAME_LEN + 1];

	/*
	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
	client_addr_size = sizeof client_addr;
	newsockfd =
		accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
	if (newsockfd < 0) {
		perror("accept");
		//exit(EXIT_FAILURE);
	}
	*/
	
	// Read characters from the connection, then process
	while (1) {


		// Listen on socket - means we're ready to accept connections,
		// incoming connection requests will be queued, man 3 listen


		printf("Listening for connection...\n");
		// maybe change to while loop so that listen() is repeatedly called until success or
		// just remove exit()
		if (listen(sockfd, 10) < 0) {
			perror("listen");
			//exit(EXIT_FAILURE);
		}

		// MAY HAVE TO ACCEPT CONNECTION EVERY TIME A REQUEST IS MADE OR USE SELECT-SERVER CODE
		// TO MANAGE MULTIPLE CLIENT CONNECTIONS
		
		printf("Waiting until connection is accepted...\n");
		// Accept a connection - blocks until a connection is ready to be accepted
		// Get back a new file descriptor to communicate on
		client_addr_size = sizeof client_addr;
		newsockfd =
			accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
		if (newsockfd < 0) {
			perror("accept");
			//exit(EXIT_FAILURE);
		}
		
		memset(buffer, 0, sizeof(buffer));
		while ((n = read(newsockfd, buffer, 1)) > 0) {
			// n is number of characters read / written
			
			// move memset to outside while loop if it causes program to be slow (in which case will
			// just have to be careful not to read wrong sections of buffer into memory)
			


			printf("Message type prefix: %c\n", buffer[0]);

			/* -------------------- */
			/* Ignore test messages */
			/* -------------------  */
			if (buffer[0] == 't') {
				continue;
			}
			/* -------------------- */
			/* Handle find requests */
			/* -------------------  */

			if (buffer[0] == 'f') {
				
				// read in function length prefix and function name
				if (receive_function(newsockfd, func_name) != 0) {
					perror("socket");
					send_error_code(newsockfd, ERRCODE_SOCKET);
					continue;
				}

                printf("Function name before dictionary search: %s\n", func_name);
				if (dict_find(srv->function_dict, func_name) != NULL) {
					// let the caller know that the function was found
					printf("Function found by server\n");
					send_find_response(newsockfd, 's');
					//sleep(2);
					continue;
				}
				else {
					// let the caller know that the function was not found
					printf("Function not found by server\n");
					send_find_response(newsockfd, 'f');
					//sleep(2);
					continue;
				}
			}


			/* -------------------- */
			/* Handle call requests */
			/* -------------------- */

			else if (buffer[0] == 'c') {

				if (receive_function(newsockfd, func_name) != 0) {
					perror("socket");
					send_error_code(newsockfd, ERRCODE_SOCKET);
					continue;
				}

				rpc_handler procedure = (rpc_handler) dict_find(srv->function_dict, func_name);
				if (procedure != NULL) {
					rpc_data* in = NULL;
					
					if ((in = receive_rpc_data(newsockfd)) == NULL) {
						printf("Failed to receive RPC data\n");
						continue;
					}

					printf("Attempting to call the function with address: %p\n", procedure);

					// Call the procedure
					rpc_data* out = procedure(in);
					printf("Returned from function call\n");
					rpc_data_free(in);

					// Check that NULL wasn't returned
					if (out == NULL) {
						fprintf(stderr, "NULL response received from procedure at server\n");
						send_error_code(newsockfd, ERRCODE_NULL_RESULT);
						continue;
					}
					else {
						// Check validity of data2 returned by procedure
						if (out->data2_len > MAX_DATA2_BYTES) {
							fprintf(stderr, "Overlength error\n");
							send_error_code(newsockfd, ERRCODE_DATA2_LEN);
							continue;
						}
						else if ((out->data2_len == 0 && out->data2 != NULL) || (out->data2_len != 0 && out->data2 == NULL)) {
							fprintf(stderr, "Malformed input - data 2 and data2_len don't match at server\n");
							send_error_code(newsockfd, ERRCODE_DATA2_MALFORMED);
							continue;
						}	

						/* Send the results back to the client */

						// Send response prefix
						buffer[0] = 'r';
						n = write(newsockfd, buffer, 1);
						if (n < 0) {
							perror("socket");
							//exit(EXIT_FAILURE);
						}
						send_rpc_data(newsockfd, out);
						rpc_data_free(out);
						//sleep(2);

						continue;
					}	
				}
				else {
					// let the client know that the function was not found
					send_error_code(newsockfd, ERRCODE_PROC_NOTFOUND);
					continue;
				}
			}

			// Invalid prefix
			else {
				printf("Invalid request received by server: %c\n", buffer[0]);
				send_error_code(newsockfd, ERRCODE_MSG_PREFIX);
				continue;
			}
		}
		printf("Connection closed...\n");

	}

	// close listening socket 
	//close(sockfd);

    // Note that this function will not usually return (i.e. will not reach this line)
    return;
}



//----------------------------------------------------------------------------------------------





// CLIENT SIDE

// use primitive data types that don't require free() calls
struct rpc_client {
    /* Add variable(s) for client state */
    int sockfd;
    char port[MAX_PORT_DIGITS+1];

};

// use primitive data types that don't require free() calls
struct rpc_handle { // handle into rpc like a file handle that lets the client
                    // invoke a function. Returned by rpc_find and then used by rpc_call
    /* Add variable(s) for handle */
	char name[MAX_NAME_LEN + 1];
	rpc_handler handler;

};

// Called before rpc_find or rpc_call. Use this for whatever you need.
// The string addr and integer port are the text-based IP address and numeric
// port number passed in on the command line
// This function should return a non-NULL pointer to a rpc_client struct containing
// client state information on success and NULL on failure
rpc_client *rpc_init_client(char *addr, int port) {
    
    // Malloc client state
    rpc_client* cl = (rpc_client *) malloc(sizeof(rpc_client));

	// return NULL on failure
	/*
    if (port == NULL)
        fprintf(stderr, "ERROR: No port provided\n");
		free(cl);
        return NULL;
	*/
	if (port > MAX_PORT_NUM || port < 0) {
		fprintf(stderr, "ERROR: Please provide a valid port number between 0 and 65535\n");
		free(cl);
        return NULL;
	}

	sprintf(cl->port,"%d",port);
    printf("Client Provided Port: %s\n", cl->port);

    int s;
	struct addrinfo hints, *servinfo, *rp;

	// Create address
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	// Get addrinfo of server. From man page:
	// The getaddrinfo() function combines the functionality provided by the
	// gethostbyname(3) and getservbyname(3) functions into a single interface
    
    //char port_buffer[MAX_PORT_DIGITS + 1];
    //itoa(port, cl->port, BASE_10);
	
    //strcpy(cl->port, port_buffer);

	s = getaddrinfo(addr, cl->port, &hints, &servinfo);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		free(cl);
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
		free(cl);
		return NULL;
	}

	freeaddrinfo(servinfo);

    return cl;
}


// At the client, tell the subsystem what details are required to place a call. The return value is a handle
// struct (not handler) for the remote procedure, which is passed to the rpc_call function.
// If name is not registered, it should return NULL. If the find operation fails, it returns NULL.
rpc_handle *rpc_find(rpc_client *cl, char *name) {

	printf("Attempting to find function: %s\n", name);

    char buffer[MAX_REQUEST_BYTES];
    int n;

	int sockfd = cl->sockfd;

	// If any of the arguments are NULL then NULL should be returned
	if (cl == NULL || name == NULL) {
		return NULL;
	}

	/* Send find request */

	// Send the prefix to indicate that this is a find request
	buffer[0] = 'f';
	if ((n = write(sockfd, buffer, 1)) < 1) {
		perror("socket");
		return NULL;
	}
	
	// Send the function name length and name to the server
	if (send_function(sockfd, name) < 0 ) {
		return NULL;
	}

	// Receive response from server
    n = read(sockfd, buffer, 2);
	while (n < 2) {
		printf("Haven't received full find response yet - reading more...\n");
		n += read(sockfd, buffer+n, 1);
		printf("Have now received %d bytes in total\n", n);
	}
	// Return NULL if an error occurred or no function matched the name
	// Could technically have different if statements for different error codes here to
	// print descriptive error messages (although there is only one error in the case of find i think)
	if (buffer[0] == 'e' || buffer[1] == 'f') {
		printf("Find error or no function matched the name\n");
		return NULL;
	}
    else if (buffer[0] == 'r' && buffer[1] == 's') {
		rpc_handle* handle = malloc(sizeof(rpc_handle));
		strcpy(handle->name, name);
		return handle;
	}
	else {
		fprintf(stderr, "Invalid find response received by client from server\n");
		return NULL;
	}

	return NULL;
}

// This function causes the subsystem to run the remote procedure, and returns the value.
// If the call fails, it returns NULL. NULL should be returned if any of the arguments are NULL. If
// this returns a non-NULL value, then it should dynamically allocate (by malloc) both the rpc_data structure
// and its data2 field. The client will free these by rpc_data_free (defined below)
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

	printf("Attempting to call function: %s\n", h->name);
	// SHOULD CHECK THAT THE CLIENT IS NOT SENDING AN INVALID RPC_DATA

	// SERVER SHOULD CHECK THAT THE HANDLER ISN'T RETURNING AN INVALID RPC_DATA
	
    char buffer[MAX_DATA2_BYTES];
	memset(buffer, 0, sizeof(buffer));
    int n;

	int sockfd = cl->sockfd;

	// If any of the arguments are NULL then NULL should be returned
	if (cl == NULL || h == NULL || payload == NULL || h->name == NULL) {
		return NULL;
	}

	// Send the prefix to indicate that this is a call request
	buffer[0] = 'c';
	if ((n = write(sockfd, buffer, 1)) < 1) {
		perror("socket");
		return NULL;
	}
	// Send function name length and function name to server
	send_function(sockfd, h->name);
	
	if (send_rpc_data(sockfd, payload) < 0) { 
		return NULL;
	}

	// Receive response from server 
	if ((n = read(sockfd, buffer, 1)) < 1) {
		perror("socket");
		return NULL;
	}
	//if (n < 0) {
	//	perror("socket");
	//	return NULL;
	//}
	if (buffer[0] == 'r') {
		return receive_rpc_data(sockfd);
	}
	else if (buffer[0] == 'e') {
		fprintf(stderr, "Error received from server\n");
		return NULL;
	}
	else {
		fprintf(stderr, "Invalid response prefix from server\n");
		return NULL;
	}
		
	return NULL;
}

// Called after the final rpc_call or rpc_find (i.e. final use of RPC system by client)
// Use for whatever is needed; it should at least free(cl)
// If it is (mistakenly) called on a client that has already been closed, or cl == NULL, it 
// should return without error. (Think: How can you tell if it has already been closed)
void rpc_close_client(rpc_client *cl) {
    // If client hasn't yet been closed, close it
	//char buffer[1];
	int n;

	// If client hasn't yet been closed, close it and free cl
	if (cl != NULL && (n = write(cl->sockfd, "t", 1)) != -1) {
		printf("cl != NULL, n = %d, freeing, closing connection and returning...\n", n);
		close(cl->sockfd);
		free(cl);
    	cl = NULL;
		return;
	}
	// If cl hasn't been freed yet, free it
    else if (cl != NULL) { 
		printf("cl != NULL, freeing and returning...\n");	
        free(cl);
    	cl = NULL;
		return;
    }
    // cl == NULL so simply return
	else {
		printf("cl == NULL, returning...\n");	
    	return;
	}
   
	
}





// SHARED FUNCTION

// Frees the memory allocated for a dynamically allocated rpc_data struct
void rpc_data_free(rpc_data *data) {
	printf("rpc_data_free called...\n");
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
	hints.ai_family = AF_INET6;       // IPv6
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