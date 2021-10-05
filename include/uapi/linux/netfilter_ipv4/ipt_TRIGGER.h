/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/* Code copied from
   https://source.codeaurora.org/external/ubicom/ubicom32/tree/ubicom-linux-dist-3.2.1/linux-2.6.x/include/linux/netfilter_ipv4/ipt_PORTTRIGGER.h
 */

#include <linux/types.h>
#ifndef _IPT_PORTTRIGGER_H_target
#define _IPT_PORTTRIGGER_H_target

#define TRIGGER_TIMEOUT 600
#define IPT_MULTI_PORTS	15

enum porttrigger_mode {
	MODE_TRIGGER_DNAT,
	MODE_TRIGGER_FORWARD_IN,
	MODE_TRIGGER_FORWARD_OUT
};

struct ipt_mport {
	unsigned short pflags;			/* Port flags */
	unsigned short ports[IPT_MULTI_PORTS];	/* Ports */
};

struct ipt_porttrigger_info {
	enum porttrigger_mode mode;
	unsigned short trigger_proto;
	unsigned short forward_proto;
	unsigned int timer;
	struct ipt_mport trigger_ports;
	struct ipt_mport forward_ports;
};

#endif /*_IPT_PORTTRIGGER_H_target*/
