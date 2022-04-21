/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef __TZ_TEEI_H__
#define __TZ_TEEI_H__

#define socket_buffer_size 2048
#define ARGS_BLOCK_SIZE 1024

struct sockaddr {
	unsigned short sa_family;
	char sa_data[14];
};

struct in_addr {
	unsigned long s_addr; /* that’s a 32-bit long, or 4 bytes */
};

struct sockaddr_in {
	short int sin_family;
	unsigned short int sin_port;
	struct in_addr sin_addr;
	unsigned char sin_zero[8];
};

struct TEEI_socket_command {
	int func;
	int cmd_size;

	union func_arg {
		char raw[ARGS_BLOCK_SIZE];

		struct func_socket {
			int af;
			int type;
			int protocol;
		} func_socket_args;

		struct func_connect {
			int sockfd;
			struct sockaddr ob_addr;
			int addrlen;
		} func_connect_args;

		struct func_bind {
			int sockfd;
			struct sockaddr ob_addr;
			int addrlen;
		} func_bind_args;

		struct func_listen {
			int sockfd;
			int backlog;
		} func_listen_args;

		struct func_accept {
			int sockfd;
			struct sockaddr ob_addr;
			int addrlen;
		} func_accept_args;

		struct func_send {
			int sockfd;
			void *buf;
			int len;
			int flags;
		} func_send_args;

#define func_recv func_send
#define func_recv_args func_send_args
#define func_recv_send func_send
#define func_recv_send_args func_send_args
		struct func_close {
			int sockfd;
		} func_close_args;

		struct func_inet_addr {
			char ip_addr[17];
		} func_inet_addr_args;

		struct func_htons {
			unsigned short portnum;
		} func_htons_args;
		struct func_setsockopt {
			int fd;
			int level;
			int optname;
			struct timeval;
			int optlen;
		} func_setsockopt_args;

	} args;

};


union TEEI_socket_response_type {
	int value;
	uint32_t addr;
	bool hasError;
	unsigned short portnum;
	unsigned int transSize;
	struct response_func_recv {
		void *buf;
		unsigned int size;
	} recv;
};

#endif /* __TZ_TEEI_H_ */
