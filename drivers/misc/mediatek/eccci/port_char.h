/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __PORT_CHAR__
#define __PORT_CHAR__

/* External API called by port_char object */
extern int port_ipc_init(struct ccci_port *port);
extern int port_ipc_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg);
extern int port_ipc_recv_match(struct ccci_port *port, struct sk_buff *skb);
extern void port_ipc_md_state_notice(struct ccci_port *port, MD_STATE state);
extern int port_ipc_write_check_id(struct ccci_port *port, struct sk_buff *skb);
extern unsigned int port_ipc_poll(struct file *fp, struct poll_table_struct *poll);

extern int port_smem_init(struct ccci_port *port);
extern int port_smem_mmap(struct ccci_port *port, struct vm_area_struct *vma);
extern long port_smem_ioctl(struct ccci_port *port, unsigned int cmd, unsigned long arg);
extern int port_smem_rx_wakeup(struct ccci_port *port);

extern void port_poller_start(struct ccci_port *port);
extern void port_poller_stop(struct ccci_port *port);

extern int port_rpc_recv_match(struct ccci_port *port, struct sk_buff *skb);

extern int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
		unsigned int length);
#endif	/*__PORT_CHAR__*/
