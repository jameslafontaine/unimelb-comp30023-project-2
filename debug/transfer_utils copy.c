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

#define htonll htobe64
#define ntohll be64toh



/* Reads n bytes from the provided descriptor into the provided buffer, returns -1
   if unsuccessful in reading n bytes, returns 0 if successful
*/
int read_nbytes(int sockfd, void* buffer, int n) {
	int amount, more;
	while ((amount = read(sockfd, buffer, n)) < n) {
		more = read(sockfd, buffer+amount, 1);
		if (more < 1) {
			perror("socket");
			return -1;
		}
		amount += more;
	}
	return 0;
}

/* Writes n bytes to the provided descriptor from the provided buffer, returns -1
   if unsuccessful in writing n bytes, returns 0 if successful
*/
int write_nbytes(int sockfd, void* buffer, int n) {
	int amount, more;
	while ((amount = write(sockfd, buffer, n)) < n) {
		more = write(sockfd, buffer+amount, 1);
		if (more < 1) {
			return -1;
		}
		amount += more;
	}
	return 0;
}

/* Receive function name being sent across TCP connection with defined format
*/
int receive_function(int sockfd, char func_name[MAX_NAME_LEN + 1]) {
	char buffer[MAX_NAME_LEN+1];

	memset(buffer, 0, MAX_NAME_LEN+1);
	// read in name length prefix
	if (read_nbytes(sockfd, buffer, NAME_LEN_PREFIX)) {
		return -1;
	}

	uint16_t func_name_len = ntohs(*(uint16_t*)&buffer);
	// double check function name length conforms to bounds
	if (func_name_len < MIN_NAME_LEN || func_name_len > MAX_NAME_LEN) {
		fprintf(stderr, "Please provide a function name between 1 and 1000 characters in length\n");
		// reply with error code
		send_error_code(sockfd, ERRCODE_NAME_LEN);
		return -1;
	}
	// read in the actual function name + '\0'
	if (read_nbytes(sockfd, buffer, func_name_len)) {
		return -1;
	}

	snprintf(func_name, func_name_len + 1, "%s", buffer);

	return 0;
}

/* Send function name across TCP connection with defined format
*/
int send_function(int sockfd, char func_name[MAX_NAME_LEN + 1]) {
	char buffer[MAX_NAME_LEN+1];

	memset(buffer, 0, MAX_NAME_LEN+1);

	// Send the length of the provided name
	uint16_t name_len = (uint16_t) strlen(func_name);
	if (name_len < MIN_NAME_LEN || name_len > MAX_NAME_LEN) {
		fprintf(stderr, "Invalid name length - please provide a name between 1 and 1000 characters long\n");
		return -1;
	}
	uint16_t* buffer_cast = (uint16_t*) malloc(sizeof(uint16_t));
	*buffer_cast = htons(name_len);
	if (write_nbytes(sockfd, buffer_cast, 2)) {
		return -1;
	}
	free(buffer_cast);

	// Send the actual name to the server
	snprintf(buffer, name_len + 1, "%s", func_name);
	if (write_nbytes(sockfd, buffer, name_len)) {
		return -1;
	}

	return 0;
}

/* Send response to client indicating the outcome of the find request
*/
int send_find_response(int sockfd, const char status) {
	char buffer[2];

	memset(buffer, 0, 2);

	buffer[0] = 'r';
	buffer[1] = status;
	if (write_nbytes(sockfd, buffer, 2)) {
		return -1;
	}

	return 0;
}

/* Send error message with error code across TCP connection 
*/
int send_error_code(int sockfd, char error_code) {
	char buffer[2];

	memset(buffer, 0, 2);

	buffer[0] = 'e';
	buffer[1] = error_code;
	if (write_nbytes(sockfd, buffer, 2)) {
		return -1;
	}
				
	return 0;
}

