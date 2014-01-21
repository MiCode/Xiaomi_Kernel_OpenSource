/* Copyright (c) 2008-2009, 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>

/* Event indentifier format:
 * bit  31-28 is processor ID 8 => apps, 4 => Q6, 0 => modem
 * bits 27-16 are subsystem id (event base)
 * bits 15-0  are event id
 */

#define PROC                            0xF0000000
#define SUB                             0x0FFF0000
#define ID                              0x0000FFFF

#define SMEM_LOG_PROC_ID_MODEM          0x00000000
#define SMEM_LOG_PROC_ID_Q6             0x40000000
#define SMEM_LOG_PROC_ID_APPS           0x80000000
#define SMEM_LOG_PROC_ID_WCNSS          0xC0000000

#define SMEM_LOG_CONT                   0x10000000

#define SMEM_LOG_SMEM_EVENT_BASE        0x00020000
#define SMEM_LOG_ERROR_EVENT_BASE       0x00060000
#define SMEM_LOG_IPC_ROUTER_EVENT_BASE  0x000D0000
#define SMEM_LOG_QMI_CCI_EVENT_BASE     0x000E0000
#define SMEM_LOG_QMI_CSI_EVENT_BASE     0x000F0000
#define ERR_ERROR_FATAL                 (SMEM_LOG_ERROR_EVENT_BASE + 1)
#define ERR_ERROR_FATAL_TASK            (SMEM_LOG_ERROR_EVENT_BASE + 2)
#define SMEM_LOG_EVENT_CB               (SMEM_LOG_SMEM_EVENT_BASE +  0)
#define SMEM_LOG_EVENT_START            (SMEM_LOG_SMEM_EVENT_BASE +  1)
#define SMEM_LOG_EVENT_INIT             (SMEM_LOG_SMEM_EVENT_BASE +  2)
#define SMEM_LOG_EVENT_RUNNING          (SMEM_LOG_SMEM_EVENT_BASE +  3)
#define SMEM_LOG_EVENT_STOP             (SMEM_LOG_SMEM_EVENT_BASE +  4)
#define SMEM_LOG_EVENT_RESTART          (SMEM_LOG_SMEM_EVENT_BASE +  5)
#define SMEM_LOG_EVENT_SS               (SMEM_LOG_SMEM_EVENT_BASE +  6)
#define SMEM_LOG_EVENT_READ             (SMEM_LOG_SMEM_EVENT_BASE +  7)
#define SMEM_LOG_EVENT_WRITE            (SMEM_LOG_SMEM_EVENT_BASE +  8)
#define SMEM_LOG_EVENT_SIGS1            (SMEM_LOG_SMEM_EVENT_BASE +  9)
#define SMEM_LOG_EVENT_SIGS2            (SMEM_LOG_SMEM_EVENT_BASE + 10)
#define SMEM_LOG_EVENT_WRITE_DM         (SMEM_LOG_SMEM_EVENT_BASE + 11)
#define SMEM_LOG_EVENT_READ_DM          (SMEM_LOG_SMEM_EVENT_BASE + 12)
#define SMEM_LOG_EVENT_SKIP_DM          (SMEM_LOG_SMEM_EVENT_BASE + 13)
#define SMEM_LOG_EVENT_STOP_DM          (SMEM_LOG_SMEM_EVENT_BASE + 14)
#define SMEM_LOG_EVENT_ISR              (SMEM_LOG_SMEM_EVENT_BASE + 15)
#define SMEM_LOG_EVENT_TASK             (SMEM_LOG_SMEM_EVENT_BASE + 16)
#define SMEM_LOG_EVENT_RS               (SMEM_LOG_SMEM_EVENT_BASE + 17)

#ifdef CONFIG_MSM_SMEM_LOGGING
void smem_log_event(uint32_t id, uint32_t data1, uint32_t data2,
		    uint32_t data3);
void smem_log_event6(uint32_t id, uint32_t data1, uint32_t data2,
		     uint32_t data3, uint32_t data4, uint32_t data5,
		     uint32_t data6);
#else
void smem_log_event(uint32_t id, uint32_t data1, uint32_t data2,
		    uint32_t data3) { }
void smem_log_event6(uint32_t id, uint32_t data1, uint32_t data2,
		     uint32_t data3, uint32_t data4, uint32_t data5,
		     uint32_t data6) { }
#endif

