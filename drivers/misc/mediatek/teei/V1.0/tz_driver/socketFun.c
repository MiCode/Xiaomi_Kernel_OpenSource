#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/thread_info.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/audit.h>
#include <linux/security.h>
#include <asm/current.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/inet_common.h>
#include <linux/uaccess.h>
#include <asm/socket.h>

#include "SOCK.h"
MODULE_LICENSE("Dual BSD/GPL");

enum {
	TZ_SOCKET = 0x0001,
	TZ_CONNECT,
	TZ_SEND,
	TZ_RECV,
	TZ_CLOSE,
	TZ_BIND,
	TZ_LISTEN,
	TZ_ACCEPT,
	TZ_INET_ADDR,
	TZ_HTONS,
	TZ_SETSOCKOPT,
};

#define ARGS_BLOCK_SIZE 1024
#define TRUST_ZONE 0x0001

#define NT_SMC_SWITCH                   0 /* switch to T */

#define NT_SMC_SWITCH_FIRST     0 /* first switch to T */
#define NT_SMC_SWITCH_SECOND    1 /* for second time*/
#define Asm (__asm__ volatile)

#define SOCKET_BASE 0xFE021000

enum {
	FUCTION_socket =                0x0,
	FUCTION_connect =               0x04,
	FUCTION_send  =                 0x08,
	FUCTION_recv =                  0x0C,
	FUCTION_close =                 0x20,
	FUNCTION_setsockopt =           0x24,
	SET_BUFFER_BASE             =   0xF0,
	SET_PARAM_BASE             =    0xF4,

};



struct socket_params_t {
	int domain;
	int type;
	int protocol;
	long returns;
} __attribute__((__packed__));


struct connect_params_t {
	int sockfd;
	struct sockaddr addr;
	int addrlen;
	long returns;
} __attribute__((__packed__));

struct send_params_t {
	int sockfd;
	size_t len;
	int flags;
	long returns;
} __attribute__((__packed__));

struct recv_params_t {
	int sockfd;
	size_t len;
	int flags;
	long returns;
} __attribute__((__packed__));


struct close_params_t {
	int sockfd;
	long returns;
} __attribute__((__packed__));

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
			int sockfd;
			int level;
			int optname;
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


/******************************************************************
 * @brief:
 *      smc call()
 * @param:
 *       p1 - param 1   cmd type
 *               p2 - param 2   first or second switch
 *       p3 - param 3   transfer data address (PA)
 *       p4 - param 4   extern value
 * @return:
 * *****************************************************************/
static void smc_call(uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)
{
	pr_info("********* go to secure world!\n");

	Asm("mov r0, %0\n\t"
	    "mov r1, %1\n\t"
	    "mov r2, %2\n\t"
	    "mov r3, %3\n\t"
	    "smc 0\n\t"
	    : /*no output*/
	    : "r" (p1), "r" (p2), "r" (p3), "r" (p4)
	    : "r0", "r1", "r2", "r3", "memory");

	pr_info("********** back form secure world !\n");

}



int tz_inet_aton(char *cp, struct in_addr *addr)
{
	register u_long val;
	register int base, n;
	register char c;
	u_int parts[4];
	register u_int *pp = parts;

	c = *cp;

	for (;;) {
		/*
		 * Collect number up to ``.''.
		 * Values are specified as for C:
		 * 0x=hex, 0=octal, isdigit=decimal.
		 */
		if (!isdigit(c))
			return 0;

		val = 0;
		base = 10;

		if (c == '0') {
			c = *++cp;

			if (c == 'x' || c == 'X')
				base = 16, c = *++cp;
			else
				base = 8;
		}

		for (;;) {
			if (isascii(c) && isdigit(c)) {
				val = (val * base) + (c - '0');
				c = *++cp;
			} else if (base == 16 && isascii(c) && isxdigit(c)) {
				val = (val << 4) |
				      (c + 10 - (islower(c) ? 'a' : 'A'));
				c = *++cp;
			} else
				break;
		}

		if (c == '.') {
			/*
			 * Internet format:
			 *      a.b.c.d
			 *      a.b.c   (with c treated as 16 bits)
			 *      a.b     (with b treated as 24 bits)
			 */
			if (pp >= parts + 3)
				return 0;

			*pp++ = val;
			c = *++cp;
		} else
			break;
	}

	/*
	 * Check for trailing characters.
	 */
	if (c != '\0' && (!isascii(c) || !isspace(c)))
		return 0;

	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts + 1;

	switch (n) {
	case 0:
		return 0;             /* initial nondigit */

	case 1:                         /* a -- 32 bits */
		break;

	case 2:                         /* a.b -- 8.24 bits */
		if (val > 0xffffff)
			return 0;

		val |= parts[0] << 24;
		break;

	case 3:                         /* a.b.c -- 8.8.16 bits */
		if (val > 0xffff)
			return 0;

		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4:                         /* a.b.c.d -- 8.8.8.8 bits */
		if (val > 0xff)
			return 0;

		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}

	if (addr)
		addr->s_addr = htonl(val);

	return 1;
}
EXPORT_SYMBOL(tz_inet_aton);

