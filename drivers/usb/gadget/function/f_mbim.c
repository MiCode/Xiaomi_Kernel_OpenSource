/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>

#include <uapi/linux/usb/cdc.h>

#include <linux/etherdevice.h>
#include <linux/crc32.h>

#include <linux/spinlock.h>

#include <linux/kmod.h> /* for call call_usermodehelper */


#include <uapi/linux/usb/ch9.h>

#include "mbim_ether.h"

/*
 * This function is a "Mobile Broadband Interface Model" (MBIM) link.
 * MBIM is intended to be used with high-speed network attachments.
 *
 * Note that MBIM requires the use of "alternate settings" for its data
 * interface.  This means that the set_alt() method has real work to do,
 * and also means that a get_alt() method is required.
 */

#define MBIM_EVENT_BY_FILE  1

#define MBIM_BULK_BUFFER_SIZE       4096
#define MAX_CTRL_PKT_SIZE           4096

struct ioctl_arg_sid_ifid {
		unsigned int sid;
		unsigned int ifid;
};
struct ioctl_arg_event_data {
		unsigned char event;
		unsigned char data[7];
};


#define MBIM_IOCTL_MAGIC        'o'
#define MBIM_GET_NTB_SIZE           _IOR(MBIM_IOCTL_MAGIC, 2, u32)
#define MBIM_GET_DATAGRAM_COUNT     _IOR(MBIM_IOCTL_MAGIC, 3, u16)
#define MBIM_SET_LOOPBACK_MODE      _IOW(MBIM_IOCTL_MAGIC, 4, u8)
#define MBIM_SET_SID_IFID           _IOW(MBIM_IOCTL_MAGIC, 5, struct ioctl_arg_sid_ifid)
#define MBIM_GET_EVENT              _IOR(MBIM_IOCTL_MAGIC, 6, struct ioctl_arg_event_data)

#define NR_MBIM_PORTS           1


struct ctrl_pkt {
	void            *buf;
	int         len;
	struct list_head    list;
};
#if 0
struct mbim_ep_descs {
	struct usb_endpoint_descriptor  *in;
	struct usb_endpoint_descriptor  *out;
	struct usb_endpoint_descriptor  *notify;
};
#endif

struct mbim_notify_port {
	struct usb_ep           *notify;
	struct usb_request      *notify_req;
	u8              notify_state;
	atomic_t            notify_count;
	struct usb_endpoint_descriptor *desc;
};

enum mbim_notify_state {
	NCM_NOTIFY_NONE,
	NCM_NOTIFY_CONNECT,
	NCM_NOTIFY_SPEED,
	NCM_NOTIFY_RESPONSE_AVAILABLE,
};

#ifdef MBIM_MULTI_SESSION
#define MAX_SESSION_NUM 11
#else
#define MAX_SESSION_NUM 11
#endif

struct f_mbim {
	struct usb_composite_dev *cdev;

	atomic_t    online;
	bool        is_open;

	atomic_t    open_excl;
	atomic_t    ioctl_excl;
	atomic_t    read_excl;
	atomic_t    write_excl;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;

	u8              port_num;
	struct mbim_gether   gether_port;
	struct net_device    *net;
	char                 ethaddr[14];
	u8                   host_addr[ETH_ALEN];

	struct mbim_notify_port     not_port;
#if 0
	struct mbim_ep_descs        fs;
	struct mbim_ep_descs        hs;
#endif

	u8              ctrl_id, data_id;
	u8              data_alt_int;

	struct mbim_ndp_parser_opts      *parser_opts;
	bool                            is_crc;


	spinlock_t          lock;

	struct list_head    cpkt_req_q;
	atomic_t    req_count;
	struct list_head    cpkt_resp_q;
	atomic_t    resp_count;
#ifdef MBIM_EVENT_BY_FILE
	struct list_head    cpkt_event_q;
	wait_queue_head_t   event_read_wq;
	atomic_t            event_read_excl;
#endif
	struct work_struct  linkup_work;
	struct work_struct  linkdown_work;

	u32                 ntb_input_size;
	u16                 ntb_max_datagrams;
	bool                loopback_mode;

	pid_t               user_pid;

	atomic_t            error;


	int                 ethid_to_sid_tbl[MAX_SESSION_NUM];
#ifdef MBIM_MULTI_SESSION
	int                 interface_state[MAX_SESSION_NUM];
#endif
};

struct mbim_ntb_input_size {
	u32 ntb_input_size;
	u16 ntb_max_datagrams;
	u16 reserved;
};

/* temporary variable used between mbim_open() and mbim_gadget_bind() */
static struct f_mbim *_mbim_dev;

static unsigned int nr_mbim_ports;

static struct mbim_ports {
	struct f_mbim   *port;
	unsigned    port_num;
} mbim_ports[NR_MBIM_PORTS];

#if 1
static inline struct f_mbim *func_to_mbim(struct usb_function *f)
{
	return container_of(f, struct f_mbim, gether_port.func);
}
#else
static inline struct f_mbim *func_to_mbim(struct usb_function *f)
{
	return container_of(f, struct f_mbim, function);
}

#endif

/* peak (theoretical) bulk transfer rate in bits-per-second */
static inline unsigned mbim_bitrate(struct usb_gadget *g)
{
	if (gadget_is_superspeed(g) && g->speed == USB_SPEED_SUPER)
		return 13 * 1024 * 8 * 1000 * 8;
	else if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return 13 * 512 * 8 * 1000 * 8;
	else
		return 19 *  64 * 1 * 1000 * 8;
}

/*-------------------------------------------------------------------------*/

#define NTB_DEFAULT_IN_SIZE USB_CDC_NCM_NTB_MIN_IN_SIZE
#define NTB_OUT_SIZE        USB_CDC_NCM_NTB_MIN_OUT_SIZE
#define NDP_IN_DIVISOR      (0x4)

#define FORMATS_SUPPORTED   USB_CDC_NCM_NTB16_SUPPORTED

static struct usb_cdc_ncm_ntb_parameters ntb_parameters = {
	.wLength = sizeof(ntb_parameters),
	.bmNtbFormatsSupported = cpu_to_le16(FORMATS_SUPPORTED),
	.dwNtbInMaxSize = cpu_to_le32(NTB_DEFAULT_IN_SIZE),
	.wNdpInDivisor = cpu_to_le16(NDP_IN_DIVISOR),
	.wNdpInPayloadRemainder = cpu_to_le16(0),
	.wNdpInAlignment = cpu_to_le16(4),

	.dwNtbOutMaxSize = cpu_to_le32(NTB_OUT_SIZE),
	.wNdpOutDivisor = cpu_to_le16(4),
	.wNdpOutPayloadRemainder = cpu_to_le16(0),
	.wNdpOutAlignment = cpu_to_le16(4),
	.wNtbOutMaxDatagrams = 1,
};

/*
 * Use wMaxPacketSize big enough to fit CDC_NOTIFY_SPEED_CHANGE in one
 * packet, to simplify cancellation; and a big transfer interval, to
 * waste less bandwidth.
 */

#define MBIM_STATUS_INTERVAL_MS    32

#ifndef EP_NOTIFY_LEN
#define NCM_STATUS_BYTECOUNT        16  /* 8 byte header + data */
#else
#define NCM_STATUS_BYTECOUNT        64  /* 8 byte header + data */
#define NCM_STATUS_BYTECOUNT_16     16  /* 8 byte header + data */
#endif

static struct usb_interface_assoc_descriptor mbim_iad_desc = {
	.bLength = sizeof(mbim_iad_desc),
	.bDescriptorType = USB_DT_INTERFACE_ASSOCIATION,
	/* .bFirstInterface =   DYNAMIC, */
	.bInterfaceCount = 2,  /* control + data */
	.bFunctionClass = 2,
	.bFunctionSubClass = 0x0e,
	.bFunctionProtocol = 0,
	/* .iFunction =     DYNAMIC */
};

/* interface descriptor: */
static struct usb_interface_descriptor mbim_control_intf = {
	.bLength = sizeof(mbim_control_intf),
	.bDescriptorType =  USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =    1,
	.bInterfaceClass =  0x02,
	.bInterfaceSubClass =   0x0e,
	.bInterfaceProtocol =   0,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc mbim_header_desc = {
	.bLength = sizeof(mbim_header_desc),
	.bDescriptorType =  USB_DT_CS_INTERFACE,
	.bDescriptorSubType =   USB_CDC_HEADER_TYPE,
	.bcdCDC =       cpu_to_le16(0x0110),
};

static struct usb_cdc_union_desc mbim_union_desc = {
	.bLength = sizeof(mbim_union_desc),
	.bDescriptorType =  USB_DT_CS_INTERFACE,
	.bDescriptorSubType =   USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 = DYNAMIC */
	/* .bSlaveInterface0 =  DYNAMIC */
};

static struct usb_cdc_mbim_desc mbim_desc = {
	.bLength = sizeof(mbim_desc),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_MBIM_TYPE,
	.bcdMBIMVersion = cpu_to_le16(0x0100),
	.wMaxControlMessage = cpu_to_le16(0x0200),
	.bNumberFilters =   0x10,
	.bMaxFilterSize =   0x40,
	.wMaxSegmentSize = cpu_to_le16(1500),
	.bmNetworkCapabilities = 0x20,
};

static struct usb_cdc_mbim_extended_desc mbim_ext_desc = {
	.bLength = sizeof(mbim_ext_desc),
	.bDescriptorType = USB_DT_CS_INTERFACE,
	.bDescriptorSubType = USB_CDC_MBIM_EXTENDED_TYPE,

	.bcdMBIMExtendedVersion = cpu_to_le16(0x0100),
	.bMaxOutstandingCommandMessages =  64,
	.wMTU = 1500,
};

/* the default data interface has no endpoints ... */
static struct usb_interface_descriptor mbim_data_nop_intf = {
	.bLength = sizeof(mbim_data_nop_intf),
	.bDescriptorType = USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	.bAlternateSetting =    0,
	.bNumEndpoints =    0,
	.bInterfaceClass =  0x0a,
	.bInterfaceSubClass =   0,
	.bInterfaceProtocol =   0x02,
	/* .iInterface = DYNAMIC */
};

/* ... but the "real" data interface has two bulk endpoints */
static struct usb_interface_descriptor mbim_data_intf = {
	.bLength = sizeof(mbim_data_intf),
	.bDescriptorType = USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	.bAlternateSetting =    1,
	.bNumEndpoints =    2,
	.bInterfaceClass =  0x0a,
	.bInterfaceSubClass =   0,
	.bInterfaceProtocol =   0x02,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor fs_mbim_notify_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,
#ifndef EP_NAMING
	.bEndpointAddress = USB_DIR_IN,
#else
	.bEndpointAddress =	USB_DIR_IN | (0x07 & USB_ENDPOINT_NUMBER_MASK), /* 7T */
#endif
	.bmAttributes =     USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =   cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval =        MBIM_STATUS_INTERVAL_MS,
};

static struct usb_endpoint_descriptor fs_mbim_in_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes =     USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_mbim_out_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes =     USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *mbim_fs_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &mbim_ext_desc,
	(struct usb_descriptor_header *) &fs_mbim_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &fs_mbim_in_desc,
	(struct usb_descriptor_header *) &fs_mbim_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor hs_mbim_notify_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes =     USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =   cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval =        USB_MS_TO_HS_INTERVAL(MBIM_STATUS_INTERVAL_MS),
};
static struct usb_endpoint_descriptor hs_mbim_in_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes =     USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =   cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_mbim_out_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes =     USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =   cpu_to_le16(512),
};

