#ifndef _UAPI_MSM_IPC_H_
#define _UAPI_MSM_IPC_H_

#include <linux/types.h>
#include <linux/ioctl.h>

struct msm_ipc_port_addr {
	uint32_t node_id;
	uint32_t port_id;
};

struct msm_ipc_port_name {
	uint32_t service;
	uint32_t instance;
};

struct msm_ipc_addr {
	unsigned char  addrtype;
	union {
		struct msm_ipc_port_addr port_addr;
		struct msm_ipc_port_name port_name;
	} addr;
};

#define MSM_IPC_WAIT_FOREVER	(~0)  /* timeout for permanent subscription */

/*
 * Socket API
 */

#ifndef AF_MSM_IPC
#define AF_MSM_IPC		27
#endif

#ifndef PF_MSM_IPC
#define PF_MSM_IPC		AF_MSM_IPC
#endif

#define MSM_IPC_ADDR_NAME		1
#define MSM_IPC_ADDR_ID			2

struct sockaddr_msm_ipc {
	unsigned short family;
	struct msm_ipc_addr address;
	unsigned char reserved;
};

struct config_sec_rules_args {
	int num_group_info;
	uint32_t service_id;
	uint32_t instance_id;
	unsigned reserved;
	gid_t group_id[0];
};

#define IPC_ROUTER_IOCTL_MAGIC (0xC3)

#define IPC_ROUTER_IOCTL_GET_VERSION \
	_IOR(IPC_ROUTER_IOCTL_MAGIC, 0, unsigned int)

#define IPC_ROUTER_IOCTL_GET_MTU \
	_IOR(IPC_ROUTER_IOCTL_MAGIC, 1, unsigned int)

#define IPC_ROUTER_IOCTL_LOOKUP_SERVER \
	_IOWR(IPC_ROUTER_IOCTL_MAGIC, 2, struct sockaddr_msm_ipc)

#define IPC_ROUTER_IOCTL_GET_CURR_PKT_SIZE \
	_IOR(IPC_ROUTER_IOCTL_MAGIC, 3, unsigned int)

#define IPC_ROUTER_IOCTL_BIND_CONTROL_PORT \
	_IOR(IPC_ROUTER_IOCTL_MAGIC, 4, unsigned int)

#define IPC_ROUTER_IOCTL_CONFIG_SEC_RULES \
	_IOR(IPC_ROUTER_IOCTL_MAGIC, 5, struct config_sec_rules_args)

struct msm_ipc_server_info {
	uint32_t node_id;
	uint32_t port_id;
	uint32_t service;
	uint32_t instance;
};

struct server_lookup_args {
	struct msm_ipc_port_name port_name;
	int num_entries_in_array;
	int num_entries_found;
	uint32_t lookup_mask;
	struct msm_ipc_server_info srv_info[0];
};

#endif