/*
 * Ascii internet address interpretation routine.
 * The value returned is in network order.
 */

/* inet_addr */
long tz_inet_addr(char *cp)
{
	struct in_addr val;

	if (tz_inet_aton(cp, &val))
		return val.s_addr;

	return INADDR_NONE;
}
EXPORT_SYMBOL(tz_inet_addr);

int tz_socket(int family, int type, int protocol, unsigned long para_address, unsigned long buffer_addr)
{
#ifdef QEMU
	int retval = 0;
	struct socket_params_t *sock_para = NULL;
	sock_para = kmalloc(sizeof(struct socket_params_t), GFP_KERNEL);

	memset(sock_para, 0, sizeof(struct socket_params_t));
	sock_para->domain = family;
	sock_para->type = type;
	sock_para->protocol = protocol;

	memcpy(para_address, sock_para, sizeof(struct socket_params_t));

	writel(0x00, SOCKET_BASE + FUCTION_socket);
	retval = ((struct socket_params_t *)para_address)->returns;
	kfree(sock_para);
	pr_info("socket function return value = %d\n", retval);

#else
	int retval = 0;
	struct socket *sock = NULL;
	int flags = 0;

	/* Check the SOCK_* constants for consistency.  */
	BUILD_BUG_ON(SOCK_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON((SOCK_MAX | SOCK_TYPE_MASK) != SOCK_TYPE_MASK);
	BUILD_BUG_ON(SOCK_CLOEXEC & SOCK_TYPE_MASK);
	BUILD_BUG_ON(SOCK_NONBLOCK & SOCK_TYPE_MASK);

	flags = type & ~SOCK_TYPE_MASK;

	if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
		return -EINVAL;

	type &= SOCK_TYPE_MASK;

	if (SOCK_NONBLOCK != O_NONBLOCK && (flags & SOCK_NONBLOCK))
		flags = (flags & ~SOCK_NONBLOCK) | O_NONBLOCK;

	/* pr_info("---tz_socket family = %d, type = %d, protocol = %d\n", family, type, protocol); */
	retval = sock_create(family, type, protocol, &sock);

	if (retval < 0)
		goto out;

	/* retval = sock_map_fd(sock, flags & (O_CLOEXEC | O_NONBLOCK)); */
	if (retval < 0)
		goto out_release;

out:
	/* It may be already another descriptor 8) Not kernel problem. */
	return retval;

out_release:
	sock_release(sock);
	return retval;
#endif
	return retval;
}
EXPORT_SYMBOL(tz_socket);


int tz_htons(unsigned short int h)
{
	return htons(h);
}


static struct socket *tz_sockfd_lookup_light(int fd, int *err, int *fput_needed)
{
	struct file *file;
	struct socket *sock;

	*err = -EBADF;
	file = fget_light(fd, fput_needed);

	if (file) {
		sock = sock_from_file(file, err);

		if (sock)
			return sock;

		fput_light(file, *fput_needed);
	}

	return NULL;
}



/*      ======================== tz_connect ===========================
 *      Attempt to connect to a socket with the server address.  The address
 *      is in kernel space.
 */

int tz_connect(int fd, struct sockaddr *address, int addrlen, unsigned long para_address, unsigned long buffer_addr)
{
#ifdef QEMU
	int retval = 0;
	struct connect_params_t *connect_para = NULL;
	connect_para = kmalloc(sizeof(struct connect_params_t), GFP_KERNEL);

	memset(connect_para, 0, sizeof(struct connect_params_t));


	connect_para->sockfd = fd;
	memcpy(&(connect_para->addr), address, addrlen);
	connect_para->addrlen = addrlen;

	memcpy(para_address, connect_para, sizeof(struct connect_params_t));

	writel(0x00, SOCKET_BASE+FUCTION_connect);
	retval = ((struct connect_params_t *)para_address)->returns;
	pr_info("connect function return value = %d\n", retval);
	kfree(connect_para);
	return retval;




#else
	struct socket *sock;
	int err, fput_needed;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (!sock)
		goto out;

	err = sock->ops->connect(sock, (struct sockaddr *)address, addrlen,
				sock->file->f_flags);
	/* This function is in the net/ipv4/af_inet.c file. */

	fput_light(sock->file, fput_needed);
out:
	return err;
#endif


}
EXPORT_SYMBOL(tz_connect);

/*      ======================== tz_bind ===========================
 *      Bind a name to a socket. Nothing much to do here since it's
 *      the protocol's responsibility to handle the local address.
 *
 *      We move the socket address to kernel space before we call
 *      the protocol layer (having also checked the address is ok).
 */

int tz_bind(int fd, struct sockaddr *addr, int addrlen)
{
	struct socket *sock;
	int err, fput_needed;

	err = 0;
	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (sock) {
		err = sock->ops->bind(sock, (struct sockaddr *)addr, addrlen);
		fput_light(sock->file, fput_needed);
	}

	return err;
}


/*      ======================== tz_bind ===========================
 *      Perform a listen. Basically, we allow the protocol to do anything
 *      necessary for a listen, and if that works, we mark the socket as
 *      ready for listening.
 */

int tz_listen(int fd, int backlog)
{
	struct socket *sock;
	int err, fput_needed;
	int somaxconn;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (sock) {
		somaxconn = sock_net(sock->sk)->core.sysctl_somaxconn;

		if ((unsigned int)backlog > somaxconn)
			backlog = somaxconn;

		err = sock->ops->listen(sock, backlog);
		fput_light(sock->file, fput_needed);
	}

	return err;
}


/*      ========================= tz_zone sys_accept ===================
 *      For accept, we attempt to create a new socket, set up the link
 *      with the client, wake up the client, then return the new
 *      connected fd. We collect the address of the connector in kernel
 *      space and move it to user at the very end. This is unclean because
 *      we open the socket then return an error.
 *
 *      1003.1g adds the ability to recvmsg() to query connection pending
 *      status to recvmsg. We need to add that support in a way thats
 *      clean when we restucture accept also.
 */

int tz_accept4(int fd, struct sockaddr *upeer_sockaddr,
		int *upeer_addrlen, int flags)
{
	struct socket *sock, *newsock;
	struct file *newfile;
	int err, len, newfd, fput_needed, cpylen;
	struct sockaddr_storage address;

	if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
		return -EINVAL;

	if (SOCK_NONBLOCK != O_NONBLOCK && (flags & SOCK_NONBLOCK))
		flags = (flags & ~SOCK_NONBLOCK) | O_NONBLOCK;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (!sock)
		goto out;

	err = -ENFILE;

	newsock = sock_alloc();

	if (NULL == newsock)
		goto out_put;

	newsock->type = sock->type;
	newsock->ops = sock->ops;

	/*
	 * We don't need try_module_get here, as the listening socket (sock)
	 * has the protocol module (sock->ops->owner) held.
	 */
	__module_get(newsock->ops->owner);

	/*
	 * FIXME Do not has this function.
	 * newfd = sock_alloc_file(newsock, &newfile, flags);
	 */
#ifdef QEMU
	newfd = sock_alloc_fd(&newfile, flags);
#else
	newfd = sock_alloc_file(newsock, &newfile, flags);
#endif

	if (unlikely(newfd < 0)) {
		err = newfd;
		sock_release(newsock);
		goto out_put;
	}

#ifdef QEMU
	err = sock_attach_fd(newsock, newfile, flags);

	if (err < 0)
		goto out_fd_simple;

#endif

	err = security_socket_accept(sock, newsock);

	if (err)
		goto out_fd;

	err = sock->ops->accept(sock, newsock, sock->file->f_flags);

	if (err < 0)
		goto out_fd;

	if (upeer_sockaddr) {
		if (newsock->ops->getname(newsock, (struct sockaddr *)&address,
					&len, 2) < 0) {
			err = -ECONNABORTED;
			goto out_fd;
		}

		/* copy socket address between kernel space.*/
		if (len > *upeer_addrlen)
			cpylen = *upeer_addrlen;
		else
			cpylen = len;

		memcpy(upeer_sockaddr, (struct sockaddr *)&address, cpylen);
	}

	/* File flags are not inherited via accept() unlike another OSes. */

	fd_install(newfd, newfile);
	err = newfd;

out_put:
	fput_light(sock->file, fput_needed);
out:
	return err;
out_fd_simple:
	sock_release(newsock);
	put_filp(newfile);
	put_unused_fd(newfd);
	goto out_put;
out_fd:
	fput(newfile);
	put_unused_fd(newfd);
	goto out_put;
}



int tz_accept(int fd, struct sockaddr *upeer_sockaddr, int *upeer_addrlen)
{
	return tz_accept4(fd, upeer_sockaddr, upeer_addrlen, 0);
}

/*     ==================== tz_send ========================== */
int tz_sendto(int fd, void *buff, size_t len, unsigned int flags, struct sockaddr *addr, int addr_len)
{
	struct socket *sock;
	int err;
	struct msghdr msg;
	struct iovec iov;
	int fput_needed;

	if (len > INT_MAX)
		len = INT_MAX;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (!sock)
		goto out;

	iov.iov_base = buff;
	iov.iov_len = len;
	msg.msg_name = NULL;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_namelen = 0;

	if (addr) {
		msg.msg_name = (struct sockaddr *)addr;
		msg.msg_namelen = addr_len;
	}

	if (sock->file->f_flags & O_NONBLOCK)
		flags |= MSG_DONTWAIT;

	msg.msg_flags = flags;
	err = sock_sendmsg(sock, &msg, len);

	fput_light(sock->file, fput_needed);
out:
	return err;
}

/*
 *      Send a datagram down a socket.
 */

int tz_send(int fd, void *buff, size_t len, unsigned int flags, unsigned long para_address, unsigned long buffer_addr)
{
#ifdef QEMU
	int retval = 0;
	struct send_params_t *send_para = NULL;
	send_para = kmalloc(sizeof(struct send_params_t), GFP_KERNEL);

	memset(send_para, 0, sizeof(struct send_params_t));


	send_para->sockfd = fd;
	memcpy(buffer_addr, buff, len);
	send_para->len = len;
	send_para->flags = flags;
	memcpy(para_address, send_para, sizeof(struct send_params_t));

	writel(0x00, SOCKET_BASE + FUCTION_send);
	retval = ((struct send_params_t *)para_address)->returns;
	pr_info("send function return value = %d\n", retval);
	kfree(send_para);

	return retval;


#else
	long ret;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(get_ds());
	ret = sys_sendto(fd, buff, len, flags, NULL, 0);
	set_fs(old_fs);
	mdelay(10);
	return ret;

#endif
}
EXPORT_SYMBOL(tz_send);
/********************************************************************
 *                       tz_recv                        *
 ********************************************************************/
int tz_recvfrom(int fd, void *ubuf, size_t size,
		unsigned flags, struct sockaddr *addr,
		int *addr_len)
{
	struct socket *sock;
	struct iovec iov;
	struct msghdr msg;
	struct sockaddr_storage address;
	int err;
	int cpylen;
	int fput_needed;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (!sock)
		goto out;

	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iovlen = 1;
	msg.msg_iov = &iov;
	iov.iov_len = size;
	iov.iov_base = ubuf;
	msg.msg_name = (struct sockaddr *)&address;
	msg.msg_namelen = sizeof(address);

	if (sock->file->f_flags & O_NONBLOCK)
		flags |= MSG_DONTWAIT;

	err = sock_recvmsg(sock, &msg, size, flags);
	pr_info("the err is %d\n", err);

	if (err >= 0 && addr != NULL) {
		if (msg.msg_namelen > *addr_len)
			cpylen = *addr_len;
		else
			cpylen = msg.msg_namelen;

		memcpy(addr, &address, cpylen);
	}

	fput_light(sock->file, fput_needed);
out:
	return err;
}



/***********************************************************************
 *      Receive a datagram from a socket.
 ***********************************************************************/

asmlinkage long tz_recv(int fd, void *ubuf, size_t size,
			unsigned flags, unsigned long para_address, unsigned long buffer_addr)
{

#ifdef QEMU
	int retval = 0;
	struct recv_params_t *recv_para = NULL;
	recv_para = kmalloc(sizeof(struct recv_params_t), GFP_KERNEL);

	memset(recv_para, 0, sizeof(struct recv_params_t));


	recv_para->sockfd = fd;
	/* memcpy(buffer_addr, buff, len); */
	recv_para->len = size;
	recv_para->flags = flags;
	memcpy(para_address, recv_para, sizeof(struct recv_params_t));

	writel(0x00, SOCKET_BASE+FUCTION_recv);
	retval = ((struct recv_params_t *)para_address)->returns;
	pr_info("recv function return value = %d\n", retval);

	if (retval > 0)
		memcpy(ubuf, buffer_addr, retval);

	kfree(recv_para);
	return retval;


#else
	/* return tz_recvfrom(fd, ubuf, size, flags, NULL, NULL); */
	long ret;
	mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(get_ds());
	ret = sys_recvfrom(fd, ubuf, size, flags, NULL, NULL);
	/* pr_info(">>>>>>>>>>>>>>> recv length is %d bytes <<<<<<<<<<<<<<<<<<<<\n", ret); */
	set_fs(old_fs);
	return ret;

#endif
}
EXPORT_SYMBOL(tz_recv);

int tz_setsocketopt(int fd, int level, int optname, char *optval,
		int optlen, unsigned long para_address, unsigned long buffer_addr)
{
	int err, fput_needed;
	struct socket *sock;

	if (optlen < 0)
		return -EINVAL;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (sock != NULL) {

		if (level == SOL_SOCKET)
			err = sock_setsockopt(sock, level, optname, optval, optlen);
		else
			err = sock->ops->setsockopt(sock, level, optname, optval, optlen);

		fput_light(sock->file, fput_needed);
	}

	return err;
}




int tz_shutdown(int fd, int how)
{
	int err, fput_needed;
	struct socket *sock;

	sock = tz_sockfd_lookup_light(fd, &err, &fput_needed);

	if (sock != NULL) {
		err = security_socket_shutdown(sock, how);

		if (!err)
			err = sock->ops->shutdown(sock, how);

		fput_light(sock->file, fput_needed);
	}

	return err;
}

asmlinkage long tz_close(int fd, unsigned long para_address, unsigned long buffer_addr)
{
#if 0
	int retval = 0;
	struct close_params_t *close_para = NULL;
	close_para = kmalloc(sizeof(struct close_params_t), GFP_KERNEL);

	memset(close_para, 0, sizeof(struct close_params_t));


	close_para->sockfd = fd;
	memcpy(para_address, close_para, sizeof(struct close_params_t));

	writel(0x00, SOCKET_BASE+FUCTION_close);
	retval = ((struct close_params_t *)para_address)->returns;
	pr_info("close function return value = %d\n", retval);
	kfree(close_para);
	return retval;


#else
	return sys_close(fd);
#endif
}
EXPORT_SYMBOL(tz_close);

void Worldtransport(int world_name, unsigned long phy_addr)
{
	if (TRUST_ZONE == world_name)
		smc_call(NT_SMC_SWITCH, NT_SMC_SWITCH_SECOND, phy_addr, 0);

	return;
}

/**********************************************************************
 *               socket_thread_function
 **********************************************************************/

int socket_thread_function(unsigned long virt_addr, unsigned long para_vaddr, unsigned long buff_vaddr)
{
#if 1
	unsigned long sendRecv_address;
	unsigned long setsockopt_address;
	long retVal = 0;
	struct TEEI_socket_command *tz_command;
	union TEEI_socket_response_type tz_response;

	/* virt_addr = (unsigned long)__va(phy_addr); */

	/*
	memset(mem_head, '\0',  sizeof(mem_head));
	memcpy(mem_head, (void*)virt_addr, sizeof(mem_head));
	functionCode = mem_head[0];
	commandLength = mem_head[1];

	memset(&tz_command, '\0', commandLength);
	memcpy(&tz_command, (void*)virt_addr, commandLength);
	*/
	tz_command = (struct TEEI_socket_command *)virt_addr;

	/* pr_info(" come into the socket_thread_function, %x in %p\n", tz_command->func, &(tz_command->func)); */
	switch (tz_command->func) {
	case TZ_SOCKET:
		/* pr_info(" come into the SOCKET branch\n"); */
		retVal = tz_socket(tz_command->args.func_socket_args.af,
				tz_command->args.func_socket_args.type,
				tz_command->args.func_socket_args.protocol,
				para_vaddr,
				buff_vaddr);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	case TZ_HTONS:
		/* pr_info(" come into the TZ_HTONS branch\n"); */
		retVal = tz_htons(tz_command->args.func_htons_args.portnum);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.portnum = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;


	case TZ_INET_ADDR:
		/* pr_info(" come into the TZ_INET_ADDR branch\n"); */
		retVal = tz_inet_addr(tz_command->args.func_inet_addr_args.ip_addr);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.addr = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	case TZ_CONNECT:
		/* pr_info(" come into the TZ_CONNECT branch\n"); */
		retVal = tz_connect(tz_command->args.func_connect_args.sockfd,
				(struct sockaddr *)&(tz_command->args.func_connect_args.ob_addr),
				tz_command->args.func_connect_args.addrlen,
				para_vaddr,
				buff_vaddr);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.addr = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	case TZ_BIND:
		/* pr_info(" come into the TZ_BIND branch\n"); */
		retVal = tz_bind(tz_command->args.func_bind_args.sockfd,
				(struct sockaddr *)&(tz_command->args.func_bind_args.ob_addr),
				tz_command->args.func_bind_args.addrlen);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;


	case TZ_LISTEN:
		/* pr_info(" come into the TZ_LISTEN branch\n"); */
		retVal = tz_listen(tz_command->args.func_listen_args.sockfd,
				tz_command->args.func_listen_args.backlog);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	case TZ_ACCEPT:
		/* pr_info(" come into the TZ_ACCEPT branch\n"); */
		retVal = tz_accept(tz_command->args.func_accept_args.sockfd,
				(struct sockaddr *)&(tz_command->args.func_accept_args.ob_addr),
				&(tz_command->args.func_accept_args.addrlen));

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;


	case TZ_RECV:
		/* pr_info(" come into the TZ_RECV branch\n"); */
		sendRecv_address = 0;
		sendRecv_address = kmalloc(tz_command->args.func_recv_args.len, GFP_KERNEL);
		retVal = tz_recv(tz_command->args.func_recv_args.sockfd,
				(void *)sendRecv_address,
				tz_command->args.func_recv_args.len,
				tz_command->args.func_recv_args.flags,
				para_vaddr,
				buff_vaddr);
		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.recv.size = retVal;
		tz_response.recv.buf = (void *)sendRecv_address;

		if (retVal > 0)
			memset((void *)virt_addr, '\0', sizeof(tz_response) + retVal);
		else
			memset((void *)virt_addr, '\0', sizeof(tz_response));

		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));

		if (retVal > 0)
			memcpy((void *)(virt_addr + sizeof(tz_response)), (void *)sendRecv_address, retVal);

		if (sendRecv_address != NULL)
			kfree(sendRecv_address);

		break;


	case TZ_SEND:
		sendRecv_address = 0;
		sendRecv_address = (unsigned long)(tz_command) + sizeof(struct TEEI_socket_command);

		retVal = tz_send(tz_command->args.func_send_args.sockfd,
				(void *)sendRecv_address,
				tz_command->args.func_send_args.len,
				tz_command->args.func_send_args.flags,
				para_vaddr,
				buff_vaddr);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;

		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;


	case TZ_CLOSE:
		retVal = tz_close(tz_command->args.func_close_args.sockfd, para_vaddr, buff_vaddr);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	case TZ_SETSOCKOPT:
		setsockopt_address = 0;
		setsockopt_address = (unsigned long)(tz_command) + sizeof(struct TEEI_socket_command);
		retVal = tz_setsocketopt(tz_command->args.func_setsockopt_args.sockfd,
					tz_command->args.func_setsockopt_args.level,
					tz_command->args.func_setsockopt_args.optname,
					setsockopt_address,
					tz_command->args.func_setsockopt_args.optlen,
					para_vaddr, buff_vaddr);

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = retVal;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	default:

		memset(&tz_response, '\0', sizeof(tz_response));
		tz_response.value = EOPNOTSUPP;
		memset((void *)virt_addr, '\0', sizeof(tz_response));
		memcpy((void *)virt_addr, &tz_response, sizeof(tz_response));
		break;

	}

	return 0;
#endif
	pr_info("In the socket_thread_function function");
	return 0;
}
EXPORT_SYMBOL(socket_thread_function);

