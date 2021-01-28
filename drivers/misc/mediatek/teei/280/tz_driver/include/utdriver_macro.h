/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef UTDRIVER_MACRO_H
#define UTDRIVER_MACRO_H

#ifdef CONFIG_MICROTRUST_TUI_DRIVER
#ifndef TUI_SUPPORT
#define TUI_SUPPORT
#endif
#endif

#define START_STATUS			(0)
#define END_STATUS			(1)
#define VFS_SIZE			0x80000

#define FAST_CALL_TYPE			(0x100)
#define STANDARD_CALL_TYPE		(0x200)
#define TYPE_NONE			(0x300)

#define SHMEM_ENABLE			0
#define SHMEM_DISABLE			1

#define VDRV_MAX_SIZE			(0x80000)

#define FAST_CREAT_NQ			(0x40)
#define FAST_ACK_CREAT_NQ		(0x41)
#define FAST_CREAT_VDRV			(0x42)
#define FAST_ACK_CREAT_VDRV		(0x43)
#define FAST_CREAT_SYS_CTL		(0x44)
#define FAST_ACK_CREAT_SYS_CTL		(0x45)
#define FAST_CREAT_FDRV			(0x46)
#define FAST_ACK_CREAT_FDRV		(0x47)

#define NQ_CALL_TYPE			(0x60)
#define VDRV_CALL_TYPE			(0x61)
#define SCHD_CALL_TYPE			(0x62)
#define FDRV_ACK_TYPE			(0x63)

#define MAX_BUFF_SIZE			(4096)

#define VALID_TYPE			(1)
#define INVALID_TYPE			(0)

#define MESSAGE_SIZE			(4096)

#define NQ_SIZE				(4096)
#define NQ_BUFF_SIZE			(4096)
#define NQ_BLOCK_SIZE			(32)
#define BLOCK_MAX_COUNT			(NQ_BUFF_SIZE / NQ_BLOCK_SIZE - 1)

#define FP_BUFF_SIZE			(512 * 1024)

#define CANCEL_MESSAGE_SIZE		(4096)
#define KEYMASTER_BUFF_SIZE		(512 * 1024)

#ifdef TUI_SUPPORT
#define TUI_DISPLAY_SYS_NO		(160)
#define TUI_NOTICE_SYS_NO		(161)

#define TUI_NOTICE_BUFFER		(0x1000)
#define TUI_DISPLAY_BUFFER		(0x200000)
#endif

#define CTL_BUFF_SIZE			(4096)
#define VDRV_MAX_SIZE			(0x80000)
#define NQ_VALID			1

#define TEEI_VFS_NUM			0x8

#define MESSAGE_LENGTH			(4096)
#define MESSAGE_SIZE			(4096)

#define NEW_CAPI_CALL			0x1000
#define CAPI_CALL			0x01
#define FDRV_CALL			0x02
#define BDRV_CALL			0x03
#define SCHED_CALL			0x04
#define INIT_CMD_CALL			0x05
#define BOOT_STAGE2			0x06
#define INVOKE_FASTCALL			0x07
#define LOAD_TEE			0x08
#define BOOT_STAGE1			0x09
#define LOAD_FUNC			0x0A
#ifdef TUI_SUPPORT
#define POWER_DOWN_CALL			0x0B
#define I2C_REE_CALL			0x1E
#define I2C_TEE_CALL			0x1F
#endif
#define LOCK_PM_MUTEX			0x0C
#define UNLOCK_PM_MUTEX			0x0D
#define SWITCH_CORE			0x0E
#define MOVE_CORE			0x0F
#define NT_DUMP_T			(0x10)
#define VFS_SYS_NO			0x08
#define REETIME_SYS_NO			0x07
#define CANCEL_SYS_NO			110
#define IRQ_DELAY			1000


#define UT_BOOT_CORE			0
#define UT_SWITCH_CORE			4

#endif /* end of UTDRIVER_MACRO_H */
