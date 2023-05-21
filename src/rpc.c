#define POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 600 
#define _DEFAULT_SOURCE         

#include "rpc.h"
#include "dict.h"
#include "transfer_utils.h"
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <endian.h>
#include <pthread.h>

#define MAX_PORT_DIGITS 5
#define MAX_PORT_NUM 65535
#define MIN_ASCII_CHAR 32
#define MAX_ASCII_CHAR 126
#define MAX_REQUEST_BYTES 102000

#define REG_SUCCESS 21

#define ERROR_MSG_SIZE 2

#define ERRCODE_PROC_NOTFOUND 12
#define ERRCODE_DATA2_PREFIX 13
#define ERRCODE_DATA2_LEN 14
#define ERRCODE_DATA2_MALFORMED 15
#define ERRCODE_READ_WRITE 16
#define ERRCODE_NULL_RESULT 17
#define ERRCODE_SOCKET 18
#define ERRCODE_MSG_PREFIX 19

#define NONBLOCKING

typedef struct {
    rpc_server* srv;
    int newsockfd;
    pthread_t pid;

} thread_args;

int create_listening_socket(char* service);

void* serve_client(void* args);

// SERVER SIDE

/* Stores information about server state 
*/
struct rpc_server {
    int listen_sockfd;
    char port[MAX_PORT_DIGITS+1];
	dict_ptr function_dict;

};

/* Creates a listening socket for the server to listen for incoming connections on
*/
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

// Serves all client requests of a single client until the connection is closed
void* serve_client(void* args) {
    int n;
	char buffer[MAX_REQUEST_BYTES];
    char func_name[MAX_NAME_LEN + 1];
    memset(buffer, 0, sizeof(buffer));

    int newsockfd = ((thread_args *)args)->newsockfd;
    rpc_server* srv = ((thread_args *)args)->srv;

    while ((n = read(newsockfd, buffer, 1)) > 0) {

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
            if (receive_function(newsockfd, func_name)) {
                send_error_code(newsockfd, ERRCODE_SOCKET);
                continue;
            }

            if (dict_find(srv->function_dict, func_name) != NULL) {
                // let the caller know that the function was found
                send_find_response(newsockfd, 's');
                //sleep(2);
                continue;
            }
            else {
                // let the caller know that the function was not found
                send_find_response(newsockfd, 'f');
                //sleep(2);
                continue;
            }
        }


        /* -------------------- */
        /* Handle call requests */
        /* -------------------- */

        else if (buffer[0] == 'c') {
			
			// Receive function name via handle
            if (receive_function(newsockfd, func_name)) {
                send_error_code(newsockfd, ERRCODE_SOCKET);
                continue;
            }

			// Receive data if the provided function is registered
            rpc_handler procedure = (rpc_handler) dict_find(srv->function_dict, func_name);
            if (procedure != NULL) {
                rpc_data* in = NULL;
                
                if ((in = receive_rpc_data(newsockfd)) == NULL) {
                    continue;
                }

                // Call the procedure
                rpc_data* out = procedure(in);
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
					if (write_nbytes(newsockfd, buffer, 1)) {
						continue;
					}
                    send_rpc_data(newsockfd, out);
                    rpc_data_free(out);
                    //sleep(2);

                    continue;
                }	
            }
            else {
                // Let the client know that the function was not found
                send_error_code(newsockfd, ERRCODE_PROC_NOTFOUND);
                continue;
            }
        }

        // Invalid prefix
        else {
            fprintf(stderr, "Invalid request received by server: %c\n", buffer[0]);
            send_error_code(newsockfd, ERRCODE_MSG_PREFIX);
            continue;
        }
    }
    pthread_detach(((thread_args *)args)->pid);
    free(args);

    return NULL;
}


/* Called before rpc_register, initalises the server state
*/
rpc_server *rpc_init_server(int port) {

    rpc_server* srv = (rpc_server *) malloc(sizeof(rpc_server));
    // Check if port is valid
	if (port > MAX_PORT_NUM || port < 0) {
		fprintf(stderr, "ERROR: Please provide a valid port number between 0 and 65535\n");
		free(srv);
        return NULL;
	}

	// Store the provided port number in server state as a string*/
	sprintf(srv->port,"%d",port);

	// Create the listening socket 
	if ((srv->listen_sockfd = create_listening_socket(srv->port)) < 0) {
        fprintf(stderr, "ERROR: Failed to create listening socket\n");
		free(srv);
        return NULL;
    }

	// Initialise function dictionary
	srv->function_dict = dict_new();

    return srv;
}


/* Register a server function uniquely identified by its name, 
   and callable by a provided function pointer
*/
int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {

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

	/* If there is already a function registered with name, replace the old function with the new one
	   Otherwise, simply regiser the new function under the new name.
	*/

	// dict_add should automatically handle function replacement 
	dict_add(srv->function_dict, name, handler);

	return REG_SUCCESS;
}