static struct usb_descriptor_header *mbim_hs_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &mbim_ext_desc,
	(struct usb_descriptor_header *) &hs_mbim_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &hs_mbim_in_desc,
	(struct usb_descriptor_header *) &hs_mbim_out_desc,
	NULL,
};

/* super speed support: */

static struct usb_endpoint_descriptor ss_mbim_notify_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes =     USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =   cpu_to_le16(NCM_STATUS_BYTECOUNT),
	.bInterval =        USB_MS_TO_HS_INTERVAL(MBIM_STATUS_INTERVAL_MS),
};

static struct usb_ss_ep_comp_descriptor ss_mbim_intr_comp_desc = {
	.bLength = sizeof(ss_mbim_intr_comp_desc),
	.bDescriptorType =  USB_DT_SS_ENDPOINT_COMP,

	/* the following 3 values can be tweaked if necessary */
	/* .bMaxBurst =     0, */
	/* .bmAttributes =  0, */
	.wBytesPerInterval =    cpu_to_le16(NCM_STATUS_BYTECOUNT),
};


static struct usb_endpoint_descriptor ss_mbim_in_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes =     USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =   cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor ss_mbim_out_desc = {
	.bLength =      USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =  USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes =     USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =   cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_mbim_bulk_comp_desc = {
	.bLength = sizeof(ss_mbim_bulk_comp_desc),
	.bDescriptorType =  USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =     0, */
	/* .bmAttributes =  0, */
};


static struct usb_descriptor_header *mbim_ss_function[] = {
	(struct usb_descriptor_header *) &mbim_iad_desc,
	/* MBIM control descriptors */
	(struct usb_descriptor_header *) &mbim_control_intf,
	(struct usb_descriptor_header *) &mbim_header_desc,
	(struct usb_descriptor_header *) &mbim_union_desc,
	(struct usb_descriptor_header *) &mbim_desc,
	(struct usb_descriptor_header *) &mbim_ext_desc,
	(struct usb_descriptor_header *) &ss_mbim_notify_desc,
	(struct usb_descriptor_header *) &ss_mbim_intr_comp_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &mbim_data_nop_intf,
	(struct usb_descriptor_header *) &mbim_data_intf,
	(struct usb_descriptor_header *) &ss_mbim_in_desc,
	(struct usb_descriptor_header *) &ss_mbim_bulk_comp_desc,
	(struct usb_descriptor_header *) &ss_mbim_out_desc,
	(struct usb_descriptor_header *) &ss_mbim_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

#define STRING_CTRL_IDX 0
#define STRING_DATA_IDX 1

static struct usb_string mbim_string_defs[] = {
	[STRING_CTRL_IDX].s = "MBIM Control",
	[STRING_DATA_IDX].s = "MBIM Data",
	{  } /* end of list */
};

static struct usb_gadget_strings mbim_string_table = {
	.language = 0x0409, /* en-us */
	.strings = mbim_string_defs,
};



static struct usb_gadget_strings *mbim_strings[] = {
	&mbim_string_table,
	NULL,
};


#define MAX_TX_NONFIXED     (512 * 3)

/*
 * Here are options for the Datagram Pointer table (NDP) parser.
 * There are 2 different formats: NDP16 and NDP32 in the spec (ch. 3),
 * in NDP16 offsets and sizes fields are 1 16bit word wide,
 * in NDP32 -- 2 16bit words wide. Also signatures are different.
 * To make the parser code the same, put the differences in the structure,
 * and switch pointers to the structures when the format is changed.
 */

struct mbim_ndp_parser_opts {
	u32		nth_sign;
	u32		ndp_sign;
	unsigned	nth_size;
	unsigned	ndp_size;
	unsigned	ndplen_align;
	/* sizes in u16 units */
	unsigned	dgram_item_len; /* index or length */
	unsigned	block_length;
	unsigned	fp_index;
	unsigned	reserved1;
	unsigned	reserved2;
	unsigned	next_fp_index;
};

#define INIT_NDP16_OPTS {				\
	.nth_sign = USB_CDC_NCM_NTH16_SIGN,		\
	.ndp_sign = USB_CDC_NCM_NDP16_NOCRC_SIGN,	\
	.nth_size = sizeof(struct usb_cdc_ncm_nth16),	\
	.ndp_size = sizeof(struct usb_cdc_ncm_ndp16),	\
	.ndplen_align = 4,				\
	.dgram_item_len = 1,				\
	.block_length = 1,				\
	.fp_index = 1,					\
	.reserved1 = 0,					\
	.reserved2 = 0,					\
	.next_fp_index = 1,				\
}

#define INIT_NDP32_OPTS {				\
	.nth_sign = USB_CDC_NCM_NTH32_SIGN,		\
	.ndp_sign = USB_CDC_NCM_NDP32_NOCRC_SIGN,	\
	.nth_size = sizeof(struct usb_cdc_ncm_nth32),	\
	.ndp_size = sizeof(struct usb_cdc_ncm_ndp32),	\
	.ndplen_align = 8,				\
	.dgram_item_len = 2,				\
	.block_length = 2,				\
	.fp_index = 2,					\
	.reserved1 = 1,					\
	.reserved2 = 2,					\
	.next_fp_index = 2,				\
}


static struct mbim_ndp_parser_opts mbim_ndp16_opts = INIT_NDP16_OPTS;
static struct mbim_ndp_parser_opts mbim_ndp32_opts = INIT_NDP32_OPTS;

static inline void put_mbim(__le16 **p, unsigned size, unsigned val)
{
	switch (size) {
	case 1:
		put_unaligned_le16((u16)val, *p);
		break;
	case 2:
		put_unaligned_le32((u32)val, *p);

		break;
	default:
		BUG();
	}

	*p += size;
}

static inline unsigned get_mbim(__le16 **p, unsigned size)
{
	unsigned tmp;

	switch (size) {
	case 1:
		tmp = get_unaligned_le16(*p);
		break;
	case 2:
		tmp = get_unaligned_le32(*p);
		break;
	default:
		BUG();
	}

	*p += size;
	return tmp;
}

/*
 * NTH16: 16-bit NCM Transfer Header.
 * Ref: NCM v1.0, Table 3-1, p16.
 */
typedef struct {
	u32              signature;     /* Sigature of the NTH16. */
	u16              header_length; /* Size of the NTH16 in bytes. */
	u16              sequence;      /* Sequence number. */
	u16              block_length;  /* Size of the NTB in bytes. */
	u16              fp_index;      /* Offset of the first NDP16 from byte zero of the NTB. */
} nth16_t;
/*
 * NDP16: 16-bit NCM Datagram Pointer.
 * Ref: NCM v1.0, Table 3-3, p18.
 */
typedef struct {
	u16              index;          /* Byte index of the datagram from byte zero of the NTB. */
	u16              length;         /* Byte length of the datagram. */
} ndp16_datagram_t;

typedef struct {
	u32              signature;		/* Signature of this NDP16. */
	u16              length;		/* Size of the NDP16 by bytes.  */
	u16              next_fp_index;  /* Link to next NDP16 in the NTB. */
	ndp16_datagram_t        datagrams[2];
} ndp16_t;

static u32 nth_seq_num;
#include <linux/ip.h>

#ifdef MBIM_MULTI_SESSION
#include <linux/if_vlan.h>
#endif

static struct sk_buff *mbim_wrap_ntb(struct mbim_gether *port,
					struct sk_buff *skb, int ifid)
{
	struct f_mbim   *mbim = func_to_mbim(&port->func);
	struct sk_buff  *skb2;
	/* ncb_len : ncm block header len => nth header + ndp header + pointers to datagrams */
	int     ncb_len = 0;
	__le16      *tmp;
	int     div = ntb_parameters.wNdpInDivisor;
	int     rem = ntb_parameters.wNdpInPayloadRemainder;
	int     pad;
	int     ndp_align = ntb_parameters.wNdpInAlignment;
	int     ndp_pad;
	unsigned    max_size = mbim->ntb_input_size;
	struct mbim_ndp_parser_opts *opts = mbim->parser_opts;
	unsigned    crc_len = mbim->is_crc ? sizeof(uint32_t) : 0;
	__le16      *tmp2 = NULL;
	struct iphdr *iph;
	char *src_addr, *dst_addr;
#ifdef MBIM_MULTI_SESSION
	u16 tci = 0;
	u8 *c = NULL;
	int ret = 0;
#endif
	iph = (struct iphdr *)(skb->data);


