#ifndef __XM_ISPV4_RPMSG_H
#define __XM_ISPV4_RPMSG_H

#include <linux/rpmsg.h>
#include <linux/cdev.h>
#include <linux/bits.h>

#define MIPC_MSGID_MASK 0xf0000000
#define MIPC_MSGID_SHIFT 28
#define MIPC_MSGID_MAX (~MIPC_MSGID_MASK)

#define MIPC_MSGID_SEND2AP_CID 1
#define MIPC_MSGID_SEND2V4_CID 2

#define MIPC_MSGID_KSEND2V4_CID 3

#define MIPC_NEED_ACK(t)                                                       \
	((t) == MIPC_MSGHEADER_CMD_NEED_ACK ||                                 \
	 (t) == MIPC_MSGHEADER_COMMON_NEED_ACK)

/* meta-data for ack msg */
#define MIPC_MSGHEADER_ACK_BUSY 0
#define MIPC_MSGHEADER_ACK_FIFO 1
#define MIPC_MSGHEADER_ACK_IMD 2

#pragma pack(1)
struct mipc_rpmsg_cheader {
	u32 msg_header_id;
	u32 msg_header_type;
	u32 msg_header_meta_data;
};
#pragma pack()

#pragma pack(1)
struct mipc_rpmsg_cheader_wrapper {
	struct mipc_rpmsg_cheader header;
	char raw[0];
};
#pragma pack()

#define MIPC_CMD_HEADER_SIZE sizeof(struct mipc_rpmsg_cheader)
typedef struct mipc_rpmsg_cheader_wrapper mipc_cmd_format_t;
#define FORMAT_RAW_CMD(buf) (mipc_cmd_format_t *)(buf)
#define HEADER_FORMAT_PTR(x, buf) mipc_cmd_format_t *x = FORMAT_RAW_CMD(buf)

/* ioctl */
#define RPEPT_CMD_SEND 0
#define RPEPT_CMD_RECV 1
#define RPEPT_CMD_POPE 2

#define RPEPT_IOCTL_RG_CMD GENMASK(3, 0)

#define RPEPT_IOCTL_RG_SENDT GENMASK(8, 4)
#define RPEPT_IOCTL_RG_SENDL GENMASK(18, 8)

#endif
