#ifndef T_UTILS_H
#define T_UTILS_H

#define MIN_NAME_LEN 1
#define MAX_NAME_LEN 1000
#define NAME_LEN_PREFIX 2
#define ERRCODE_NAME_LEN 11
#define MAX_DATA2_BYTES 100000
#define DATA1_SIZE 8
#define DATA2_LEN_SIZE 8

int send_error_code(int sockfd, char error_code);

int receive_function(int sockfd, char func_name[MAX_NAME_LEN + 1]);

int send_function(int sockfd, char func_name[MAX_NAME_LEN + 1]);

rpc_data* receive_rpc_data(int sockfd);

int send_rpc_data(int sockfd, rpc_data* payload);

int send_find_response(int sockfd, const char status);

int read_nbytes(int sockfd, void* buffer, int n);

int write_nbytes(int sockfd, void* buffer, int n);

#endif