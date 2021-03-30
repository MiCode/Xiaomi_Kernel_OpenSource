/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef __ISPV2_IOPARAM_H__
#define __ISPV2_IOPARAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif


/* Command Reserved Types */
#define CAM_RESERVED_OPCODE_BASE    0x2000
#define CAM_RESERVED_POWERUP_EX     (CAM_RESERVED_OPCODE_BASE+0x1)

/* PowerSetting Config Value properties */
#define POWER_CFG_VAL_TYPE_MASK    0xF000
#define POWER_CFG_VAL_TYPE_EX      0x2000
#define POWER_CFG_VAL_MASK         0x000F


#define AL6021_IOC_MAGIC 'Q'
#define AL6021_PRIVATE    168

#define AL6021_PREAMBLE_LEN		2 // words

#define AL6021_OP_READ_MASK		0x10
#define AL6021_OP_READ_MASK_n		0xef
#define AL6021_OP_SPI_MASK			0x80
#define AL6021_OP_SPI_MASK_n		0x7f

#define AL6021_OP_WRITABLE			0x0f
#define AL6021_OP_GPIO_WRITE		0x0e
#define AL6021_OP_GPIO_READ			0x0d

#define AL6021_OP_SPI_WRITE_REG	0x89
#define AL6021_OP_SPI_WRITE_MEM	0x85
#define AL6021_OP_SPI_READ_REG		(AL6021_OP_SPI_WRITE_REG  | AL6021_OP_READ_MASK)	// 0x99
#define AL6021_OP_SPI_READ_MEM	(AL6021_OP_SPI_WRITE_MEM  | AL6021_OP_READ_MASK)	// 0x95

#define al6021_op_is_read(_op)		((_op) & AL6021_OP_READ_MASK)
#define al6021_op_is_write(_op)		(!((_op) & AL6021_OP_READ_MASK))

#define INSTRUCTION_RANGE			0xffffff00

#define _PASS_2(line) _reserved_ ## line
#define _PASS_1(line) _PASS_2(line)
#define RESERVED _PASS_1(__LINE__)

typedef enum
{
	ISPV2EventExit = 0,
	ISPV2EventMipiRXErr,
	ISPV2EventMax,
} ISPV2ICEventType;

#pragma pack(1)
// ioctl parameter = linked-list of transactions
struct al6021_ioparam_s {
	union { // next transaction in linked-list
		struct al6021_ioparam_s *next;
		u64 _next;
	};
	u32 size; // size of transaction in bytes
	int result;
	u8 sub_protocol;
	union {
		struct {
			// 10-bit ID: 1 1 1 1 0 A9 A8 RW - A7 A6 A5 A4 A3 A2 A1 A0
			u8 slave_id_10[2];
		};
		struct {
			u8 RESERVED[1];
			u8 slave_id;	// 7-bit ID: A6 A5 A4 A3 A2 A1 A0 RW
		};
	};
	union /* structure of txn (transaction) */ {
		struct { // sub_protocol == 0 // 'e' protocol
			u8 op;
			union {
				u32 addr; // internal address in slave device
				u32 instruction; // treated as instruction if >= INSTRUCTION_RANGE
				struct {
					u8 RESERVED[3];
					u8 pin; // treated as pin number
				};
			};
			union {
				struct /* of WRITE transaction */ {
					u32 data_out[1];
				};
				struct /* of READ transaction */ {
					u32 preamble[AL6021_PREAMBLE_LEN];
					u32 data_in[1];
				};
				u32 pin_value;
			};
		}; /* prot_e */ // referred as an anonymous structure

		struct { // sub_protocol == 1 // 'a' protocol
			u8 op;
			u16 addr; // internal address in slave device
			u8 RESERVED[1];
		} prot_a;

		struct { // sub_protocol == 0xff // raw protocol
			u8 raw[1];
		}; /* prot_raw; */ // referred as an anonymous structure
	};
};
#pragma pack()

#define AL6021_SIZEOF_TXN_HEAD \
	((u8 *)&(((struct al6021_ioparam_s *)0)->data_out[0]) - \
	 (u8 *)&(((struct al6021_ioparam_s *)0)->op))

#define AL6021_SIZEOF_TXN_BASIC \
	(AL6021_SIZEOF_TXN_HEAD + \
	 sizeof(((struct al6021_ioparam_s *)0)->data_out[0]))

#define AL6021_SIZEOF_IOPARAM_HEAD \
	((u8 *)&(((struct al6021_ioparam_s *)0)->op) - \
	 (u8 *)&(((struct al6021_ioparam_s *)0)->next))

#define AL6021_SIZEOF_IOPARAM_BASIC \
	(AL6021_SIZEOF_IOPARAM_HEAD + \
	 AL6021_SIZEOF_TXN_BASIC)

#define AL6021_LENOF_DATA_IN(_iop) \
({ \
	struct al6021_ioparam_s *__iop = _iop; \
	u32 __len; \
	\
	if (__iop) \
		__len = __iop->size - \
			AL6021_SIZEOF_TXN_HEAD - \
			sizeof(__iop->preamble); \
	else \
		__len = 0; \
	\
	__len; \
})

#define AL6021_PWR_UP \
	_IO(AL6021_IOC_MAGIC, AL6021_PRIVATE + 1)
#define AL6021_PWR_DOWN \
	_IO(AL6021_IOC_MAGIC, AL6021_PRIVATE + 2)
#define AL6021_WRITE_DATA \
	_IOW(AL6021_IOC_MAGIC, AL6021_PRIVATE + 3, struct al6021_ioparam_s)
#define AL6021_READ_DATA \
	_IOR(AL6021_IOC_MAGIC, AL6021_PRIVATE + 4, struct al6021_ioparam_s)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __ISPV2_IOPARAM_H__
