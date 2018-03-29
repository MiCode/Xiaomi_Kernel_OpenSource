#ifndef __TZ_TEEI_H__
#define __TZ_TEEI_H__

#define socket_buffer_size 2048
#define ARGS_BLOCK_SIZE 1024

struct sockaddr {
	unsigned short sa_family; /* 地址族 */
	char sa_data[14];   /* 14字节协议地址 */
};

struct in_addr {
	unsigned long s_addr; /* that’s a 32-bit long, or 4 bytes */
};

struct sockaddr_in {
	short int sin_family;   /* 地址族 */
	unsigned short int sin_port;   /* 端口号 */
	struct in_addr sin_addr;   /* IP地址 */
	unsigned char sin_zero[8];   /* 填充0以保持与struct sockaddr同样大小 */
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


#if 0
#define TEEI_pointer_message (void *)

#define TEEI_CREATE_MESSAGE_FIALED (-1)

#define MESSAGE_HEADER(msg) ((struct TEEI_message_header *)msg)
#define MESSAGE_PAYLOAD(msg) (void *)(msg + sizeof(TEEI_message_header))

/**
 * get size of a message.
 */
inline int TEEI_sizeof_message(TEEI_pointer_message pMessage)
{
	TEEI_message_header *msg = (TEEI_message_header *) pMessage;
	return msg->payload_size + sizeof(TEEI_message_header);
}

/*
 * Create a memory block as a message.
 * payload_size: is the payload size.
 * Whole message size = sizeof(TEEI_message_header) + payload_size
 *
 * return message pointer. DO NOT FORGET freeing this block!!!
 *        TEEI_CREATE_MESSAGE_FIALED if malloc failed.
 */
TEEI_pointer_message create_TEEI_message_with_payload_size(unsigned long payload_size);

/**
 * create a context type message with context name;
 */
TEEI_pointer_message create_TEEI_message_context_init(const char *TEE_name, int TEE_name_size);

#endif
#endif /* __TZ_TEEI_H_ */
