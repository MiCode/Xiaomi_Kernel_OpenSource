#ifndef _SOCKEV_H_
#define _SOCKEV_H_

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/socket.h>

enum sknetlink_groups {
	SKNLGRP_UNICAST,
	SKNLGRP_SOCKEV,
	__SKNLGRP_MAX
};

#define SOCKEV_STR_MAX 32

/********************************************************************
*		Socket operation messages
****/

struct sknlsockevmsg {
	__u8 event[SOCKEV_STR_MAX];
	__u32 pid; /* (struct task_struct*)->pid */
	__u16 skfamily; /* (struct socket*)->sk->sk_family */
	__u8 skstate; /* (struct socket*)->sk->sk_state */
	__u8 skprotocol; /* (struct socket*)->sk->sk_protocol */
	__u16 sktype; /* (struct socket*)->sk->sk_type */
	__u64 skflags; /* (struct socket*)->sk->sk_flags */
};

#endif /* _SOCKEV_H_ */

