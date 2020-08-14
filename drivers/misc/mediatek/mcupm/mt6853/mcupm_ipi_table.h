/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MCUPM_IPI_TABLE_H__
#define __MCUPM_IPI_TABLE_H__

#include <mt-plat/mtk_tinysys_ipi.h>
#include "mcupm_ipi_id.h"

#define MCUPM_MBOX_TOTAL 8

/*share memory start address defination*/
#define SMEM_SIZE_80B			0x00000014      //80 Bytes
#define PIN_S_SIZE			SMEM_SIZE_80B
#define PIN_R_SIZE			SMEM_SIZE_80B
#define MBOX_TABLE_SIZE			(PIN_S_SIZE + PIN_R_SIZE)
#define SMEM0_SET_IRQ_REG		SW_INT_SET
#define SMEM0_CLR_IRQ_REG		SW_INT_CLR

/* definition of slot offset for send PINs */
#define PIN_S_OFFSET_PLATFORM		0
#define PIN_S_OFFSET_CPU_DVFS		0
#define PIN_S_OFFSET_FHCTL		0
#define PIN_S_OFFSET_MCDI		0
#define PIN_S_OFFSET_SUSPEND		0
#define PIN_S_OFFSET_SMET       0
#define PIN_S_OFFSET_RMET       0
#define PIN_S_OFFSET_EEMSN       0

#define PIN_S_MSG_SIZE_PLATFORM		4	//uint 4 byts
#define PIN_S_MSG_SIZE_CPU_DVFS		4	//uint 4 byts
#define PIN_S_MSG_SIZE_FHCTL		9	//uint 4 byts
#define PIN_S_MSG_SIZE_MCDI		3	//uint 4 byts
#define PIN_S_MSG_SIZE_SUSPEND		3	//uint 4 byts
#define PIN_S_MSG_SIZE_SMET         4   //unit 4 bytes
#define PIN_S_MSG_SIZE_RMET         4   //unit 4 bytes
#define PIN_S_MSG_SIZE_EEMSN     4 //unit 4 bytes

/* definition of slot size for send PINs */
#define PIN_S_SIZE_PLATFORM	PIN_S_SIZE
#define PIN_S_SIZE_CPU_DVFS	PIN_S_SIZE
#define PIN_S_SIZE_FHCTL	PIN_S_SIZE
#define PIN_S_SIZE_MCDI	PIN_S_SIZE
#define PIN_S_SIZE_SUSPEND	PIN_S_SIZE
#define PIN_S_SIZE_SMET     PIN_S_SIZE
#define PIN_S_SIZE_RMET     PIN_S_SIZE
#define PIN_S_SIZE_EEMSN	PIN_S_SIZE

#define PIN_R_MSG_SIZE_PLATFORM	1	//uint 4 byts
#define PIN_R_MSG_SIZE_CPU_DVFS	4	//uint 4 byts
#define PIN_R_MSG_SIZE_FHCTL	1	//uint 4 byts
#define PIN_R_MSG_SIZE_MCDI	1	//uint 4 byts
#define PIN_R_MSG_SIZE_SUSPEND	1	//uint 4 byts
#define PIN_R_MSG_SIZE_SMET     4   //uint 4 byts
#define PIN_R_MSG_SIZE_RMET     4   //uint 4 byts
#define PIN_R_MSG_SIZE_EEMSN     1 //unit 4 bytes

/* definition of slot size for received PINs */
#define PIN_R_SIZE_PLATFORM	PIN_R_SIZE
#define PIN_R_SIZE_CPU_DVFS	PIN_R_SIZE
#define PIN_R_SIZE_FHCTL	PIN_R_SIZE
#define PIN_R_SIZE_MCDI	PIN_R_SIZE
#define PIN_R_SIZE_SUSPEND	PIN_R_SIZE
#define PIN_R_SIZE_SMET     PIN_R_SIZE
#define PIN_R_SIZE_RMET     PIN_R_SIZE
#define PIN_R_SIZE_EEMSN	PIN_R_SIZE

/* definition of slot offset for received PINs */
#define PIN_R_OFFSET_PLATFORM	(PIN_S_OFFSET_PLATFORM + PIN_S_SIZE_PLATFORM)
#define PIN_R_OFFSET_CPU_DVFS	(PIN_S_OFFSET_CPU_DVFS + PIN_S_SIZE_CPU_DVFS)
#define PIN_R_OFFSET_FHCTL	(PIN_S_OFFSET_FHCTL + PIN_S_SIZE_FHCTL)
#define PIN_R_OFFSET_MCDI	(PIN_S_OFFSET_MCDI + PIN_S_SIZE_MCDI)
#define PIN_R_OFFSET_SUSPEND	(PIN_S_OFFSET_SUSPEND + PIN_S_SIZE_SUSPEND)
#define PIN_R_OFFSET_SMET       (PIN_S_OFFSET_SMET + PIN_S_SIZE_SMET)
#define PIN_R_OFFSET_RMET       (PIN_S_OFFSET_RMET + PIN_S_SIZE_RMET)
#define PIN_R_OFFSET_EEMSN      (PIN_S_OFFSET_EEMSN + PIN_S_SIZE_EEMSN)

