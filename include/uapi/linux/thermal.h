/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_THERMAL_H
#define _UAPI_LINUX_THERMAL_H

#define THERMAL_NAME_LENGTH	20

/* Adding event notification support elements */
#define THERMAL_GENL_FAMILY_NAME                "thermal_event"
#define THERMAL_GENL_VERSION                    0x01
#define THERMAL_GENL_MCAST_GROUP_NAME           "thermal_mc_grp"

/* C3T code for HQ-223914 by liunianliang at 2022/08/03 start */
#define THERMAL_AVAILABLE_STATE_LENGTH	512
/* C3T code for HQ-223914 by liunianliang at 2022/08/03 end */

/* Events supported by Thermal Netlink */
enum events {
	THERMAL_AUX0,
	THERMAL_AUX1,
	THERMAL_CRITICAL,
	THERMAL_DEV_FAULT,
};

/* attributes of thermal_genl_family */
enum {
	THERMAL_GENL_ATTR_UNSPEC,
	THERMAL_GENL_ATTR_EVENT,
	__THERMAL_GENL_ATTR_MAX,
};
#define THERMAL_GENL_ATTR_MAX (__THERMAL_GENL_ATTR_MAX - 1)

/* commands supported by the thermal_genl_family */
enum {
	THERMAL_GENL_CMD_UNSPEC,
	THERMAL_GENL_CMD_EVENT,
	__THERMAL_GENL_CMD_MAX,
};
#define THERMAL_GENL_CMD_MAX (__THERMAL_GENL_CMD_MAX - 1)

#endif /* _UAPI_LINUX_THERMAL_H */