	src_addr = (char *) &(iph->saddr);
	dst_addr = (char *) &(iph->daddr);
#if 0
	pr_debug("%s, src_addr = " NIPQUAD_FMT ", dst_addr = " NIPQUAD_FMT "\n",
		__func__, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
#endif
	opts->ndp_sign = cpu_to_le32(USB_CDC_MBIM_NDP16_IPS_SIGN);

	/*
	*c = (u8 *)&opts->ndp_sign;
	*c[3] = ifid;
	*/


	/* add ntp header */
	ncb_len += opts->nth_size;
	ndp_pad = ALIGN(ncb_len, ndp_align) - ncb_len;
	ncb_len += ndp_pad;
	ncb_len += opts->ndp_size;
	ncb_len += 2 * 2 * opts->dgram_item_len; /* Datagram entry : 2 bytes Index, 2 bytes Lengthi */
						 /* for 16bit NDP, dgram_item_len = 1 */
	ncb_len += 2 * 2 * opts->dgram_item_len; /* Zero datagram entry */
	pad = ALIGN(ncb_len, div) + rem - ncb_len;
	ncb_len += pad;

	if (ncb_len + skb->len + crc_len > max_size) {
		dev_kfree_skb_any(skb);
		pr_err("over size");
		goto error;
	}

	if (skb_headroom(skb) < ncb_len || skb_tailroom(skb) < crc_len) {
		pr_debug("skb_copy_expand %d\n", skb_headroom(skb));
		skb2 = skb_copy_expand(skb, ncb_len,
				max_size - skb->len - ncb_len - crc_len,
				GFP_ATOMIC);
		dev_kfree_skb_any(skb);
		if (!skb2) {
			pr_err("skb2 = NULL");
			return NULL;
		}
		skb = skb2;
	}


	tmp = (void *) skb_push(skb, ncb_len);
	tmp2 = tmp;
	memset(tmp, 0, ncb_len);
	/* NTH */
	put_unaligned_le32(opts->nth_sign, tmp); /* NTH dwSignature */

	tmp += 2;
	/* wHeaderLength */
	put_unaligned_le16(opts->nth_size, tmp++);

	/* skip wSequence */
	put_unaligned_le16(nth_seq_num, tmp++);

	nth_seq_num++;

	put_mbim(&tmp, opts->block_length, skb->len); /* (d)wBlockLength */

	/* (d)wFpIndex */
	/* the first pointer is right after the NTH + align */
	put_mbim(&tmp, opts->fp_index, opts->nth_size + ndp_pad);
	tmp = (void *)tmp + ndp_pad;

	/* NDP */
	put_unaligned_le32(opts->ndp_sign, tmp); /* NDP dwSignature */

	tmp += 2;
	/* wLength */
	put_unaligned_le16(ncb_len - opts->nth_size - pad, tmp++);
	tmp++;

	if (mbim->is_crc) {
		uint32_t crc;

		pr_debug("put crc ??\n");
		crc = ~crc32_le(~0,
				skb->data + ncb_len,
				skb->len - ncb_len);
		put_unaligned_le32(crc, skb->data + skb->len);
		skb_put(skb, crc_len);
	}

	/* (d)wDatagramIndex[0] */
	put_mbim(&tmp, opts->dgram_item_len, ncb_len);
	/* (d)wDatagramLength[0] */
	put_mbim(&tmp, opts->dgram_item_len, skb->len - ncb_len);
	/* (d)wDatagramIndex[1] and  (d)wDatagramLength[1] already zeroed */

	if (skb->len > MAX_TX_NONFIXED)
		memset(skb_put(skb, skb_tailroom(skb)),
			   0, skb_tailroom(skb));

	return skb;
error:
	pr_debug("error\n");
	return NULL;
}
#if 0
struct eth_dev {
	/* lock is held while accessing port_usb
	 * or updating its backlink port_usb->ioport
	 */
	spinlock_t      lock;
	struct gether       *port_usb;

	struct net_device   *net;
	struct usb_gadget   *gadget;

	spinlock_t      req_lock;   /* guard {rx,tx}_reqs */
	struct list_head    tx_reqs, rx_reqs;
	atomic_t        tx_qlen;

	struct sk_buff_head rx_frames;

	unsigned        header_len;
	struct sk_buff      *(*wrap)(struct gether *, struct sk_buff *skb);
	int         (*unwrap)(struct gether *,
						struct sk_buff *skb,
						struct sk_buff_head *list);

	struct work_struct  work;

	unsigned long       todo;
#define WORK_RX_MEMORY      0

	bool            zlp;
	u8          host_mac[ETH_ALEN];
};
#endif

/* copy from cdc_mbim.c */
/* this function will pack ip packet into ethernet packet */
/* buf : ip packet */
#ifdef MBIM_MULTI_SESSION
static int cdc_mbim_process_dgram(struct f_mbim  *mbim, struct sk_buff *buf, u16 tci)
#else
static int cdc_mbim_process_dgram(struct f_mbim  *mbim, struct sk_buff *buf)
#endif
{
	__be16 proto = 0;
	struct sk_buff *skb = NULL;
	struct iphdr *iph = (struct iphdr *)(buf->data);
	struct ethhdr *eth;

#ifdef DEBUG
	pr_debug("ip packet len = %x", buf->len);
#endif
	switch (iph->version) {
	case 0x4:
		proto = htons(ETH_P_IP);
#ifdef DEBUG
		pr_debug("ipv4");
#endif
	break;
	case 0x6:
		proto = htons(ETH_P_IPV6);
#ifdef DEBUG
		pr_debug("ipv6");
#endif
		break;
	default:
#ifdef DEBUG
		pr_err("unknown ip version %d", iph->version);
#endif
		goto err;
	}
	/* add an ethernet header */
	/* mt5303: The u_ether.c is modified to reserve the space for ethernet header */
	skb = (struct sk_buff *) buf;

	/*move the header potiner to allocate the location with ETH_HLEN length*/
	eth = (struct ethhdr *) skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);
	eth->h_proto = proto;

	return 0;
err:
	pr_err("cdc_mbim_process_dgram error");
	return -EINVAL;
}

static int mbim_unwrap_ntb(struct mbim_gether *port,
						  struct sk_buff *skb,
						  struct sk_buff_head *list)
{
	struct f_mbim    *mbim = func_to_mbim(&port->func);
	__le16          *tmp = (void *) skb->data;
	unsigned        index, index2;
	unsigned        dg_len, dg_len2;
	unsigned        ndp_len;
	struct sk_buff  *skb2;
	int             ret = -EINVAL;
	unsigned        max_size = le32_to_cpu(ntb_parameters.dwNtbOutMaxSize);
	const struct mbim_ndp_parser_opts *opts = mbim->parser_opts;
	unsigned        crc_len = mbim->is_crc ? sizeof(uint32_t) : 0;
	int             dgram_counter;

	ndp16_t *ndp;
	struct iphdr *iph;
	struct usb_cdc_ncm_ndp16 *ndp16 = NULL;
#ifdef MBIM_MULTI_SESSION
	u8 *c = NULL;
	u16 tci;
#endif

	/* dwSignature */
	if (get_unaligned_le32(tmp) != opts->nth_sign) {
		pr_notice("Wrong NTH SIGN, skblen %d\n",
				skb->len);
		print_hex_dump(KERN_INFO, "HEAD:", DUMP_PREFIX_ADDRESS, 32, 1,
				skb->data, 32, false);

		goto err;
	}
	tmp += 2;
	/* wHeaderLength */
	if (get_unaligned_le16(tmp++) != opts->nth_size) {
		pr_notice("Wrong NTB headersize\n");
		goto err;
	}

	tmp++; /* skip wSequence */

	/* (d)wBlockLength */
	if (get_mbim(&tmp, opts->block_length) > max_size) {
		pr_notice("OUT size exceeded\n");
		goto err;
	}

	index = get_mbim(&tmp, opts->fp_index);
	/* NCM 3.2 */
	if (((index % 4) != 0) && (index < opts->nth_size)) {
		pr_notice("Bad index: %x\n",
				index);
		goto err;
	}

	ndp = ((void *)skb->data) + index;
	iph = (struct iphdr *)(skb->data+(ndp->datagrams[0].index));
#if 0
	pr_debug("%s, src_addr = " NIPQUAD_FMT ", dst_addr = " NIPQUAD_FMT "\n",
		__func__, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
#endif
	/* walk through NDP */
	tmp = ((void *)skb->data) + index;
#if 0
	if (get_unaligned_le32(tmp) != mbim->ndp_sign) {
		INFO(port->func.config->cdev, "Wrong NDP SIGN\n");
		goto err;
	}
#else

	ndp16 = (struct usb_cdc_ncm_ndp16 *)tmp;
	switch (ndp16->dwSignature & cpu_to_le32(0x00ffffff)) {
	case USB_CDC_MBIM_NDP16_IPS_SIGN:
		pr_debug("USB_CDC_MBIM_NDP16_IPS_SIGN\n");
#ifdef MBIM_MULTI_SESSION
		c = (u8 *)&ndp16->dwSignature;
		for (i = 0; i < MAX_SESSION_NUM; i++) {
			if (mbim->ethid_to_sid_tbl[i] == c[3]) {
				tci = i;
				break;
			}
		}
		if (i == MAX_SESSION_NUM) {
			pr_debug("i(%d) == MAX_SESSION_NUM(%d)\n", i, MAX_SESSION_NUM);
			goto err;
		}
#endif

		break;
	case USB_CDC_MBIM_NDP16_DSS_SIGN:
#ifdef MBIM_MULTI_SESSION
		c = (u8 *)&ndp16->dwSignature;
		tci = c[3] + 256;
		break;
#else
		pr_err("received unsupported NDP DSS SIGN\n");
		goto err;
#endif
	default:
		pr_err("Wrong NDP SIGN\n");
		goto err;
	}
#endif

	tmp += 2;

	ndp_len = get_unaligned_le16(tmp++);
	/*
	 * NCM 3.3.1
	 * entry is 2 items
	 * item size is 16/32 bits, opts->dgram_item_len * 2 bytes
	 * minimal: struct usb_cdc_ncm_ndpX + normal entry + zero entry
	 */
	if ((ndp_len < opts->ndp_size + 2 * 2 * (opts->dgram_item_len * 2))
			|| (ndp_len % opts->ndplen_align != 0)) {
		pr_err("Bad NDP length: %x\n", ndp_len);
		goto err;
	}
	tmp += opts->reserved1;
	tmp += opts->next_fp_index; /* skip reserved (d)wNextFpIndex */
	tmp += opts->reserved2;

	ndp_len -= opts->ndp_size;
	index2 = get_mbim(&tmp, opts->dgram_item_len);
	dg_len2 = get_mbim(&tmp, opts->dgram_item_len);
	dgram_counter = 0;

	do {
		index = index2;
		dg_len = dg_len2;
		if (dg_len < 14 + crc_len) { /* ethernet header + crc */
			pr_err("Bad dgram length: %x\n",
					dg_len);
			goto err;
		}
		if (mbim->is_crc) {
			uint32_t crc, crc2;

			pr_debug("has crc\n");
			crc = get_unaligned_le32(skb->data +
					index + dg_len - crc_len);
			crc2 = ~crc32_le(~0,
					skb->data + index,
					dg_len - crc_len);
			if (crc != crc2) {
				pr_err("Bad CRC\n");
				goto err;
			}
		}
		index2 = get_mbim(&tmp, opts->dgram_item_len);
		dg_len2 = get_mbim(&tmp, opts->dgram_item_len);

		if (index2 == 0 || dg_len2 == 0) {
			skb2 = skb;
		} else {
			skb2 = skb_clone(skb, GFP_ATOMIC);
			if (skb2 == NULL) {
				pr_notice("clone error\n");
				goto err;
			}
		}

		/* NDP is located behind datagram */
		if (!skb_pull(skb2, index)) {
			ret = -EOVERFLOW;
			pr_notice("skb_pull error\n");
			goto err;
		}

		skb_trim(skb2, dg_len - crc_len);

#ifdef MBIM_MULTI_SESSION
		ret = cdc_mbim_process_dgram(mbim, skb2, tci); /*add the Ethernet header*/
#else
		ret = cdc_mbim_process_dgram(mbim, skb2); /*add the Ethernet header*/
#endif
		if (ret)
			goto err;

		skb_queue_tail(list, skb2);

		ndp_len -= 2 * (opts->dgram_item_len * 2);

		dgram_counter++;

		if (index2 == 0 || dg_len2 == 0)
			break;
	} while (ndp_len > 2 * (opts->dgram_item_len * 2)); /* zero entry */

	return 0;
err:
	skb_queue_purge(list);
	dev_kfree_skb_any(skb);
	pr_debug("mbim_unwrap_ntb error\n");
	return ret;
}


static inline int mbim_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1)
		return 0;

	atomic_dec(excl);
	return -EBUSY;
}