extern struct mtk_ipi_device mcupm_ipidev;

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
struct mtk_mbox_info mcupm_mbox_table[MCUPM_MBOX_TOTAL] = {
	{0, 0, 0, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 1, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 2, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 3, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 4, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 5, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 6, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
	{0, 0, 7, MBOX_TABLE_SIZE, 1, 1, 0, 0, 0, 0, 0, 0, 0,
		{ { { { 0 } } } }, {0, 0, 0} },
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
struct mtk_mbox_pin_send mcupm_mbox_pin_send[] = {
	{0, PIN_S_OFFSET_PLATFORM, 1, 0, PIN_S_MSG_SIZE_PLATFORM,
		0, CH_S_PLATFORM,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{1, PIN_S_OFFSET_CPU_DVFS, 1, 0, PIN_S_MSG_SIZE_CPU_DVFS,
		1, CH_S_CPU_DVFS,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{2, PIN_S_OFFSET_FHCTL, 1, 0, PIN_S_MSG_SIZE_FHCTL,
		2, CH_S_FHCTL,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{3, PIN_S_OFFSET_MCDI, 1, 0, PIN_S_MSG_SIZE_MCDI,
		3, CH_S_MCDI,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{4, PIN_S_OFFSET_SUSPEND, 1, 0, PIN_S_MSG_SIZE_SUSPEND,
		4, CH_S_SUSPEND,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{5, PIN_S_OFFSET_RMET, 0, 0, PIN_S_MSG_SIZE_RMET,
		5, IPIR_C_MET,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{6, PIN_S_OFFSET_SMET, 1, 0, PIN_S_MSG_SIZE_SMET,
		6, IPIS_C_MET,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
	{7, PIN_S_OFFSET_EEMSN, 1, 0, PIN_S_MSG_SIZE_EEMSN,
		7, CH_S_EEMSN,
		{ { 0 } }, { 0 }, { { { { 0 } } } } },
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
struct mtk_mbox_pin_recv mcupm_mbox_pin_recv[] = {
	{0, PIN_R_OFFSET_PLATFORM, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_PLATFORM, 0,
		CH_S_PLATFORM, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{1, PIN_R_OFFSET_CPU_DVFS, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_CPU_DVFS, 1,
		CH_S_CPU_DVFS, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{2, PIN_R_OFFSET_FHCTL, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_FHCTL, 2,
		CH_S_FHCTL, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{3, PIN_R_OFFSET_MCDI, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_MCDI, 3,
		CH_S_MCDI, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{4, PIN_R_OFFSET_SUSPEND, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_SUSPEND, 4,
		CH_S_SUSPEND, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{5, PIN_R_OFFSET_SMET, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_RMET, 5,
		IPIR_C_MET, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{6, PIN_R_OFFSET_RMET, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_SMET, 6,
		IPIS_C_MET, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
	{7, PIN_R_OFFSET_EEMSN, 0, 0, 1, 0,
		PIN_R_MSG_SIZE_EEMSN, 7,
		CH_S_EEMSN, { 0 }, 0, 0, 0, { { { { 0 } } } },
		{0, 0, 0, 0, 0, 0} },
};

#define MCUPM_TOTAL_SEND_PIN     (sizeof(mcupm_mbox_pin_send) \
				  / sizeof(struct mtk_mbox_pin_send))
#define MCUPM_TOTAL_RECV_PIN     (sizeof(mcupm_mbox_pin_recv) \
				  / sizeof(struct mtk_mbox_pin_recv))

struct mtk_mbox_device mcupm_mboxdev = {
	.name = "mcupm_mboxdev",
	.pin_recv_table = &mcupm_mbox_pin_recv[0],
	.pin_send_table = &mcupm_mbox_pin_send[0],
	.info_table = &mcupm_mbox_table[0],
	.count = MCUPM_MBOX_TOTAL,
	.recv_count = MCUPM_TOTAL_RECV_PIN,
	.send_count = MCUPM_TOTAL_SEND_PIN,
};

struct mtk_ipi_device mcupm_ipidev = {
	.name = "mcupm_ipidev",
	.id = IPI_DEV_MCUPM,
	.mbdev = &mcupm_mboxdev,
};
EXPORT_SYMBOL_GPL(mcupm_ipidev);

#endif
