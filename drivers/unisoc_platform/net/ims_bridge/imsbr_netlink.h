/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_NETLINK_H
#define _IMSBR_NETLINK_H

int imsbr_netlink_init(void);
void imsbr_netlink_exit(void);
int imsbr_spi_match(u32 spi);

struct esph {
	__u32 spi;
	__u32 seqno;
};

#endif