static inline void mbim_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static struct ctrl_pkt *mbim_alloc_ctrl_pkt(unsigned len, gfp_t flags)
{
	struct ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

#if defined(CONFIG_64BIT) && defined(CONFIG_MTK_LM_MODE)
	pkt->buf = kmalloc(len, flags | GFP_DMA);
#else
	pkt->buf = kmalloc(len, flags);
#endif
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void mbim_free_ctrl_pkt(struct ctrl_pkt *pkt)
{
	if (pkt) {
		kfree(pkt->buf);
		kfree(pkt);
	}
}

static struct usb_request *mbim_alloc_req(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_ATOMIC);

	if (!req)
		return NULL;

#if defined(CONFIG_64BIT) && defined(CONFIG_MTK_LM_MODE)
	req->buf = kmalloc(buffer_size, GFP_ATOMIC | GFP_DMA);
#else
	req->buf = kmalloc(buffer_size, GFP_ATOMIC);
#endif
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}
	req->length = buffer_size;
	return req;
}

void fmbim_free_req(struct usb_ep *ep, struct usb_request *req)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static void fmbim_ctrl_response_available(struct f_mbim *dev)
{
	struct usb_request      *req = dev->not_port.notify_req;
	struct usb_cdc_notification *event = NULL;
	unsigned long           flags;
	int             ret;

	pr_debug("dev:%pK portno#%d\n", dev, dev->port_num);

	spin_lock_irqsave(&dev->lock, flags);

	if (!atomic_read(&dev->online)) {
		pr_err("dev:%pK is not online\n", dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (!req) {
		pr_err("dev:%pK req is NULL\n", dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (!req->buf) {
		pr_err("dev:%pK req->buf is NULL\n", dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (atomic_inc_return(&dev->not_port.notify_count) != 1) {
		pr_debug("delay ep_queue: notifications queue is busy[%d]",
				atomic_read(&dev->not_port.notify_count));
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}


	req->length = sizeof(*event);
	event = req->buf;
	event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	event->bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
	event->wValue = cpu_to_le16(0);
	event->wIndex = cpu_to_le16(dev->ctrl_id);
	event->wLength = cpu_to_le16(0);
	spin_unlock_irqrestore(&dev->lock, flags);

	ret = usb_ep_queue(dev->not_port.notify,
			   req, GFP_ATOMIC);
	if (ret) {
		atomic_dec(&dev->not_port.notify_count);
		pr_err("ep enqueue error %d\n", ret);
	}

	pr_debug("Succcessfull Exit");
}

static int
fmbim_send_cpkt_response(struct f_mbim *gr, struct ctrl_pkt *cpkt)
{
	struct f_mbim   *dev = gr;
	unsigned long   flags;

	if (!gr || !cpkt) {
		pr_err("Invalid cpkt, dev:%pK cpkt:%pK\n",
				gr, cpkt);
		return -ENODEV;
	}

	pr_debug("dev:%pK port_num#%d\n", dev, dev->port_num);

	if (!atomic_read(&dev->online)) {
		pr_err("dev:%pK is not connected\n", dev);
		mbim_free_ctrl_pkt(cpkt);
		return 0;
	}

	if (dev->not_port.notify_state != NCM_NOTIFY_RESPONSE_AVAILABLE) {
		pr_err("dev:%pK state=%d, recover!!\n", dev,
				dev->not_port.notify_state);
		mbim_free_ctrl_pkt(cpkt);
		return 0;
	}

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&cpkt->list, &dev->cpkt_resp_q);
	atomic_inc(&dev->resp_count);
	spin_unlock_irqrestore(&dev->lock, flags);

	fmbim_ctrl_response_available(dev);

	return 0;
}

static void mbim_reset_function_queue(struct f_mbim *dev)
{
	struct ctrl_pkt *cpkt = NULL;
	unsigned long flags;

	pr_debug("Queue empty packet for QBI");

	spin_lock_irqsave(&dev->lock, flags);
	if (!dev->is_open) {
		pr_err("%s: mbim file handler %pK is not open", __func__, dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	cpkt = mbim_alloc_ctrl_pkt(0, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("%s: Unable to allocate reset function pkt\n", __func__);
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	list_add_tail(&cpkt->list, &dev->cpkt_req_q);
	spin_unlock_irqrestore(&dev->lock, flags);

	pr_debug("%s: Wake up read queue", __func__);
	wake_up(&dev->read_wq);
}

/* should clear mbim->not_port.notify_count before call this function */
static void mbim_clear_queues(struct f_mbim *mbim)
{
	struct ctrl_pkt *cpkt = NULL;
	struct list_head *act, *tmp;
	unsigned long flags;

	pr_debug("mbim_clear_queues");

	spin_lock_irqsave(&mbim->lock, flags);
	list_for_each_safe(act, tmp, &mbim->cpkt_req_q) {
		cpkt = list_entry(act, struct ctrl_pkt, list);
		list_del(&cpkt->list);
		mbim_free_ctrl_pkt(cpkt);
	}
	list_for_each_safe(act, tmp, &mbim->cpkt_resp_q) {
		cpkt = list_entry(act, struct ctrl_pkt, list);
		list_del(&cpkt->list);
		mbim_free_ctrl_pkt(cpkt);
	}
	spin_unlock_irqrestore(&mbim->lock, flags);
}

static void mbim_reset_response(struct f_mbim *mbim)
{
	int status = 0;

	pr_debug("mbim_reset_notify");

	atomic_set(&mbim->not_port.notify_count, 0);
	atomic_set(&mbim->resp_count, 0);
	mbim_clear_queues(mbim);
	mbim_reset_function_queue(mbim);
#if 1
	if (atomic_read(&mbim->not_port.notify_count) > 0) {
		status = usb_ep_disable(mbim->not_port.notify);
		if (status < 0)
			pr_err("%s: usb_ep_disable failed %d", __func__, status);

		status = usb_ep_dequeue(mbim->not_port.notify, mbim->not_port.notify_req);
		if (status < 0)
			pr_err("%s: usb_ep_dequeue failed %d", __func__, status);

		status = usb_ep_enable(mbim->not_port.notify);
		if (status < 0)
			pr_err("%s: usb_ep_enable failed %d", __func__, status);
	} else {
		/* event the notify_count is zero, there may still have data in queue */
		mbim_clear_queues(mbim);
	}
	/* usb_ep_dequeue should invoke ECONNRESET in mbim_notify_complete
		and cause mbim_clear_queues and mbim_reset_response */
#endif


}

static inline void mbim_reset_values(struct f_mbim *mbim)
{
	mbim->parser_opts = &mbim_ndp16_opts;

	mbim->ntb_input_size = NTB_DEFAULT_IN_SIZE;
#if 1
	mbim->gether_port.fixed_out_len = le32_to_cpu(ntb_parameters.dwNtbOutMaxSize);
	mbim->gether_port.fixed_in_len = NTB_DEFAULT_IN_SIZE;
#endif
	atomic_set(&mbim->online, 0);
	atomic_set(&mbim->req_count, 0);

	mbim_reset_response(mbim);
}



static void fmbim_reset_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim       *dev = req->context;

	pr_debug("fmbim_reset_cmd_complete");
	mbim_reset_response(dev);
}


/*
 * Context: mbim->lock held
 */
static void mbim_do_notify(struct f_mbim *mbim, unsigned long *flags)
{
	struct usb_request      *req = mbim->not_port.notify_req;
	struct usb_cdc_notification *event;
	int             status;

	pr_debug("notify_state: %d", mbim->not_port.notify_state);

	if (!req)
		return;

	pr_debug("notify count = %d", atomic_read(&mbim->not_port.notify_count));
	event = req->buf;

	switch (mbim->not_port.notify_state) {
	case NCM_NOTIFY_NONE:
		if (atomic_read(&mbim->not_port.notify_count) > 0)
			pr_err("Pending notifications in NCM_NOTIFY_NONE\n");
		else
			pr_debug("No pending notifications\n");

		return;

	case NCM_NOTIFY_RESPONSE_AVAILABLE:
		pr_debug("Notification %02x sent\n", event->bNotificationType);

		if (atomic_read(&mbim->not_port.notify_count) <= 0) {
			pr_debug("notify_response_available: done");
			return;
		}

		spin_unlock_irqrestore(&mbim->lock, *flags);
		status = usb_ep_queue(mbim->not_port.notify, req, GFP_ATOMIC);
		spin_lock_irqsave(&mbim->lock, *flags);
		if (status) {
			atomic_dec(&mbim->not_port.notify_count);
			pr_err("Queue notify request failed, err: %d", status);
		}

		return;

	}
	event->bmRequestType = 0xA1;
	event->wIndex = cpu_to_le16(mbim->ctrl_id);

	/*
	 * In double buffering if there is a space in FIFO,
	 * completion callback can be called right after the call,
	 * so unlocking
	 */
	atomic_inc(&mbim->not_port.notify_count);
	spin_unlock_irqrestore(&mbim->lock, *flags);
	status = usb_ep_queue(mbim->not_port.notify, req, GFP_ATOMIC);
	spin_lock_irqsave(&mbim->lock, *flags);
	if (status) {
		atomic_dec(&mbim->not_port.notify_count);
		pr_err("usb_ep_queue failed, err: %d", status);
	}
}

static void mbim_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim           *mbim = req->context;
	struct usb_cdc_notification *event = req->buf;
	unsigned long flags;

	pr_debug("dev:%pK\n", mbim);

	spin_lock_irqsave(&mbim->lock, flags);
	switch (req->status) {
	case 0:
		pr_debug("Notification %02x sent\n",
			event->bNotificationType);

		atomic_dec(&mbim->not_port.notify_count);
		pr_debug("notify_count = %d",
				atomic_read(&mbim->not_port.notify_count));
		break;

	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		mbim->not_port.notify_state = NCM_NOTIFY_NONE;
		atomic_set(&mbim->not_port.notify_count, 0);
		pr_err("ESHUTDOWN/ECONNRESET, connection gone");
		spin_unlock_irqrestore(&mbim->lock, flags);
		mbim_clear_queues(mbim);
		mbim_reset_function_queue(mbim);
		spin_lock_irqsave(&mbim->lock, flags);
		break;
	default:
		pr_err("Unknown event %02x --> %d\n",
			event->bNotificationType, req->status);
		break;
	}

	mbim_do_notify(mbim, &flags);

	spin_unlock_irqrestore(&mbim->lock, flags);

	pr_debug("dev:%pK Exit\n", mbim);
}

static void mbim_ep0out_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* now for SET_NTB_INPUT_SIZE only */
	unsigned        in_size = 0;
	struct usb_function *f = req->context;
	struct f_mbim       *mbim = func_to_mbim(f);
	struct mbim_ntb_input_size *ntb = NULL;

	pr_debug("dev:%pK\n", mbim);

	req->context = NULL;
	if (req->status || req->actual != req->length) {
		pr_err("Bad control-OUT transfer\n");
		goto invalid;
	}

	if (req->length == 4) {
		in_size = get_unaligned_le32(req->buf);
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
			in_size > le32_to_cpu(ntb_parameters.dwNtbInMaxSize)) {
			pr_err("Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
	} else if (req->length == 8) {
		ntb = (struct mbim_ntb_input_size *)req->buf;
		in_size = get_unaligned_le32(&(ntb->ntb_input_size));
		if (in_size < USB_CDC_NCM_NTB_MIN_IN_SIZE ||
			in_size > le32_to_cpu(ntb_parameters.dwNtbInMaxSize)) {
			pr_err("Illegal INPUT SIZE (%d) from host\n", in_size);
			goto invalid;
		}
		mbim->ntb_max_datagrams =
			get_unaligned_le16(&(ntb->ntb_max_datagrams));
	} else {
		pr_err("Illegal NTB length %d\n", in_size);
		goto invalid;
	}

	pr_debug("Set NTB INPUT SIZE %d\n", in_size);

	mbim->ntb_input_size = in_size;
#if 1
	mbim->gether_port.fixed_in_len = in_size;
#endif
	return;

invalid:
	usb_ep_set_halt(ep);

	pr_err("dev:%pK Failed\n", mbim);

}

static void
fmbim_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mbim       *dev = req->context;
	struct ctrl_pkt     *cpkt = NULL;
	int         len = req->actual;
	unsigned long flags;
	u8 *data = NULL;

	if (!dev) {
		pr_err("mbim dev is null\n");
		return;
	}

	if (req->status < 0) {
		pr_err("mbim command error %d\n", req->status);
		return;
	}

	pr_debug("dev:%pK port#%d\n", dev, dev->port_num);

	cpkt = mbim_alloc_ctrl_pkt(len, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("Unable to allocate ctrl pkt\n");
		return;
	}

	pr_debug("Add to cpkt_req_q packet with len = %d\n", len);
	memcpy(cpkt->buf, req->buf, len);
	spin_lock_irqsave(&dev->lock, flags);
	data = cpkt->buf;
	list_add_tail(&cpkt->list, &dev->cpkt_req_q);
	atomic_inc(&dev->req_count);

	spin_unlock_irqrestore(&dev->lock, flags);

	/* wakeup read thread */
	pr_debug("Wake up read queue");
	wake_up(&dev->read_wq);
}

static int
mbim_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_mbim           *mbim = func_to_mbim(f);
	struct usb_composite_dev    *cdev = mbim->cdev;
	struct usb_request      *req = cdev->req;
	struct ctrl_pkt     *cpkt = NULL;
	int value = -EOPNOTSUPP;
	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	u16 w_length = le16_to_cpu(ctrl->wLength);

	unsigned char *net_addr = NULL;
	unsigned long flags;

	/*
	 * composite driver infrastructure handles everything except
	 * CDC class messages; interface activation uses set_alt().
	 */

	if (!atomic_read(&mbim->online)) {
		pr_err("usb cable is not connected\n");
		return -ENOTCONN;
	}

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE) << 8)
		| USB_REQ_GET_DESCRIPTOR:
			pr_debug("USB_REQ_GET_DESCRIPTOR");
		break;
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_RESET_FUNCTION:

		value = 0;
		req->complete = fmbim_reset_cmd_complete;
		req->context = mbim;
		break;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SEND_ENCAPSULATED_COMMAND:

		value = w_length;
		req->complete = fmbim_cmd_complete;
		req->context = mbim;
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_ENCAPSULATED_RESPONSE:

		if (w_value) {
			pr_err("w_length > 0: %d", w_length);
			break;
		}

		spin_lock_irqsave(&mbim->lock, flags);
		if (list_empty(&mbim->cpkt_resp_q)) {
			pr_err("ctrl resp queue empty\n");
			spin_unlock_irqrestore(&mbim->lock, flags);
			break;
		}

		cpkt = list_first_entry(&mbim->cpkt_resp_q,
					struct ctrl_pkt, list);
		list_del(&cpkt->list);
		spin_unlock_irqrestore(&mbim->lock, flags);

		value = min_t(unsigned, w_length, cpkt->len);
		memcpy(req->buf, cpkt->buf, value);
		mbim_free_ctrl_pkt(cpkt);

		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_PARAMETERS:

		if (w_length == 0 || w_value != 0 || w_index != mbim->ctrl_id)
			break;

		value = w_length > sizeof(ntb_parameters) ?
			sizeof(ntb_parameters) : w_length;
		memcpy(req->buf, &ntb_parameters, value);
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_INPUT_SIZE:

		if (w_length < 4 || w_value != 0 || w_index != mbim->ctrl_id)
			break;

		put_unaligned_le32(mbim->ntb_input_size, req->buf);
		value = 4;
		break;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SET_NTB_INPUT_SIZE:

		if (w_length != 4 && w_length != 8) {
			pr_err("wrong NTB length %d", w_length);
			break;
		}

		if (w_value != 0 || w_index != mbim->ctrl_id)
			break;

		req->complete = mbim_ep0out_complete;
		req->length = w_length;
		req->context = f;

		value = req->length;
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NTB_FORMAT:
	{
		uint16_t format;


		if (w_length < 2 || w_value != 0 || w_index != mbim->ctrl_id)
			break;

		format = (mbim->parser_opts == &mbim_ndp16_opts) ? 0x0000 : 0x0001;
		put_unaligned_le16(format, req->buf);
		value = 2;
		break;
	}

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SET_NTB_FORMAT:
	{
		if (w_length != 0 || w_index != mbim->ctrl_id)
			break;
		switch (w_value) {
		case 0x0000:
			mbim->parser_opts = &mbim_ndp16_opts;
			pr_debug("NCM16 selected\n");
			break;
		case 0x0001:
			mbim->parser_opts = &mbim_ndp32_opts;
			pr_debug("NCM32 selected\n");
			break;
		default:
			break;
		}
		value = 0;
		break;
	}

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_SET_NET_ADDRESS:
	{
		if (w_length != 6 || w_value != 0)
			break;
		net_addr = mbim->host_addr;
		memcpy(net_addr, req->buf, 6);
		value = 0;
		break;
	}

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| USB_CDC_GET_NET_ADDRESS:
	{
		net_addr = mbim->host_addr;
		value = 6;
		memcpy(req->buf, net_addr, value);
		break;
	}

	/* optional in mbim descriptor: */
	/* case USB_CDC_GET_MAX_DATAGRAM_SIZE: */
	/* case USB_CDC_SET_MAX_DATAGRAM_SIZE: */

	default:
	pr_err("invalid control req: %02x.%02x v%04x i%04x l%d\n",
		ctrl->bRequestType, ctrl->bRequest,
		w_value, w_index, w_length);
	}

	 /* respond with data transfer or status phase? */
	if (value >= 0) {
		if ((value % cdev->gadget->ep0->maxpacket) == 0) {
			/* zero end packet */
			if (value < w_length)
				value++;
		} else {
			req->zero = 0;
		}
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0) {
			pr_err("queueing req failed: %02x.%02x, err %d\n",
				ctrl->bRequestType,
				   ctrl->bRequest, value);
		}
	} else {
		pr_err("ctrl req err %d: %02x.%02x v%04x i%04x l%d\n",
			value, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int mbim_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_mbim       *mbim = func_to_mbim(f);
	struct usb_composite_dev *cdev = mbim->cdev;
	int ret = 0;
	unsigned long flags;

	/* Control interface has only altsetting 0 */
	if (intf == mbim->ctrl_id) {

		pr_debug("CONTROL_INTERFACE\n");

		if (alt != 0)
			goto fail;

		if (mbim->not_port.notify->driver_data) {
			pr_debug("reset mbim control %d\n", intf);
			usb_ep_disable(mbim->not_port.notify);
		}
#if 1
		if (!mbim->not_port.notify->desc) {
			if (config_ep_by_speed(cdev->gadget, f,
					mbim->not_port.notify)) {
				mbim->not_port.notify->desc = NULL;
				pr_err("Failed configuring notify ep %s: err %d\n",
						mbim->not_port.notify->name, ret);
				return ret;
			}
		}
#else
		mbim->not_port.desc = ep_choose(cdev->gadget,
				mbim->hs.notify,
				mbim->fs.notify);
#endif

		ret = usb_ep_enable(mbim->not_port.notify);
		if (ret) {
			pr_err("usb ep#%s enable failed, err#%d\n",
				mbim->not_port.notify->name, ret);
			return ret;
		}
		mbim->not_port.notify->driver_data = mbim;

	/* Data interface has two altsettings, 0 and 1 */
	} else if (intf == mbim->data_id) {

		pr_debug("DATA_INTERFACE");

		if (alt > 1)
			goto fail;

#if 1
		if (mbim->gether_port.in_ep->driver_data) {
			pr_debug("reset mbim\n");

			mbim_reset_values(mbim);
			mbim_disconnect(&mbim->gether_port);
		}
#endif
		/*
		 * CDC Network only sends data in non-default altsettings.
		 * Changing altsettings resets filters, statistics, etc.
		 */
		if (alt == 1) {
			pr_debug("Alt set 1, initialize ports");
			if (!(mbim->gether_port.in_ep->desc) || !(mbim->gether_port.out_ep->desc)) {
				pr_debug("Choose endpoints descriptor");
				ret = config_ep_by_speed(cdev->gadget, f,
						mbim->gether_port.in_ep);
				if (ret) {
					mbim->gether_port.in_ep->desc = NULL;
					pr_err("IN ep %s failed: %d\n",
							mbim->gether_port.in_ep->name, ret);
					return ret;
				}


				pr_debug("Set mbim port in_desc = 0x%pK",
						mbim->gether_port.in_ep->desc);

				ret = config_ep_by_speed(cdev->gadget, f,
						mbim->gether_port.out_ep);
				if (ret) {
					mbim->gether_port.in_ep->desc = NULL;
					mbim->gether_port.out_ep->desc = NULL;
					pr_err("OUT ep %s failed: %d\n",
							mbim->gether_port.out_ep->name, ret);
					return ret;
				}
				pr_debug("Set mbim port out_desc = 0x%pK",
						mbim->gether_port.out_ep->desc);
				pr_debug("Activate mbim\n");
				mbim->net = mbim_connect(&mbim->gether_port);
				if (IS_ERR(mbim->net)) {
					mbim->net = NULL;
					pr_err("gether_connect failed\n");
					return PTR_ERR(mbim->net);
				}


			} else {
				pr_debug("PORTS already SET");
			}
		}

		mbim->data_alt_int = alt;
		spin_lock_irqsave(&mbim->lock, flags);
		mbim->not_port.notify_state = NCM_NOTIFY_RESPONSE_AVAILABLE;
		spin_unlock_irqrestore(&mbim->lock, flags);
	} else {
		goto fail;
	}

	atomic_set(&mbim->online, 1);

	pr_debug("SET DEVICE ONLINE");

	/* wakeup file threads */
	wake_up(&mbim->read_wq);
	wake_up(&mbim->write_wq);

	return 0;

fail:
	pr_err("ERROR: Illegal Interface");
	return -EINVAL;
}

/*
 * Because the data interface supports multiple altsettings,
 * this MBIM function *MUST* implement a get_alt() method.
 */
static int mbim_get_alt(struct usb_function *f, unsigned intf)
{
	struct f_mbim   *mbim = func_to_mbim(f);

	if (intf == mbim->ctrl_id)
		return 0;
	else if (intf == mbim->data_id)
		return mbim->data_alt_int;

	return -EINVAL;
}

static void mbim_disable(struct usb_function *f)
{
	struct f_mbim   *mbim = func_to_mbim(f);

	pr_debug("SET DEVICE OFFLINE");
	atomic_set(&mbim->online, 0);

	mbim->not_port.notify_state = NCM_NOTIFY_NONE;

	atomic_set(&mbim->not_port.notify_count, 0);
	mbim_clear_queues(mbim);
	mbim_reset_function_queue(mbim);

	mbim_disconnect(&mbim->gether_port);

	if (mbim->not_port.notify->driver_data) {
		usb_ep_disable(mbim->not_port.notify);
		mbim->not_port.notify->driver_data = NULL;
	}

	pr_debug("mbim deactivated\n");
}


/*-------------------------------------------------------------------------*/

/*
 * Callbacks let us notify the host about connect/disconnect when the
 * net device is opened or closed.
 *
 * For testing, note that link states on this side include both opened
 * and closed variants of:
 *
 *   - disconnected/unconfigured
 *   - configured but inactive (data alt 0)
 *   - configured and active (data alt 1)
 *
 * Each needs to be tested with unplug, rmmod, SET_CONFIGURATION, and
 * SET_INTERFACE (altsetting).  Remember also that "configured" doesn't
 * imply the host is actually polling the notification endpoint, and
 * likewise that "active" doesn't imply it's actually using the data
 * endpoints for traffic.
 */

typedef enum {
	/* Data Path Event */
	MBIM_SIG_DATA_PATH_OPENED,
	MBIM_SIG_DATA_PATH_CLOSED,
	  MBIM_SIG_DUMMY = 0xff /* 1bytes */
} mbim_signal_e;

/* Data Path Event Parameter */
typedef struct {
	u8 session_id;
} mbim_sig_dpe_param_t;

/* SI_PAD_SIZE is 29 bytes */
#define MBIM_SIGNAL_PARAM_SIZE 7
typedef struct {
	u8 sig_id;    /* sender's pid */
	u8 param[MBIM_SIGNAL_PARAM_SIZE];   /* sender's uid */
} mbim_siginfo_t;

#define NETIF0_OPEN_SIGNAL  SIGUSR2
#define NETIF0_CLOSE_SIGNAL SIGUSR1



int mbim_daemon_signal(mbim_signal_e sig_num, u8 *param, int param_len)
#ifdef MBIM_EVENT_BY_FILE
{
	struct ctrl_pkt     *cpkt = NULL;
	int len = sizeof(mbim_siginfo_t);
	mbim_siginfo_t mbim_siginfo;
	struct f_mbim *dev = _mbim_dev;
	unsigned long flags;

	if (param_len > MBIM_SIGNAL_PARAM_SIZE) {
		pr_err("param_len (%d) > MBIM_SIGNAL_PARAM_SIZE (%d)", param_len, MBIM_SIGNAL_PARAM_SIZE);
		return -ENODEV;
	}

	memcpy(mbim_siginfo.param, param, param_len);

	mbim_siginfo.sig_id = sig_num;

	spin_lock_irqsave(&dev->lock, flags);

	cpkt = mbim_alloc_ctrl_pkt(len, GFP_ATOMIC);
	if (!cpkt) {
		pr_err("Unable to allocate ctrl pkt\n");
		spin_unlock_irqrestore(&dev->lock, flags);
		return -ENODEV;
	}

	pr_debug("Add to cpkt_event_q packet with len = %d\n", len);
	memcpy(cpkt->buf, &mbim_siginfo, len);
	list_add_tail(&cpkt->list, &dev->cpkt_event_q);
	spin_unlock_irqrestore(&dev->lock, flags);

	/* wakeup read thread */
	pr_debug("Wake up event queue");
	wake_up(&dev->event_read_wq);
	return 0;
}
#else
{
	int ret;
	siginfo_t info;
	struct task_struct *t;
	int pid = _mbim_dev->user_pid;
	int send_signal = 0;
	mbim_siginfo_t *mbim_siginfo = (mbim_siginfo_t *) &(info._sifields._pad[0]);

	memset(&info, 0, sizeof(struct siginfo));
	info.si_code = SI_KERNEL;
	mbim_siginfo->sig_id = sig_num;
	pr_notice("enter");
	if (param_len > MBIM_SIGNAL_PARAM_SIZE) {
		pr_err("param_len (%d) > MBIM_SIGNAL_PARAM_SIZE (%d)", param_len, MBIM_SIGNAL_PARAM_SIZE);
		return -ENODEV;
	}

	memcpy(mbim_siginfo->param, param, param_len);

	if (!_mbim_dev->user_pid) {
		pr_err("MBIM Daemon pid is not set");
		return -ENODEV;
	}

	t = pid_task(find_vpid(pid), PIDTYPE_PID);
	if (t == NULL) {
		pr_err("Can't find task by pid %d", pid);
		return -ENODEV;
	}

	if (sig_num == MBIM_SIG_DATA_PATH_OPENED)
		send_signal = NETIF0_OPEN_SIGNAL;
	else if (sig_num == MBIM_SIG_DATA_PATH_CLOSED)
		send_signal = NETIF0_CLOSE_SIGNAL;
	else
		return -ENODEV;

	ret = kill_pid(find_vpid(pid), send_signal, 1);
	if (ret < 0) {
		pr_debug("[Failed %d]Sent signal %d with event %d to MBIM Daemon pid %d",
			ret, send_signal, sig_num, pid);
		return -ENODEV;
	}

	pr_debug("[Done]Sent signal %d with event %d to MBIM Daemon pid %d\n",
		send_signal, sig_num, pid);

	return 0;

}
#endif /* MBIM_EVENT_BY_FILE */
static void mbim_linkup_work(struct work_struct *work)
{
	/* data path event param */
	mbim_sig_dpe_param_t sig_param;

	pr_debug("enter");

	sig_param.session_id = 0;
	mbim_daemon_signal(MBIM_SIG_DATA_PATH_OPENED, (u8 *)&sig_param, sizeof(mbim_sig_dpe_param_t));
}

static void mbim_linkdown_work(struct work_struct *work)
{
	/* data path event param */
	mbim_sig_dpe_param_t sig_param;

	pr_debug("enter");

	sig_param.session_id = 0;
	mbim_daemon_signal(MBIM_SIG_DATA_PATH_CLOSED, (u8 *)&sig_param, sizeof(mbim_sig_dpe_param_t));

}

static void mbim_gether_open(struct mbim_gether *geth)
{
	pr_debug("mbim_gether_open\n");
}

static void mbim_gether_close(struct mbim_gether *geth)
{
	pr_debug("mbim_gether_close\n");
}

static int
mbim_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_mbim       *mbim = func_to_mbim(f);
	int         status;
	struct usb_ep       *ep;

	pr_debug("Enter");

	mbim->cdev = cdev;

	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	mbim->ctrl_id = status;
	mbim_iad_desc.bFirstInterface = status;

	mbim_control_intf.bInterfaceNumber = status;
	mbim_union_desc.bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	mbim->data_id = status;
	mbim->data_alt_int = 0;

	mbim_data_nop_intf.bInterfaceNumber = status;
	mbim_data_intf.bInterfaceNumber = status;
	mbim_union_desc.bSlaveInterface0 = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_in_desc);
	if (!ep) {
		pr_err("usb epin autoconfig failed\n");
		goto fail;
	}
	pr_debug("usb epin autoconfig succeeded\n");
	ep->driver_data = cdev; /* claim */
	mbim->gether_port.in_ep = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_out_desc);
	if (!ep) {
		pr_err("usb epout autoconfig failed\n");
		goto fail;
	}
	pr_debug("usb epout autoconfig succeeded\n");
	ep->driver_data = cdev; /* claim */
	mbim->gether_port.out_ep = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_mbim_notify_desc);
	if (!ep) {
		pr_err("usb notify ep autoconfig failed\n");
		goto fail;
	}

	pr_debug("usb notify ep autoconfig succeeded\n");
	mbim->not_port.notify = ep;
	ep->driver_data = cdev; /* claim */

	status = -ENOMEM;

	/* allocate notification request and buffer */
	mbim->not_port.notify_req = mbim_alloc_req(ep, NCM_STATUS_BYTECOUNT);
	if (!mbim->not_port.notify_req) {
		pr_debug("failed to allocate notify request\n");
		goto fail;
	}
	pr_debug("allocated notify ep request & request buffer\n");

	mbim->not_port.notify_req->context = mbim;
	mbim->not_port.notify_req->complete = mbim_notify_complete;


