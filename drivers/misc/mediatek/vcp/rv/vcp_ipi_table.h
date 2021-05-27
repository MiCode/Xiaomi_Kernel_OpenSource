/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _VCP_IPI_TABLE_H_
#define _VCP_IPI_TABLE_H_

#include "vcp_mbox_layout.h"
#include "vcp_ipi_pin.h"

/*
 * mbox information
 *
 * mbdev  :mbox device
 * irq_num:identity of mbox irq
 * id     :mbox id
 * slot   :how many slots that mbox used, up to 1GB
 * opt    :option for mbox or share memory, 0:mbox, 1:share memory
 * enable :mbox status, 0:disable, 1: enable
 * is64d  :mbox is64d status, 0:32d, 1: 64d
 * base   :mbox base address
 * set_irq_reg  : mbox set irq register
 * clr_irq_reg  : mbox clear irq register
 * init_base_reg: mbox initialize register
 * mbox lock    : lock of mbox
 */
struct mtk_mbox_info *vcp_mbox_info;

/*
 * mbox pin structure, this is for receive defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * recv_opt : (opt)recv option,  0:receive ,1: response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * buf_full_opt : (opt)buffer option 0:drop, 1:assert, 2:overwrite(plt)
 * cb_ctx_opt : (opt)callback option 0:isr context, 1:process context(plt)
 * msg_size   : (slot)msg used slots in the mbox, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id : (u32) ipc channel id(plt)
 * notify     : (completion)notify process
 * mbox_pin_cb: (cb)cb function
 * pin_buf : (void*)buffer point
 * prdata  : (void*)private data
 * pin_lock: (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_recv *vcp_mbox_pin_recv;


/*
 * mbox pin structure, this is for send defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * send_opt : (opt)send opt, 0:send ,1: send for response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * msg_size : (slot)message size in words, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id    : (u32) ipc channel id(plt)
 * mutex      : (mutex)mutex for remote response
 * completion : (completion)completion for remote response
 * pin_lock   : (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_send *vcp_mbox_pin_send;


struct mtk_mbox_device vcp_mboxdev = {
	.name = "vcp_mboxdev",
	.pin_recv_table = 0,
	.pin_send_table = 0,
	.info_table = 0,
	.count = 0,
	.recv_count = 0,
	.send_count = 0,
	.post_cb = (mbox_rx_cb_t)vcp_clr_spm_reg,
};

struct mtk_ipi_device vcp_ipidev = {
	.name = "vcp_ipidev",
	.id = IPI_DEV_VCP,
	.mbdev = &vcp_mboxdev,
	.pre_cb = (ipi_tx_cb_t)vcp_awake_lock,
	.post_cb = (ipi_tx_cb_t)vcp_awake_unlock,
	.prdata = 0,
};
EXPORT_SYMBOL(vcp_ipidev);

#endif
