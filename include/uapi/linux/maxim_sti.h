/* drivers/input/touchscreen/maxim_sti.c
 *
 * Maxim SmartTouch Imager Touchscreen Driver
 *
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 * Copyright (C) 2013, NVIDIA Corporation.  All Rights Reserved.
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MAXIM_STI_H__
#define __MAXIM_STI_H__

#ifndef __KERNEL__
#include <stdlib.h>
#include "genetlink.h"
#endif

#define XSTR(s)               STR(s)
#define STR(s)                #s

#define DRV_VER_MAJOR         1
#define DRV_VER_MINOR         1

#define DRIVER_VERSION_STR    XSTR(DRV_VER_MAJOR) "." XSTR(DRV_VER_MINOR)
#define DRIVER_VERSION_NUM    ((DRV_VER_MAJOR << 8) | DRV_VER_MINOR)

#define DRIVER_VERSION        DRIVER_VERSION_STR
#define DRIVER_RELEASE        "April 29, 2015"
#define DRIVER_PROTOCOL       0x0102

/****************************************************************************\
* Netlink: common kernel/user space macros                                   *
\****************************************************************************/

#define NL_BUF_SIZE  30720

#define NL_ATTR_FIRST(nptr) \
	((struct nlattr *)((void *)nptr + NLMSG_HDRLEN + GENL_HDRLEN))
#define NL_ATTR_LAST(nptr) \
	((struct nlattr *)((void *)nptr + \
			NLMSG_ALIGN(((struct nlmsghdr *)nptr)->nlmsg_len)))
#define NL_SIZE(nptr)   NLMSG_ALIGN(((struct nlmsghdr *)nptr)->nlmsg_len)
#define NL_TYPE(nptr)              (((struct nlmsghdr *)nptr)->nlmsg_type)
#define NL_SEQ(nptr)               (((struct nlmsghdr *)nptr)->nlmsg_seq)
#define NL_OK(nptr)              (NL_TYPE(nptr) >= NLMSG_MIN_TYPE)
#define NL_ATTR_VAL(aptr, type)  ((type *)((void *)aptr + NLA_HDRLEN))
#define NL_ATTR_NEXT(aptr) \
	((struct nlattr *)((void *)aptr + \
			NLA_ALIGN(((struct nlattr *)aptr)->nla_len)))
#define GENL_CMP(name1, name2)  strncmp(name1, name2, GENL_NAMSIZ)
#define GENL_COPY(name1, name2) strlcpy(name1, name2, GENL_NAMSIZ)
#define GENL_CHK(name)          (strlen(name) > (GENL_NAMSIZ - 1))
#define MSG_TYPE(nptr)          NL_ATTR_FIRST(nptr)->nla_type
#define MSG_PAYLOAD(nptr)       NL_ATTR_VAL(NL_ATTR_FIRST(nptr), void)

/****************************************************************************\
* Netlink: common kernel/user space inline functions                         *
\****************************************************************************/

static inline void
nl_msg_init(void *buf, __u16 family_id, __u32 sequence, __u8 dst)
{
	struct nlmsghdr    *nlh = (struct nlmsghdr *)buf;
	struct genlmsghdr  *genl = (struct genlmsghdr *)(buf + NLMSG_HDRLEN);

	memset(buf, 0, NLMSG_HDRLEN + GENL_HDRLEN);
	nlh->nlmsg_type = family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = sequence;
	nlh->nlmsg_len = NLMSG_HDRLEN + GENL_HDRLEN;
	genl->cmd = dst;
}

static inline void
*nl_alloc_attr(void *buf, __u16 type, __u16 len)
{
	struct nlmsghdr  *nlh = (struct nlmsghdr *)buf;
	struct nlattr    *attr = NL_ATTR_LAST(nlh);

	if ((NL_SIZE(buf) + NLMSG_ALIGN(NLA_HDRLEN + len)) > NL_BUF_SIZE)
		return NULL;

	attr->nla_type = type;
	attr->nla_len = NLA_HDRLEN + len;
	nlh->nlmsg_len += NLMSG_ALIGN(attr->nla_len);
	return NL_ATTR_VAL(attr, void);
}

static inline int
nl_add_attr(void *buf, __u16 type, void *ptr, __u16 len)
{
	void  *a_ptr;

	a_ptr = nl_alloc_attr(buf, type, len);
	if (a_ptr == NULL)
		return -EPERM;
	memcpy(a_ptr, ptr, len);
	return 0;
}

/****************************************************************************\
* Netlink: multicast groups enum and name strings                            *
\****************************************************************************/

enum {
	MC_DRIVER,
	MC_FUSION,
	MC_EVENT_BROADCAST,
	MC_GROUPS,
};

#define MC_DRIVER_NAME     "driver"
#define MC_FUSION_NAME     "fusion"
#define MC_EVENT_BROADCAST_NAME  "event_broadcast"

#define NL_FAMILY_VERSION  1

#define TF_FAMILY_NAME     "touch_fusion"

/****************************************************************************\
* Netlink: common parameter and message definitions                          *
\****************************************************************************/

enum {
	DR_STATE_BASIC,
	DR_STATE_ACTIVE,
	DR_STATE_SUSPEND,
	DR_STATE_RESUME,
	DR_STATE_FAULT,
};