/* Receive RPC data being sent across TCP connection with defined format
*/
rpc_data* receive_rpc_data(int sockfd) {

	char buffer[MAX_DATA2_BYTES];
	//char* buffer = (char *) malloc(MAX_DATA2_BYTES);

	memset(buffer, 0, MAX_DATA2_BYTES);

	//printf("Reading in data 1...\n");
	if (read_nbytes(sockfd, buffer, DATA1_SIZE)) {
		return NULL;
	}
	
	int64_t data1 = ntohll(*(int64_t *)&buffer);
	rpc_data* payload = malloc(sizeof(rpc_data));
	payload->data1 = data1;

	//printf("Reading in data 2 length...\n");

	// Check if data 2 was malformed
	if (read_nbytes(sockfd, buffer, 1)) {
		return NULL;
	}

	if (buffer[0] == 'm') {
		fprintf(stderr, "Malformed input indicated by sender\n");
		return NULL;
	} 
	else if (buffer[0] != 'v') {
		fprintf(stderr, "Invalid malformity indicator received\n");
		return NULL;
	}

	// Read in data2_len
	if (read_nbytes(sockfd, buffer, DATA2_LEN_SIZE)) {
		return NULL;
	}

	uint64_t data2_len = ntohll(*(uint64_t *)&buffer);
	//printf("Data 2 length received: %ld\n", data2_len);

	// Read in data2 if data2_len > 0
	if (data2_len > 0) {
		//printf("Reading in data 2...\n");
		memset(buffer, 0, MAX_DATA2_BYTES);
		if (read_nbytes(sockfd, buffer, data2_len)) {
			return NULL;
		}

		payload->data2_len = data2_len;
		payload->data2 = malloc(data2_len);
		memcpy(payload->data2, buffer, data2_len);
		return payload;
	}
	// otherwise set data2_len to 0 and data2 to null and return payload
	else if (data2_len == 0) {
		payload->data2_len = 0;
		payload->data2 = NULL;
		return payload;
	}
	return NULL;
}


 /* Send RPC data across TCP connection with defined format
*/
int send_rpc_data(int sockfd, rpc_data* payload) {

	char buffer[MAX_DATA2_BYTES];

	// Check if payload is null
	if (payload == NULL) {
		return -1;
	}

	//printf("Sending data 1...\n");

	// Send data 1 to the server
	int64_t data1 = (int64_t) payload->data1;  
	
	int64_t* buffer_cast = (int64_t *) malloc(sizeof(int64_t));
	*buffer_cast = htonll(data1);
	if (write_nbytes(sockfd, buffer_cast, DATA1_SIZE)) {
		return -1;
	}
	free(buffer_cast);

	// Check if data 2 exists and send the data 2 length as an uint64
	uint64_t data2_len = (uint64_t) payload->data2_len;
	void* data2 = payload->data2;

	//printf("Checking data 2 validity...\n");

	// Check validity of data2
	if (data2_len > MAX_DATA2_BYTES) {
		fprintf(stderr, "Overlength error\n");
		buffer[0] = 'm';
		write_nbytes(sockfd, buffer, 1);
		return -1;
	}
	else if ((data2_len == 0 && data2 != NULL) || (data2_len != 0 && data2 == NULL)) {
		fprintf(stderr, "Malformed input - data 2 and data2_len don't match\n");
		buffer[0] = 'm';
		write_nbytes(sockfd, buffer, 1);
		return -1;
	}

	buffer[0] = 'v';
	if (write_nbytes(sockfd, buffer, 1)) {
		return -1;
	}

	// Send data 2 length

	//printf("Sending data 2 length...\n");

	uint64_t* buffer_cast2 = (uint64_t *) malloc(sizeof(uint64_t));
	*buffer_cast2 = htonll(data2_len);
	if (write_nbytes(sockfd, buffer_cast2, DATA2_LEN_SIZE)) {
		return -1;
	}
	free(buffer_cast2);

	// Send data 2 if data2 len > 0
	if (data2_len > 0) {

		//printf("Sending data 2...\n");

		// Send data 2
		if (write_nbytes(sockfd, data2, data2_len)) {
			return -1;
		}
	}
	return 0;
}

