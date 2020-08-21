#ifndef _UAPI_MSM_ETH_H_
#define _UAPI_MSM_ETH_H_

#include <linux/types.h>

/**
 * defines eth_meta_event - Events for eth
 *
 * CV2X pipe connect: CV2X pipe connected
 * CV2X pipe disconnect: CV2X pipe disconnected
 */
#define ETH_EVT_START 0
#define ETH_EVT_CV2X_PIPE_CONNECTED (ETH_EVT_START + 1)
#define ETH_EVT_CV2X_PIPE_DISCONNECTED (ETH_EVT_CV2X_PIPE_CONNECTED + 1)
#define ETH_EVT_CV2X_MODE_NOT_ENABLED (ETH_EVT_CV2X_PIPE_DISCONNECTED + 1)

/**
 * struct eth_msg_meta - Format of the message meta-data.
 * @msg_type: the type of the message
 * @rsvd: reserved bits for future use.
 * @msg_len: the length of the message in bytes
 *
 * For push model:
 * Client in user-space should issue a read on the device (/dev/emac) with a
 * sufficiently large buffer in a continuous loop, call will block when there is
 * no message to read. Upon return, client can read the eth_msg_meta from start
 * of buffer to find out type and length of message
 * size of buffer supplied >= (size of largest message + size of metadata)
 *
 */
struct eth_msg_meta {
	__u8  msg_type;
	__u8  rsvd;
	__u16 msg_len;
};

#endif /* _UAPI_MSM_ETH_H_ */
