/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2015,2017, 2018, 2020, The Linux Foundation. All rights reserved. */
/******************************************************************
 * DESCRIPTION
 * Header file for eMBMs Tunneling Module in kernel.
 *******************************************************************
 */

#ifndef EMBMS_H
#define EMBMS_H

#include <linux/ioctl.h>
#include <stdbool.h>
#include <linux/if_addr.h>
#include <linux/list.h>
#include <linux/ip.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>

#define EMBMS_MAX_IFACE_NAME    20

/* Defining IP and UDP header related macros*/

#define UDP_CHECKSUM                0
#define IP_VERSION                  4
#define IP_IHL                      5
#define IP_TOS                      0
#define IP_ID                       1
#define IP_FRAG_OFFSET              htons(0x4000)
#define IP_TTL                      64
#define BRIDGE_IFACE                "bridge0"

#define BUF_LEN 1024
#define TUNNELING_ON 1
#define TUNNELING_OFF 0

// definitions required for IOCTL
static unsigned int dev_num = 1;
/* Embms device used for communication*/
struct cdev embms_device;
static struct class *embms_class;
static dev_t device;
#define EMBMS_IOC_MAGIC 0x64

#define embms_debug pr_debug
#define embms_error pr_debug

/* The name of the device file*/
#define EMBMS_DEVICE_NAME "embms_tm_device"

extern int (*embms_tm_multicast_recv)(struct sk_buff *skb);

/**
 * enum embms_action_type - Describes action to perform
 * @ADD_CLIENT_ENTRY: add client entry to TMGI
 * @DELETE_CLIENT_ENTRY: deelte client entry from TMGI
 * @TMGI_DEACTIVATE: Delete TMGI entry
 * @CLIENT_ACTIVATE_ALL_TMGI: Add client to all TMGI
 * @CLIENT_DEACTIVATE_ALL_TMGI: Delete client from all TMGI
 * @SESSION_DEACTIVATE: Stop session
 * @SOCK_INFO: Socket information like V4 addr, port etc
 *
 * This enum defines the types of action which are
 * supported by this module.
 */

enum {
	ADD_CLIENT_ENTRY = 0,
	DELETE_CLIENT_ENTRY,
	TMGI_DEACTIVATE,
	CLIENT_ACTIVATE_ALL_TMGI,
	CLIENT_DEACTIVATE_ALL_TMGI,
	SESSION_DEACTIVATE,
	SOCK_INFO
} embms_action_type;

/**
 * struct tmgi_to_clnt_info_update - information for addition/deletion
 * @multicast_addr: TMGI multicast IP to receive data
 * @multicast_port: TMGI multicast port to receive date
 * @client_addr: Client IPV4 address for sending data
 * @client_port: Client port for sending data
 * @data_port: port used to send data to client
 * @action_type: Action to be performed
 * @iface_name: iface to listen to for data
 *
 * This structure contains information as to what action
 * needs to be performed on TMGI-client table. It is
 * sent as a parameter during an IOCTL call
 */

struct tmgi_to_clnt_info_update {
	u32 multicast_addr;
	u16 multicast_port;
	u32 client_addr;
	u16 client_port;
	u16 data_port;
	u32 action_type;
	char iface_name[EMBMS_MAX_IFACE_NAME];
};

/**
 * struct clnt_info - contains client information
 * @addr: Client IPV4 address for sending packets
 * @port: Client port for sending packets
 * @dmac: Client DMAC address
 * @client_list_ptr : list ptr used to maintain client list
 *
 * This structure maintains complete client information
 * to be used when sending packets to client
 */

struct clnt_info {
	u32 addr;
	u16 port;
	u8 dmac[ETH_ALEN];
	struct list_head client_list_ptr;
};

/**
 * struct tmgi_to_clnt_info - contains TMGI information
 * @tmgi_multicast_addr: TMGI IPV4 address to listen for packets
 * @tmgi_port: Client port to listen for packets
 * @no_of_clients: No of clients for a TMGI
 * @client_list_head : list head for client list
 * @tmgi_list_ptr : list ptr to maintain tmgi list
 *
 * This structure maintains complete client information
 * to be used when sending data to client
 */

struct tmgi_to_clnt_info {
	u32 tmgi_multicast_addr;
	u16 tmgi_port;
	u16 no_of_clients;
	struct list_head client_list_head;
	struct list_head tmgi_list_ptr;
};

/**
 * struct embms_info_internal - stores module specific params
 * @device_under_use: Used to prevent concurent access to the same device
 * @embms_data_port: Source Data port used for tunnelled packets
 * @embms_iface: Iface to receive embms traffic
 * @embms_tunneling_status : Current EMBMS Status
 * @no_of_tmgi_sessions : Number of current active TMGI sessions
 * @lock : Lock for concurrency scenarios
 * @ip_ident : IP identification number to be used for sent packets
 *
 * This tructure holds module specific information which is
 * used throughout the module to maintain consistency
 */

struct embms_info_internal {
	atomic_t device_under_use;
	int embms_data_port;
	char embms_iface[EMBMS_MAX_IFACE_NAME];
	int embms_tunneling_status;
	int no_of_tmgi_sessions;
	/*lock to prevent concurrent access*/
	spinlock_t lock;
	atomic_t ip_ident;
};

/* This ioctl is used to add a new client entry to tunneling module.
 * Entry params are populated in the struct used for ioctl
 */

#define ADD_EMBMS_TUNNEL _IOW(EMBMS_IOC_MAGIC, 0, \
		struct tmgi_to_clnt_info_update)

/* This ioctl is used to delete a client entry for a particular
 * TMGI from tunneling module.
 * Entry params are populated in the struct used for ioctl
 */

#define DEL_EMBMS_TUNNEL _IOW(EMBMS_IOC_MAGIC, 1, \
		struct tmgi_to_clnt_info_update)

/* This ioctl is used to delete a TMGI entry completely
 * from tunneling module.
 * Entry params are populated in the struct used for ioctl
 */

#define TMGI_DEACTIVATE _IOW(EMBMS_IOC_MAGIC, 2, \
		struct tmgi_to_clnt_info_update)

/* This ioctl is used to delete client entry completely
 * from tunneling module.
 * Entry params are populated in the struct used for ioctl
 */

#define CLIENT_DEACTIVATE _IOW(EMBMS_IOC_MAGIC, 3, \
		struct tmgi_to_clnt_info_update)

/* Gets the ON/OFF status of Tunneling module*/

#define GET_EMBMS_TUNNELING_STATUS _IO(EMBMS_IOC_MAGIC, 4)

/* Used to start tunneling. Argument is the port
 * number to be used to send
 * data to clients
 */

#define START_EMBMS_TUNNEL _IOW(EMBMS_IOC_MAGIC, 5, \
		struct tmgi_to_clnt_info_update)

/* Used to stop tunnleing*/

#define STOP_EMBMS_TUNNEL _IO(EMBMS_IOC_MAGIC, 6)

/* Return values indicating error status*/
#define SUCCESS	0	/* Successful operation*/
#define FAILURE	-1	/* Unsuccessful operation*/

/* Error Condition Values*/
#define ENOMEM	-2	/* Out of memory*/
#define EBADPARAM	-3	/* Incorrect parameters passed*/
#define ENOEFFECT	-4	/* No Effect*/

#endif

