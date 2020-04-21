/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _SCP_IPI_TABLE_H_
#define _SCP_IPI_TABLE_H_

#include "scp_mbox_layout.h"
#include "scp_ipi_pin.h"

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
struct mtk_mbox_info scp_mbox_info[SCP_MBOX_TOTAL] = {
	{0, 0, 0, 64, 0, 1, 1, 0, 0, 0, 0, 0, 0, { { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 1, 64, 0, 1, 1, 0, 0, 0, 0, 0, 0, { { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 2, 64, 0, 1, 1, 0, 0, 0, 0, 0, 0, { { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 3, 64, 0, 1, 1, 0, 0, 0, 0, 0, 0, { { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 4, 64, 0, 1, 1, 0, 0, 0, 0, 0, 0, { { { { 0 } } } }, {0, 0, 0} },
};

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
struct mtk_mbox_pin_recv scp_mbox_pin_recv[] = {
	{0, 0, 0, 0, 1, 0, PIN_IN_SIZE_AUDIO_VOW_ACK_1, 0,
	 IPI_IN_AUDIO_VOW_ACK_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{0, 0, 0, 0, 1, 0, PIN_IN_SIZE_AUDIO_VOW_1, 0,
	 IPI_IN_AUDIO_VOW_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{1, 0, 0, 0, 1, 0, PIN_IN_SIZE_APCCCI_0, 0,
	 IPI_IN_APCCCI_0, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{1, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_ERROR_INFO_0, 0,
	 IPI_IN_SCP_ERROR_INFO_0, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{1, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_READY_0, 0,
	 IPI_IN_SCP_READY_0, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{1, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_RAM_DUMP_0, 0,
	 IPI_IN_SCP_RAM_DUMP_0, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{1, 0, 1, 0, 1, 0, PIN_OUT_R_SIZE_SLEEP_0, 0,
	 IPI_OUT_C_SLEEP_0, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{2, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_MPOOL, 0,
	 IPI_IN_SCP_MPOOL_0, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{3, 0, 0, 0, 1, 0, PIN_IN_SIZE_AUDIO_ULTRA_SND_1, 0,
	 IPI_IN_AUDIO_ULTRA_SND_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{3, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_ERROR_INFO_1, 0,
	 IPI_IN_SCP_ERROR_INFO_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{3, 0, 0, 0, 1, 0, PIN_IN_SIZE_LOGGER_CTRL, 0,
	 IPI_IN_LOGGER_CTRL, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{3, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_READY_1, 0,
	 IPI_IN_SCP_READY_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{3, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_RAM_DUMP_1, 0,
	 IPI_IN_SCP_RAM_DUMP_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{3, 0, 1, 0, 1, 0, PIN_OUT_R_SIZE_SLEEP_1, 0,
	 IPI_OUT_C_SLEEP_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
	{4, 0, 0, 0, 1, 0, PIN_IN_SIZE_SCP_MPOOL, 0,
	 IPI_IN_SCP_MPOOL_1, { 0 }, 0, 0, 0, { { { { 0 } } } },
	{0, 0, 0, 0, 0, 0} },
};

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
struct mtk_mbox_pin_send scp_mbox_pin_send[] = {
	{0, 0, 0, 0, PIN_OUT_SIZE_AUDIO_VOW_1, 0,
	 IPI_OUT_AUDIO_VOW_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, 0, 0, 0, PIN_OUT_SIZE_APCCCI_0, 0,
	 IPI_OUT_APCCCI_0, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, 0, 0, 0, PIN_OUT_SIZE_DVFS_SET_FREQ_0, 0,
	 IPI_OUT_DVFS_SET_FREQ_0, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, 0, 0, 0, PIN_OUT_C_SIZE_SLEEP_0, 0,
	 IPI_OUT_C_SLEEP_0, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, 0, 0, 0, PIN_OUT_SIZE_TEST_0, 0,
	 IPI_OUT_TEST_0, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{2, 0, 0, 0, PIN_OUT_SIZE_SCP_MPOOL, 0,
	 IPI_OUT_SCP_MPOOL_0, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, 0, 0, 0, PIN_OUT_SIZE_AUDIO_ULTRA_SND_1, 0,
	 IPI_OUT_AUDIO_ULTRA_SND_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, 0, 0, 0, PIN_OUT_SIZE_DVFS_SET_FREQ_1, 0,
	 IPI_OUT_DVFS_SET_FREQ_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, 0, 0, 0, PIN_OUT_C_SIZE_SLEEP_1, 0,
	 IPI_OUT_C_SLEEP_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, 0, 0, 0, PIN_OUT_SIZE_TEST_1, 0,
	 IPI_OUT_TEST_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, 0, 0, 0, PIN_OUT_SIZE_LOGGER_CTRL, 0,
	 IPI_OUT_LOGGER_CTRL, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, 0, 0, 0, PIN_OUT_SIZE_SCPCTL_1, 0,
	 IPI_OUT_SCPCTL_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
	{4, 0, 0, 0, PIN_OUT_SIZE_SCP_MPOOL, 0,
	 IPI_OUT_SCP_MPOOL_1, { { 0 } }, { 0 }, { { { { 0 } } } } },
};

#define SCP_TOTAL_RECV_PIN	(sizeof(scp_mbox_pin_recv) \
				 / sizeof(struct mtk_mbox_pin_recv))
#define SCP_TOTAL_SEND_PIN	(sizeof(scp_mbox_pin_send) \
				 / sizeof(struct mtk_mbox_pin_send))

struct mtk_mbox_device scp_mboxdev = {
	.name = "scp_mboxdev",
	.pin_recv_table = &scp_mbox_pin_recv[0],
	.pin_send_table = &scp_mbox_pin_send[0],
	.info_table = &scp_mbox_info[0],
	.count = SCP_MBOX_TOTAL,
	.recv_count = SCP_TOTAL_RECV_PIN,
	.send_count = SCP_TOTAL_SEND_PIN,
	.post_cb = (mbox_rx_cb_t)scp_clr_spm_reg,
};

struct mtk_ipi_device scp_ipidev = {
	.name = "scp_ipidev",
	.id = IPI_DEV_SCP,
	.mbdev = &scp_mboxdev,
	.pre_cb = (ipi_tx_cb_t)scp_awake_lock,
	.post_cb = (ipi_tx_cb_t)scp_awake_unlock,
	.prdata = 0,
};

#endif