#if 0
	mbim->fs.in = usb_find_endpoint(mbim_fs_function,
			f->descriptors, &fs_mbim_in_desc);
	mbim->fs.out = usb_find_endpoint(mbim_fs_function,
			f->descriptors, &fs_mbim_out_desc);
	mbim->fs.notify = usb_find_endpoint(mbim_fs_function,
			f->descriptors, &fs_mbim_notify_desc);
#endif

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	hs_mbim_in_desc.bEndpointAddress = fs_mbim_in_desc.bEndpointAddress;
	hs_mbim_out_desc.bEndpointAddress = fs_mbim_out_desc.bEndpointAddress;
	hs_mbim_notify_desc.bEndpointAddress = fs_mbim_notify_desc.bEndpointAddress;

	ss_mbim_in_desc.bEndpointAddress = fs_mbim_in_desc.bEndpointAddress;
	ss_mbim_out_desc.bEndpointAddress =	fs_mbim_out_desc.bEndpointAddress;
	ss_mbim_notify_desc.bEndpointAddress = fs_mbim_notify_desc.bEndpointAddress;

	status = usb_assign_descriptors(f, mbim_fs_function, mbim_hs_function,
			mbim_ss_function);
	if (status)
		goto fail;

	mbim->gether_port.open = mbim_gether_open;
	mbim->gether_port.close = mbim_gether_close;
