/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _NPU_HW_ACCESS_H
#define _NPU_HW_ACCESS_H

/*
 * Includes
 */
#include "npu_common.h"

/*
 * Defines
 */
#define IPC_MEM_OFFSET_FROM_SSTCM 0x00018000
#define SYS_CACHE_SCID 23

#define QFPROM_FMAX_REG_OFFSET_1 0x00006014
#define QFPROM_FMAX_BITS_MASK_1  0xF8000000
#define QFPROM_FMAX_BITS_SHIFT_1 27

#define QFPROM_FMAX_REG_OFFSET_2 0x00006018
#define QFPROM_FMAX_BITS_MASK_2  0x00000007
#define QFPROM_FMAX_BITS_SHIFT_2 5


#define REGW(npu_dev, off, val) npu_core_reg_write(npu_dev, off, val)
#define REGR(npu_dev, off) npu_core_reg_read(npu_dev, off)
#define MEMW(npu_dev, dst, src, size) npu_mem_write(npu_dev, (void *)(dst),\
	(void *)(src), size)
#define MEMR(npu_dev, src, dst, size) npu_mem_read(npu_dev, (void *)(src),\
	(void *)(dst), size)
#define IPC_ADDR npu_ipc_addr()
#define INTERRUPT_ACK(npu_dev, num) npu_interrupt_ack(npu_dev, num)
#define INTERRUPT_RAISE_NPU(npu_dev) npu_interrupt_raise_m0(npu_dev)
#define INTERRUPT_RAISE_DSP(npu_dev) npu_interrupt_raise_dsp(npu_dev)

/*
 * Data Structures
 */
struct npu_device;
struct npu_ion_buf_t;
struct npu_host_ctx;
struct npu_client;
typedef irqreturn_t (*intr_hdlr_fn)(int32_t irq, void *ptr);

/*
 * Function Prototypes
 */
uint32_t npu_core_reg_read(struct npu_device *npu_dev, uint32_t off);
void npu_core_reg_write(struct npu_device *npu_dev, uint32_t off, uint32_t val);
uint32_t npu_tcsr_reg_read(struct npu_device *npu_dev, uint32_t off);
uint32_t npu_apss_shared_reg_read(struct npu_device *npu_dev, uint32_t off);
void npu_apss_shared_reg_write(struct npu_device *npu_dev, uint32_t off,
	uint32_t val);
uint32_t npu_cc_reg_read(struct npu_device *npu_dev, uint32_t off);
void npu_cc_reg_write(struct npu_device *npu_dev, uint32_t off,
	uint32_t val);
void npu_mem_write(struct npu_device *npu_dev, void *dst, void *src,
	uint32_t size);
int32_t npu_mem_read(struct npu_device *npu_dev, void *src, void *dst,
	uint32_t size);
uint32_t npu_qfprom_reg_read(struct npu_device *npu_dev, uint32_t off);

int npu_mem_map(struct npu_client *client, int buf_hdl, uint32_t size,
	uint64_t *addr);
void npu_mem_unmap(struct npu_client *client, int buf_hdl, uint64_t addr);
void npu_mem_invalidate(struct npu_client *client, int buf_hdl);
bool npu_mem_verify_addr(struct npu_client *client, uint64_t addr);

void *npu_ipc_addr(void);
void npu_interrupt_ack(struct npu_device *npu_dev, uint32_t intr_num);
int32_t npu_interrupt_raise_m0(struct npu_device *npu_dev);
int32_t npu_interrupt_raise_dsp(struct npu_device *npu_dev);

uint8_t npu_hw_clk_gating_enabled(void);
uint8_t npu_hw_log_enabled(void);

int npu_enable_irq(struct npu_device *npu_dev);
void npu_disable_irq(struct npu_device *npu_dev);

int npu_enable_sys_cache(struct npu_device *npu_dev);
void npu_disable_sys_cache(struct npu_device *npu_dev);

void *subsystem_get_local(char *sub_system);
void subsystem_put_local(void *sub_system_handle);

void npu_process_log_message(struct npu_device *npu_dev, uint32_t *msg,
	uint32_t size);

#endif /* _NPU_HW_ACCESS_H*/