/* Waits for incoming requests for any of the registered functions, or rpc_find, on the
   port specified in rpc_init_server of any interface. If it is a function call request, it will
   call the requested function, send a reply to the caller, and resume waiting for new
   requests. If it is rpc_find, it will reply to the caller saying whether the name was found or not,
   or possibly an error code
*/
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) {
        return;
    }

    int newsockfd;
    int sockfd = srv->listen_sockfd;

	struct sockaddr_in client_addr;
	socklen_t client_addr_size;
	
	/* Indefinitely serve clients */
	while (1) {
        
        //printf("Listening for connections...\n");

		/* Listen on socket - means we're ready to accept connections,
           incoming connection requests will be queued, man 3 listen
		*/
		if (listen(sockfd, 10) < 0) {
			perror("listen");
			exit(EXIT_FAILURE);
		}

		
		
        //printf("Accepting connection...\n");

		/* Accept a connection - blocks until a connection is ready to be accepted
		   Get back a new file descriptor to communicate on
		*/
		client_addr_size = sizeof client_addr;
		newsockfd =
			accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
		if (newsockfd < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}
        

        thread_args* args = (thread_args *) malloc(sizeof(thread_args));
        args->srv = srv;
        args->newsockfd = newsockfd;

        #ifdef NONBLOCKING
		/* Spawn a new thread when a connection is accepted */
        if (pthread_create(&(args->pid), NULL, serve_client, (void *) args) != 0) {
            perror("pthread_create() error");   // pthread_create() returns 1 if error creating thread
            exit(1);
        }

        //if (pthread_join(args->pid, NULL)) {
		//    printf("Error joining thread\n");  // pthread_join() returns 1 if error joining thread
		//    exit(1);
	    //}
        #endif

        #ifndef NONBLOCKING
        serve_client((void *) args);
        #endif
	}
	// close listening socket 
	//close(sockfd);

    // Note that this function will not usually return (i.e. will not reach this line)
    return;
}




//----------------------------------------------------------------------------------------------





// CLIENT SIDE

/* Stores information about client state 
*/
struct rpc_client {
    int sockfd;
    char port[MAX_PORT_DIGITS+1];

};

/* Handle which lets a client invoke a function 
*/
struct rpc_handle { // handle into rpc like a file handle that lets the client
                    // invoke a function.
	char name[MAX_NAME_LEN + 1];
	rpc_handler handler;

};

/* Initialise client state
*/
rpc_client *rpc_init_client(char *addr, int port) {
    
    // Malloc client state
    rpc_client* cl = (rpc_client *) malloc(sizeof(rpc_client));

	// Check port validity
	if (port > MAX_PORT_NUM || port < 0) {
		fprintf(stderr, "ERROR: Please provide a valid port number between 0 and 65535\n");
		free(cl);
        return NULL;
	}

	sprintf(cl->port,"%d",port);

    int s;
	struct addrinfo hints, *servinfo, *rp;

	// Create address
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;

	// Get addrinfo of server
	s = getaddrinfo(addr, cl->port, &hints, &servinfo);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		free(cl);
		return NULL;
	}

	// Connect to first valid result
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


/* Checks if a function is registered on the server and provides a handle to call that function with
*/
rpc_handle *rpc_find(rpc_client *cl, char *name) {

    char buffer[MAX_REQUEST_BYTES];

	int sockfd = cl->sockfd;

	// If any of the arguments are NULL then NULL should be returned
	if (cl == NULL || name == NULL) {
		return NULL;
	}

	/* Send find request */

	// Send the prefix to indicate that this is a find request
	buffer[0] = 'f';
	if (write_nbytes(sockfd, buffer, 1)) {
		return NULL;
	}
	// Send the function name length and name to the server
	if (send_function(sockfd, name)) {
		return NULL;
	}

	// Receive response from server
	if (read_nbytes(sockfd, buffer, 2)) {
		return NULL;
	}
    
	// Return NULL if an error occurred or no function matched the name
	if (buffer[0] == 'e' || buffer[1] == 'f') {
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

/* Request the subsystem to run the remote procedure, and return the value
*/
rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
	
    char buffer[MAX_DATA2_BYTES];
	memset(buffer, 0, sizeof(buffer));

	int sockfd = cl->sockfd;

	// If any of the arguments are NULL then NULL should be returned
	if (cl == NULL || h == NULL || payload == NULL || h->name == NULL) {
		return NULL;
	}

	// Send the prefix to indicate that this is a call request
	buffer[0] = 'c';
	if (write_nbytes(sockfd, buffer, 1)) {
		return NULL;
	}

	// Send function name length and function name to server
	if (send_function(sockfd, h->name)) {
        return NULL;
	}
	
	// Send RPC data to the server
	if (send_rpc_data(sockfd, payload)) { 
		return NULL;
	}

	// Receive response from server 
	if (read_nbytes(sockfd, buffer, 1)) {
		return NULL;
	}

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

/* Called after client has finished using RPC system, closes connection and frees client state 
   if this hasn't already been done 
*/
void rpc_close_client(rpc_client *cl) {

	int n;

	// If client hasn't yet been closed, close it and free cl
	if (cl != NULL && (n = write(cl->sockfd, "t", 1)) != -1) {
		close(cl->sockfd);
		free(cl);
    	cl = NULL;
		return;
	}
	// If cl hasn't been freed yet, free it
    else if (cl != NULL) { 
        free(cl);
    	cl = NULL;
		return;
    }
    // cl == NULL so simply return
	else {
    	return;
	}
   
	
}





// SHARED FUNCTION

/* Frees the memory allocated for a dynamically allocated rpc_data struct
*/
void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