#if 0
	pr_debug("mbim(%d): %s speed IN/%s OUT/%s NOTIFY/%s\n",
			mbim->port_num,
			gadget_is_superspeed(c->cdev->gadget) ? "super" :
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			mbim->gether_port.in_ep->name, mbim->gether_port.out_ep->name,
			mbim->not_port.notify->name);
#endif
	return 0;

fail:
	pr_err("%s failed to bind, err %d\n", f->name, status);
	usb_free_all_descriptors(f);


	if (mbim->not_port.notify_req) {
		kfree(mbim->not_port.notify_req->buf);
		usb_ep_free_request(mbim->not_port.notify,
					mbim->not_port.notify_req);
	}

	/* we might as well release our claims on endpoints */
	if (mbim->not_port.notify)
		mbim->not_port.notify->driver_data = NULL;
#if 1
	if (mbim->gether_port.out_ep)
		mbim->gether_port.out_ep->driver_data = NULL;
	if (mbim->gether_port.in_ep)
		mbim->gether_port.in_ep->driver_data = NULL;
#endif
	return status;
}

static void mbim_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_mbim		*mbim = func_to_mbim(f);

	DBG(c->cdev, "mbim unbind\n");

	mbim_string_defs[0].id = 0;
	usb_free_all_descriptors(f);

	kfree(mbim->not_port.notify_req->buf);
	usb_ep_free_request(mbim->not_port.notify, mbim->not_port.notify_req);
}