enum {
	DR_INPUT_FINGER,
	DR_INPUT_STYLUS,
	DR_INPUT_ERASER,
};

enum {
	DR_IRQ_FALLING_EDGE,
	DR_IRQ_RISING_EDGE,
};

enum {
	DR_ADD_MC_GROUP,
	DR_ECHO_REQUEST,
	DR_CHIP_READ,
	DR_CHIP_WRITE,
	DR_CHIP_RESET,
	DR_GET_IRQLINE,
	DR_DELAY,
	DR_CHIP_ACCESS_METHOD,
	DR_CONFIG_IRQ,
	DR_CONFIG_INPUT,
	DR_CONFIG_WATCHDOG,
	DR_DECONFIG,
	DR_INPUT,
	DR_RESUME_ACK,
	DR_LEGACY_FWDL,
	DR_LEGACY_ACCELERATION,
	DR_HANDSHAKE,
	DR_CONFIG_FW,
	DR_IDLE,
	DR_SYSFS_ACK,
	DR_TF_STATUS,
};

struct __attribute__ ((__packed__)) dr_add_mc_group {
	__u8  number;
	char  name[GENL_NAMSIZ];
};

struct __attribute__ ((__packed__)) dr_echo_request {
	__u32  cookie;
};

struct __attribute__ ((__packed__)) dr_chip_read {
	__u16  address;
	__u16  length;
};

struct __attribute__ ((__packed__)) dr_chip_write {
	__u16  address;
	__u16  length;
	__u8   data[0];
};

struct __attribute__ ((__packed__)) dr_chip_reset {
	__u8  state;
};

struct __attribute__ ((__packed__)) dr_delay {
	__u32  period;
};

struct __attribute__ ((__packed__)) dr_chip_access_method {
	__u8  method;
};

#define MAX_IRQ_PARAMS  37
struct __attribute__ ((__packed__)) dr_config_irq {
	__u8   irq_method;
	__u8   irq_edge;
	__u8   irq_params;
	__u16  irq_param[MAX_IRQ_PARAMS];
};

struct __attribute__ ((__packed__)) dr_config_input {
	__u16  x_range;
	__u16  y_range;
};

struct __attribute__ ((__packed__)) dr_config_watchdog {
	__u32  pid;
};

struct __attribute__ ((__packed__)) dr_input_event {
	__u8   id;
	__u8   tool_type;
	__u16  x;
	__u16  y;
	__u8   z;
};

#define MAX_INPUT_EVENTS  10
struct __attribute__ ((__packed__)) dr_input {
	struct dr_input_event  event[MAX_INPUT_EVENTS];
	__u8                   events;
};

struct __attribute__ ((__packed__)) dr_legacy_acceleration {
	__u8  enable;
};

struct __attribute__ ((__packed__)) dr_handshake {
	__u16 tf_ver;
	__u16 chip_id;
};

#define  DR_SYSFS_UPDATE_NONE     0x0000
#define  DR_SYSFS_UPDATE_BIT_GLOVE    0
#define  DR_SYSFS_UPDATE_BIT_CHARGER  1
#define  DR_SYSFS_UPDATE_BIT_LCD_FPS  2

#define  DR_SYSFS_ACK_GLOVE       0x5A5A5A5A
#define  DR_SYSFS_ACK_CHARGER     0xA5A5A5A5
#define  DR_SYSFS_ACK_LCD_FPS     0xC3C3C3C3

enum {
	DR_NO_CHARGER,
	DR_WIRED_CHARGER,
	DR_WIRELESS_CHARGER,
};

struct __attribute__ ((__packed__)) dr_sysfs_ack {
	__u32 type;
};

struct __attribute__ ((__packed__)) dr_config_fw {
	__u16 fw_ver;
	__u16 fw_protocol;
};

struct __attribute__ ((__packed__)) dr_idle {
	__u8  idle;
};

#define  TF_STATUS_DEFAULT_LOADED (1 << 0)
#define  TF_STATUS_BUSY (1 << 1)

struct __attribute__ ((__packed__)) dr_tf_status {
	__u32 tf_status;
};

enum {
	FU_ECHO_RESPONSE,
	FU_CHIP_READ_RESULT,
	FU_IRQLINE_STATUS,
	FU_ASYNC_DATA,
	FU_RESUME,
	FU_HANDSHAKE_RESPONSE,
	FU_SYSFS_INFO,
};

struct __attribute__ ((__packed__)) fu_echo_response {
	__u32  cookie;
	__u8   driver_state;
};

struct __attribute__ ((__packed__)) fu_chip_read_result {
	__u16  address;
	__u16  length;
	__u8   data[0];
};

struct __attribute__ ((__packed__)) fu_irqline_status {
	__u8  status;
};

struct __attribute__ ((__packed__)) fu_async_data {
	__u16  address;
	__u16  length;
	__u16  status;
	__u8   data[0];
};

struct __attribute__ ((__packed__)) fu_handshake_response {
	__u16  driver_ver;
	__u16  panel_id;
	__u16  driver_protocol;
};

struct __attribute__ ((__packed__)) fu_sysfs_info {
	__u8   type;
	__u16  glove_value;
	__u16  charger_value;
	__u16  lcd_fps_value;
};

#endif