int
mbim_bind_config(struct usb_configuration *c, u8 ethaddr[ETH_ALEN],	struct mbim_eth_dev *dev)
{
	struct f_mbim   *mbim;
	int status = 0;

	if (_mbim_dev == NULL) {
		pr_err("mbim_init failed");
		return -1;
	}
	mbim = _mbim_dev;

	if (mbim == NULL) {
		pr_err("NULL mbim_dev");
		status = -EINVAL;
		return status;
	}


	if (!ethaddr) {
		mbim->host_addr[0] = 0x02;
		mbim->host_addr[1] = 0x02;
		mbim->host_addr[2] = 0x45;
		mbim->host_addr[3] = 0x46;
		mbim->host_addr[4] = 0x67;
		mbim->host_addr[5] = 0x55;
	} else
		ether_addr_copy(mbim->host_addr, ethaddr);

	/*mbim->vendorID = vendorID;*/
	/*mbim->manufacturer = manufacturer;*/

	mbim->gether_port.ioport = dev;

	/* maybe allocate device-global string IDs */
	if (mbim_string_defs[0].id == 0) {
		status = usb_string_ids_tab(c->cdev, mbim_string_defs);
		if (status)
			return status;

		mbim_control_intf.iInterface = mbim_string_defs[STRING_CTRL_IDX].id;
		mbim_data_intf.iInterface = mbim_string_defs[STRING_DATA_IDX].id;
	}

	/* allocate and initialize one new instance */
	mbim = mbim_ports[0].port;
	if (!mbim) {
		pr_err("mbim struct not allocated");
		mbim_string_defs[0].id = 0;
		return -ENOMEM;
	}

	mbim->cdev = c->cdev;

	mbim_reset_values(mbim);
#if 1
	mbim->gether_port.is_fixed = true;

	mbim->gether_port.cdc_filter = DEFAULT_FILTER;
	mbim->gether_port.func.name = "usb_mbim";
	mbim->gether_port.func.strings = mbim_strings;
	/* descriptors are per-instance copies */
	mbim->gether_port.func.bind = mbim_bind;
	mbim->gether_port.func.unbind = mbim_unbind;
	mbim->gether_port.func.set_alt = mbim_set_alt;
	mbim->gether_port.func.get_alt = mbim_get_alt;
	mbim->gether_port.func.setup = mbim_setup;
	mbim->gether_port.func.disable = mbim_disable;

	mbim->gether_port.wrap = mbim_wrap_ntb;
	mbim->gether_port.unwrap = mbim_unwrap_ntb;
#else
	mbim->function.name = "mbim";
	mbim->function.strings = mbim_strings;
/* descriptors are per-instance copies */
	mbim->function.bind = mbim_bind;
	mbim->function.unbind = mbim_unbind;
	mbim->function.set_alt = mbim_set_alt;
	mbim->function.get_alt = mbim_get_alt;
	mbim->function.setup = mbim_setup;
	mbim->function.disable = mbim_disable;
#endif

	pr_err("usb_add_function\n");
#if 1
	status = usb_add_function(c, &mbim->gether_port.func);
#else
	status = usb_add_function(c, &mbim->function);
#endif
	if (status) {
		pr_err("usb_add_function(c, &mbim->port.func) failed");
		mbim_string_defs[0].id = 0;
		return status;
	}

	pr_err("Exit status %d", status);

	return status;
}

/* ------------ MBIM DRIVER File Operations API for USER SPACE ------------ */

static ssize_t
mbim_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct f_mbim *dev = fp->private_data;
	struct ctrl_pkt *cpkt = NULL;
	int ret = 0;


	if (!dev) {
		pr_err("Received NULL mbim pointer\n");
		return -ENODEV;
	}

	if (count > MBIM_BULK_BUFFER_SIZE)
		return -EINVAL;

	if (mbim_lock(&dev->read_excl)) {
		pr_err("Previous reading is not finished yet\n");
		return -EBUSY;
	}

	/* block until mbim online */
	while (!(atomic_read(&dev->online) || atomic_read(&dev->error))) {
		pr_err("USB cable not connected. Wait =%d %d.\n",
			atomic_read(&dev->online), atomic_read(&dev->error));
		ret = wait_event_interruptible(dev->read_wq,
			(atomic_read(&dev->online) || atomic_read(&dev->error)));
		pr_err("USB cable not connected. Wait =%d %d.\n",
			atomic_read(&dev->online), atomic_read(&dev->error));
		if (ret < 0) {
			pr_err("wait failed.. =%d ", ret);
			mbim_unlock(&dev->read_excl);
			return -ERESTARTSYS;
		}
	}

	if (atomic_read(&dev->error)) {
		mbim_unlock(&dev->read_excl);
		return -EIO;
	}

	while (list_empty(&dev->cpkt_req_q)) {
		pr_debug("Requests list is empty. Wait.\n");
		ret = wait_event_interruptible(dev->read_wq,
			!list_empty(&dev->cpkt_req_q));
		if (ret < 0) {
			pr_err("Waiting failed %d\n", ret);
			mbim_unlock(&dev->read_excl);
			return -ERESTARTSYS;
		}
		pr_debug("Received request packet\n");
	}

	cpkt = list_first_entry(&dev->cpkt_req_q, struct ctrl_pkt,
							list);
	if (cpkt == NULL) {
		pr_err("cpkt is NULL from list_first_entry, please check");
		return -EIO;
	}
	if (cpkt->len > count) {
		mbim_unlock(&dev->read_excl);
		return -ENOMEM;
	}

	pr_debug("cpkt size:%d\n", cpkt->len);

	list_del(&cpkt->list);
	mbim_unlock(&dev->read_excl);

	ret = copy_to_user(buf, cpkt->buf, cpkt->len);
	if (ret) {
		pr_err("copy_to_user failed: err %d\n", ret);
		ret = 0;
	} else {
		pr_debug("copied %d bytes to user\n", cpkt->len);
		ret = cpkt->len;
	}

	mbim_free_ctrl_pkt(cpkt);

	return ret;
}

static ssize_t
mbim_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	struct f_mbim *dev = fp->private_data;
	struct ctrl_pkt *cpkt = NULL;
	int ret = 0;


	if (!dev) {
		pr_err("Received NULL mbim pointer\n");
		return -ENODEV;
	}

	if (!count) {
		pr_err("zero length ctrl pkt\n");
		return -ENODEV;
	}

	if (count > MAX_CTRL_PKT_SIZE)
		return -ENOMEM;


	if (!atomic_read(&dev->online)) {
		pr_err("USB cable not connected\n");
		mbim_unlock(&dev->write_excl);
		return -EPIPE;
	}

	cpkt = mbim_alloc_ctrl_pkt(count, GFP_KERNEL);
	if (!cpkt) {
		pr_err("failed to allocate ctrl pkt\n");
		mbim_unlock(&dev->write_excl);
		return -ENOMEM;
	}

	ret = copy_from_user(cpkt->buf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed err:%d\n", ret);
		mbim_free_ctrl_pkt(cpkt);
		mbim_unlock(&dev->write_excl);
		return 0;
	}

	fmbim_send_cpkt_response(dev, cpkt);

	mbim_unlock(&dev->write_excl);

	return count;

}

static int mbim_open(struct inode *ip, struct file *fp)
{
	pr_debug("Open mbim driver\n");

	while (!_mbim_dev) {
		pr_err("mbim_dev not created yet\n");
		return -ENODEV;
	}

	if (mbim_lock(&_mbim_dev->open_excl)) {
		pr_err("Already opened\n");
		return -EBUSY;
	}

	pr_debug("Lock mbim_dev->open_excl for open\n");

	if (!atomic_read(&_mbim_dev->online))
		pr_err("USB cable not connected\n");

	_mbim_dev->user_pid = pid_nr(task_pid(current));
	pr_debug("Set MBIM daemon pid %d\n", pid_nr(task_pid(current)));

	fp->private_data = _mbim_dev;

	atomic_set(&_mbim_dev->error, 0);

	spin_lock_irq(&_mbim_dev->lock);
	_mbim_dev->is_open = true;
	spin_unlock_irq(&_mbim_dev->lock);

	pr_debug("Exit, mbim file opened\n");

	return 0;
}

static int mbim_release(struct inode *ip, struct file *fp)
{
	struct f_mbim *mbim = fp->private_data;
	unsigned long flags;

	pr_debug("Close mbim file");
	pr_err("=================================");
	pr_err("Mbim file is closed !!!!!!!!!!!!!!!!1");
	pr_err("=================================");

	spin_lock_irqsave(&mbim->lock, flags);
	mbim->is_open = false;
	spin_unlock_irqrestore(&mbim->lock, flags);

	mbim->user_pid = 0;

	mbim_unlock(&_mbim_dev->open_excl);

	return 0;
}

static long mbim_ioctl(struct file *fp, unsigned cmd, unsigned long arg)
{
	struct f_mbim *mbim = fp->private_data;
	int ret = 0;
	int data = 0;

	struct ioctl_arg_sid_ifid sid_ifid;
#ifdef MBIM_MULTI_SESSION
	struct ctrl_pkt *cpkt = NULL;
#endif

	pr_err("Received command %d", cmd);

#if 0
	if (mbim_lock(&mbim->ioctl_excl))
		return -EBUSY;
#endif

	switch (cmd) {
	case MBIM_GET_NTB_SIZE:
		ret = copy_to_user((void __user *)arg,
			&mbim->ntb_input_size, sizeof(mbim->ntb_input_size));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("Sent NTB size %d", mbim->ntb_input_size);
		break;
	case MBIM_GET_DATAGRAM_COUNT:
		ret = copy_to_user((void __user *)arg,
			&mbim->ntb_max_datagrams,
			sizeof(mbim->ntb_max_datagrams));
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("Sent NTB datagrams count %d",
			mbim->ntb_max_datagrams);
		break;
	case MBIM_SET_LOOPBACK_MODE:

		if (copy_from_user(&data, (void __user *)arg, sizeof(u8))) {
			pr_err("MBIM_SET_LOOPBACK_MODE copy_from_user failed");
			ret = -EFAULT;
			break;
		}

		if ((data == 1) && (mbim->loopback_mode == false)) {
			mbim->loopback_mode = true;
			mbim->net->features |= NETIF_F_LOOPBACK;
			pr_debug("Enable loopback mode");
		} else if ((data == 0) && (mbim->loopback_mode == true)) {
			mbim->loopback_mode = false;
			mbim->net->features &= ~NETIF_F_LOOPBACK;
			pr_debug("Disable loopback mode");
		} else {
			pr_err("loopback mode setting mismatch arg=%d, loopback_mode=%d", data, mbim->loopback_mode);
		}
		break;

	case MBIM_SET_SID_IFID:
		if (copy_from_user(&sid_ifid, (void __user *)arg, sizeof(struct ioctl_arg_sid_ifid))) {
			pr_err("MBIM_SET_SID_IFID copy_from_user failed");
			ret = -EFAULT;
			break;
		}

		if (sid_ifid.ifid >= MAX_SESSION_NUM) {
			pr_err("Out of sessionID =%d\n", sid_ifid.ifid);
			ret = -EFAULT;
			break;
		}

		mbim->ethid_to_sid_tbl[sid_ifid.ifid] = sid_ifid.sid;
		pr_err("mbim->ethid_to_sid_tbl[%d] = %d\n", sid_ifid.ifid, sid_ifid.sid);
#ifdef CONFIG_MTK_NET_CCMNI
		ccmni_update_mbim_interface(0, sid_ifid.ifid);
#endif
		break;
#ifdef MBIM_MULTI_SESSION
	case MBIM_GET_EVENT:

		if (mbim_lock(&mbim->event_read_excl)) {
			pr_err("Previous reading is not finished yet\n");
			return -EBUSY;
		}

		/* block until mbim online */
		if (atomic_read(&mbim->error)) {
			mbim_unlock(&mbim->event_read_excl);
			return -EIO;
		}

		while (list_empty(&mbim->cpkt_event_q)) {
			pr_err("Requests list is empty. Wait.\n");
			ret = wait_event_interruptible(mbim->event_read_wq,
					!list_empty(&mbim->cpkt_event_q));
			if (ret < 0) {
				pr_err("Waiting failed\n");
				mbim_unlock(&mbim->event_read_excl);
				return -ERESTARTSYS;
			}
			pr_debug("Received event packet\n");
		}

		cpkt = list_first_entry(&mbim->cpkt_event_q, struct ctrl_pkt,
				list);
		if (cpkt->len > sizeof(struct ioctl_arg_event_data)) {
			mbim_unlock(&mbim->event_read_excl);
			return -ENOMEM;
		}

		pr_debug("cpkt size:%d\n", cpkt->len);

		list_del(&cpkt->list);
		mbim_unlock(&mbim->event_read_excl);

		ret = copy_to_user((void __user *)arg,
			cpkt->buf, cpkt->len);
		if (ret) {
			pr_err("copying to user space failed");
			ret = -EFAULT;
		}
		pr_debug("MBIM_GET_EVENT ");

		mbim_free_ctrl_pkt(cpkt);


		break;
#endif
	default:
		pr_err("wrong parameter\n");
		ret = -EINVAL;
	}

#if 0
	mbim_unlock(&mbim->ioctl_excl);
#endif

	return ret;
}

/* file operations for MBIM device /dev/mbim */
static const struct file_operations mbim_fops = {
	.owner = THIS_MODULE,
	.open = mbim_open,
	.release = mbim_release,
	.read = mbim_read,
	.write = mbim_write,
	.unlocked_ioctl = mbim_ioctl,
};

static struct miscdevice mbim_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mbim",
	.fops = &mbim_fops,
};

#ifdef MBIM_CTRL
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/* MBIM proc file system control commands */
enum {
	MBIM_NOTIFY_CLOSE	= 0x00, /* usb0.x CLOSE notify*/
	MBIM_NOTIFY_OPEN	= 0x01, /* usb0.x OPEN notify*/
};

static int mbim_proc_ctrl_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%d\n", 0);

	return 0;
}

static int mbim_proc_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, mbim_proc_ctrl_show, NULL);
}

static ssize_t mbim_proc_ctrl_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	unsigned op = 0;
	unsigned vlan_id = 0;
	u8 buf[LINE_SIZE] = {0};

	len = count > sizeof(buf) ? sizeof(buf) : count;
	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	if (sscanf(buf, "0x%x 0x%x", &op, &vlan_id) == 2) {
		pr_debug("[mbim_proc_ctrl_write] case = [%d, %d]\n", op, vlan_id);
		switch (op) {

		case MBIM_NOTIFY_CLOSE:
			if (vlan_id >= MAX_SESSION_NUM) {
				pr_debug("[mbim_proc_ctl_write] Error! vlan_id [%d] >= MAX_SESSION_NUM [%d]",
					vlan_id, MAX_SESSION_NUM);
				break;
			}

			_mbim_dev->interface_state[vlan_id] = 0;
			mbim_daemon_signal(MBIM_SIG_DATA_PATH_CLOSED, &vlan_id, sizeof(vlan_id));
			break;

		case MBIM_NOTIFY_OPEN:
			if (vlan_id >= MAX_SESSION_NUM) {
				pr_debug("[mbim_proc_ctl_write] Error! vlan_id [%d] >= MAX_SESSION_NUM [%d]",
					vlan_id, MAX_SESSION_NUM);
				break;
			}

			_mbim_dev->interface_state[vlan_id] = 1;
			mbim_daemon_signal(MBIM_SIG_DATA_PATH_OPENED, &vlan_id, sizeof(vlan_id));
			break;

		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return len;
}

static const struct file_operations mbim_proc_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= mbim_proc_ctrl_open,
	.read		= seq_read,
	.write		= mbim_proc_ctrl_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int mbim_proc_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *p;

	if (!parent)
		return -ENOMEM;

	p = proc_create(MBIM_CTRL_FILE, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH, parent, &mbim_proc_ctrl_fops);
	if (!p)
		return -ENOMEM;

	return 0;
}

void mbim_proc_exit(struct proc_dir_entry *parent)
{
	if (parent)
		remove_proc_entry(MBIM_CTRL_FILE, parent);
}
#endif

/****************************************************************/

#ifdef MBIM_MULTI_SESSION
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/rculist.h>
#include <net/p8022.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/notifier.h>
#include <net/rtnetlink.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <asm/uaccess.h>

static int vlan_device_event(struct notifier_block *unused, unsigned long event,
							 void *ptr)
{
		struct net_device *vlan_dev = ptr;
		struct vlan_group *grp;
		struct vlan_info *vlan_info;
		int i, flgs;
		struct net_device *real_dev;
		struct vlan_dev_priv *vlan;
		bool last = false;
		unsigned int vlan_id;

		LIST_HEAD(list);

		if (is_vlan_dev(vlan_dev)) {
			vlan_id = (unsigned int) vlan_dev_vlan_id(vlan_dev);
			pr_debug("is_vlan_dev, vlan->id = %d\n", vlan_id);
			real_dev = vlan_dev_real_dev(vlan_dev);
		} else {
			pr_debug("is not vlan_dev\n");
			goto out;
		}

		switch (event) {
		case NETDEV_UP:
				pr_debug("NETDEV_UP\n");
				mbim_daemon_signal(MBIM_SIG_DATA_PATH_OPENED, &vlan_id, sizeof(vlan_id));
				break;
		case NETDEV_DOWN:
				pr_debug("NETDEV_DOWN\n");
				mbim_daemon_signal(MBIM_SIG_DATA_PATH_CLOSED, &vlan_id, sizeof(vlan_id));
				break;
		}

out:
		return NOTIFY_DONE;
}

static struct notifier_block vlan_notifier_block __read_mostly = {
	.notifier_call = vlan_device_event,
};
#endif

int mbim_init(int instances)
{
	int i;
#ifdef MBIM_MULTI_SESSION
	int j;
#endif
	struct f_mbim *dev = NULL;
	int ret;

	pr_debug("initialize %d instances\n", instances);

	pr_debug("max seg size = %d", mbim_desc.wMaxSegmentSize);
	if (instances > NR_MBIM_PORTS) {
		pr_err("Max-%d instances supported\n", NR_MBIM_PORTS);
		return -EINVAL;
	}

	for (i = 0; i < instances; i++) {
		dev = kzalloc(sizeof(struct f_mbim), GFP_KERNEL);
		if (!dev) {
			pr_err("Failed to allocate mbim dev\n");
			ret = -ENOMEM;
			goto fail_probe;
		}

		dev->port_num = i;
		spin_lock_init(&dev->lock);
		INIT_LIST_HEAD(&dev->cpkt_req_q);
		atomic_set(&dev->req_count, 0);
		INIT_LIST_HEAD(&dev->cpkt_resp_q);
		atomic_set(&dev->resp_count, 0);

		mbim_ports[i].port = dev;
		mbim_ports[i].port_num = i;

		init_waitqueue_head(&dev->read_wq);
		init_waitqueue_head(&dev->write_wq);
#ifdef MBIM_EVENT_BY_FILE
		INIT_LIST_HEAD(&dev->cpkt_event_q);
		init_waitqueue_head(&dev->event_read_wq);
		atomic_set(&dev->event_read_excl, 0);
#endif

		atomic_set(&dev->open_excl, 0);
		atomic_set(&dev->ioctl_excl, 0);
		atomic_set(&dev->read_excl, 0);
		atomic_set(&dev->write_excl, 0);

		nr_mbim_ports++;

#ifdef MBIM_MULTI_SESSION
		for (j = 0; j < MAX_SESSION_NUM; j++) {
			dev->ethid_to_sid_tbl[j] = -1;
			dev->interface_state[j] = 0;
		}
#endif

		INIT_WORK(&dev->linkup_work, mbim_linkup_work);
		INIT_WORK(&dev->linkdown_work, mbim_linkdown_work);


	}

	_mbim_dev = dev;
	dev->loopback_mode = false;
	ret = misc_register(&mbim_device);
	if (ret) {
		pr_err("mbim driver failed to register");
		goto fail_probe;
	}

	pr_debug("Initialized %d ports\n", nr_mbim_ports);

	return ret;

fail_probe:
	pr_err("Failed");
	for (i = 0; i < nr_mbim_ports; i++) {
		kfree(mbim_ports[i].port);
		mbim_ports[i].port = NULL;
	}

	return ret;
}

void mbim_cleanup(void)
{
	int i = 0;

	pr_debug("Enter");

	for (i = 0; i < nr_mbim_ports; i++) {
		kfree(mbim_ports[i].port);
		mbim_ports[i].port = NULL;
	}
	nr_mbim_ports = 0;

	misc_deregister(&mbim_device);

	_mbim_dev = NULL;
}

