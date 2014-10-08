/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef _H_MHI_MACROS
#define _H_MHI_MACROS

#define MHI_IPC_LOG_PAGES (50)
#define MHI_LOG_SIZE 0x1000
#define MHI_LINK_STABILITY_WAIT_MS 100
#define MHI_MAX_LINK_RETRIES 9
#define DT_WAIT_RETRIES 30
#define MHI_MAX_SUSPEND_RETRIES 1000
#define MHI_VERSION 0x01000000
#define ALIGNMENT_OFFSET 0xFFF
#define NR_OF_CMD_RINGS 1
#define EV_EL_PER_RING (256 + 16)
#define CMD_EL_PER_RING 128
#define ELEMENT_GAP 1
#define MHI_EPID 4
#define MHI_MAX_RESUME_TIMEOUT 5000
#define MHI_MAX_SUSPEND_TIMEOUT 5000
#define MHI_MAX_CMD_TIMEOUT 500

#define MAX_NR_MSI 4

#define EVENT_RINGS_ALLOCATED 4
#define PRIMARY_EVENT_RING 0
#define SOFTWARE_EV_RING 1
#define IPA_OUT_EV_RING 2
#define IPA_IN_EV_RING 3

#define PRIMARY_CMD_RING 0
#define MHI_WORK_Q_MAX_SIZE 128

#define MAX_XFER_WORK_ITEMS 100
#define MHI_MAX_SUPPORTED_DEVICES 1

#define MAX_NR_TRBS_PER_SOFT_CHAN 10
#define MAX_NR_TRBS_PER_HARD_CHAN (128 + 16)
#define MHI_PCIE_VENDOR_ID 0x17CB
#define MHI_PCIE_DEVICE_ID_9x35 0x0300
#define MHI_PCIE_DEVICE_ID_ZIRC 0x0301
#define TRB_MAX_DATA_SIZE 0x1000


#define MHI_DATA_SEG_WINDOW_START_ADDR 0x0ULL
#define MHI_DATA_SEG_WINDOW_END_ADDR 0x3E800000ULL

#define MHI_M1_ENTRY_DELAY_MS 100
#define MHI_XFER_DB_INTERVAL 8
#define MHI_EV_DB_INTERVAL 32

#define MHI_HANDLE_MAGIC 0x12344321
/* PCIe Device Info */

#define MHI_PCIE_DEVICE_BAR0_OFFSET_LOW (16)
#define MHI_PCIE_DEVICE_BAR0_OFFSET_HIGH (20)
#define MHI_PCIE_DEVICE_MANUFACT_ID_OFFSET (0)
#define MHI_PCIE_DEVICE_ID_OFFSET (2)

#define IS_HARDWARE_CHANNEL(_CHAN_NR) \
	(((enum MHI_CLIENT_CHANNEL)(_CHAN_NR) > \
	MHI_CLIENT_RESERVED_1_UPPER) && \
	 ((enum MHI_CLIENT_CHANNEL)(_CHAN_NR) < MHI_CLIENT_RESERVED_2_LOWER))

#define IS_SOFTWARE_CHANNEL(_CHAN_NR) \
	(((enum MHI_CLIENT_CHANNEL)(_CHAN_NR) >= 0) && \
	 ((enum MHI_CLIENT_CHANNEL)(_CHAN_NR) < MHI_CLIENT_RESERVED_1_LOWER))

#define IRQ_TO_MSI(_MHI_DEV_CTXT, _IRQ_NR) \
	((_IRQ_NR) - (_MHI_DEV_CTXT)->dev_info->core.irq_base)
#define MSI_TO_IRQ(_MHI_DEV_CTXT, _MSI_NR) \
	((_MHI_DEV_CTXT)->dev_info->core.irq_base + (_MSI_NR))
#define VALID_CHAN_NR(_CHAN_NR) (IS_HARDWARE_CHANNEL(_CHAN_NR) || \
		IS_SOFTWARE_CHANNEL(_CHAN_NR))

#define VALID_BUF(_BUF_ADDR, _BUF_LEN) \
	(((uintptr_t)(_BUF_ADDR) >=  MHI_DATA_SEG_WINDOW_START_ADDR) && \
		(((uintptr_t)(_BUF_ADDR) + (uintptr_t)(_BUF_LEN) < \
		MHI_DATA_SEG_WINDOW_END_ADDR)))

#define MHI_HW_INTMOD_VAL_MS 2
/* Timeout Values */
#define MHI_READY_STATUS_TIMEOUT_MS 50
#define MHI_THREAD_SLEEP_TIMEOUT_MS 20
#define MHI_RESUME_WAKE_RETRIES 20

/* Debugging Capabilities*/
#define MHI_DBG_MAX_EVENT_HISTORY 10

/* MHI Transfer Ring Elements 7.4.1*/
#define TX_TRB_LEN
#define MHI_TX_TRB_LEN__SHIFT (0)
#define MHI_TX_TRB_LEN__MASK (0xFFFF)

#define MHI_TX_TRB_SET_LEN(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = ((_PKT)->data_tx_pkt).buf_len; \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT)); \
	new_val |= (_VAL) << MHI_##_FIELD ## __SHIFT; \
	new_val &= (((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT)); \
	((_PKT)->data_tx_pkt).buf_len = new_val; \
}
#define MHI_TX_TRB_GET_LEN(_FIELD, _PKT) \
	(((_PKT)->data_tx_pkt).buf_len & (((MHI_##_FIELD ## __MASK) << \
			MHI_##_FIELD ## __SHIFT))); \

/* MHI Event Ring Elements 7.4.1*/
#define EV_TRB_CODE
#define MHI_EV_TRB_CODE__MASK (0xFF)
#define MHI_EV_TRB_CODE__SHIFT (24)
#define MHI_EV_READ_CODE(_FIELD, _PKT) (((_PKT->type).xfer_details >> \
			MHI_##_FIELD ## __SHIFT) & \
		MHI_ ##_FIELD ## __MASK)
#define EV_LEN
#define MHI_EV_LEN__MASK (0xFFFF)
#define MHI_EV_LEN__SHIFT (0)
#define MHI_EV_READ_LEN(_FIELD, _PKT) (((_PKT->xfer_event_pkt).xfer_details >> \
			MHI_##_FIELD ## __SHIFT) & \
		MHI_ ##_FIELD ## __MASK)

#define EV_CHID
#define MHI_EV_CHID__MASK (0xFF)
#define MHI_EV_CHID__SHIFT (24)
#define MHI_EV_READ_CHID(_FIELD, _PKT) ((((_PKT)->xfer_event_pkt).info >> \
			MHI_##_FIELD ## __SHIFT) & \
		MHI_ ##_FIELD ## __MASK)

#define EV_PTR
#define MHI_EV_PTR__MASK (0xFFFFFFFFFFFFFFFFULL)
#define MHI_EV_PTR__SHIFT (0)

#define MHI_EV_READ_PTR(_FIELD, _PKT) ((((_PKT)->xfer_event_pkt).xfer_ptr >> \
			MHI_##_FIELD ## __SHIFT) & \
		MHI_ ##_FIELD ## __MASK)

#define EV_STATE
#define MHI_EV_STATE__MASK (0xFF)
#define MHI_EV_STATE__SHIFT (24)
#define MHI_READ_STATE(_PKT) ((((_PKT)->state_change_event_pkt).state >> \
			MHI_EV_STATE__SHIFT) & \
		MHI_EV_STATE__MASK)

#define EXEC_ENV
#define MHI_EXEC_ENV__MASK (0xFF)
#define MHI_EXEC_ENV__SHIFT (24)
#define MHI_READ_EXEC_ENV(_PKT) ((((_PKT)->ee_event_pkt).exec_env>> \
			MHI_EXEC_ENV__SHIFT) & \
		MHI_EXEC_ENV__MASK)

/* MacroS for reading common "info" field for TRBs*/
#define TX_TRB_CHAIN
#define MHI_TX_TRB_CHAIN__SHIFT (0)
#define MHI_TX_TRB_CHAIN__MASK (0x1)
#define TX_TRB_IEOB
#define MHI_TX_TRB_IEOB__MASK (0x1)
#define MHI_TX_TRB_IEOB__SHIFT (8)
#define TX_TRB_IEOT
#define MHI_TX_TRB_IEOT__MASK (0x1)
#define MHI_TX_TRB_IEOT__SHIFT (9)
#define TX_TRB_BEI
#define MHI_TX_TRB_BEI__MASK (0x1)
#define MHI_TX_TRB_BEI__SHIFT (10)
#define TX_TRB_TYPE
#define MHI_TX_TRB_TYPE__MASK (0xFF)
#define MHI_TX_TRB_TYPE__SHIFT (16)

#define EV_TRB_TYPE
#define MHI_EV_TRB_TYPE__MASK (0xFF)
#define MHI_EV_TRB_TYPE__SHIFT (16)

#define CMD_TRB_TYPE
#define MHI_CMD_TRB_TYPE__MASK (0xFF)
#define MHI_CMD_TRB_TYPE__SHIFT (16)

#define CMD_TRB_CHID
#define MHI_CMD_TRB_CHID__MASK (0xFF)
#define MHI_CMD_TRB_CHID__SHIFT (24)

#define MHI_TRB_SET_INFO(_FIELD, _PKT, _VAL) \
	do { \
		u32 new_val = ((_PKT)->type).info; \
		new_val &= (~((MHI_##_FIELD ## __MASK) << \
					MHI_##_FIELD ## __SHIFT)); \
		new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
		(_PKT->type).info = new_val; \
	} while (0)

#define MHI_TRB_GET_INFO(_FIELD, _PKT, _DEST) \
	do { \
		_DEST = ((_PKT)->type).info; \
		_DEST &= (((MHI_##_FIELD ## __MASK) << \
				MHI_##_FIELD ## __SHIFT)); \
		_DEST >>= MHI_##_FIELD ## __SHIFT; \
	} while (0)

#define MHI_TRB_READ_INFO(_FIELD, _PKT) \
	((((_PKT)->type).info >> MHI_##_FIELD ## __SHIFT) & \
	 MHI_##_FIELD ## __MASK)

#define HIGH_WORD(_x) ((u32)((((u64)(_x)) >> 32) & 0xFFFFFFFF))
#define LOW_WORD(_x) ((u32)(((u64)(_x)) & 0xFFFFFFFF))

#define EVENT_RING_MSI_VEC
#define MHI_EVENT_RING_MSI_VEC__MASK (0xf)
#define MHI_EVENT_RING_MSI_VEC__SHIFT (2)
#define EVENT_RING_POLLING
#define MHI_EVENT_RING_POLLING__MASK (0x1)
#define MHI_EVENT_RING_POLLING__SHIFT (0)
#define EVENT_RING_STATE_FIELD
#define MHI_EVENT_RING_STATE_FIELD__MASK (0x1)
#define MHI_EVENT_RING_STATE_FIELD__SHIFT (1)

#define MHI_SET_EVENT_RING_INFO(_FIELD, _PKT, _VAL) \
{ \
	u32 new_val = (_PKT); \
	new_val &= (~((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	new_val |= _VAL << MHI_##_FIELD ## __SHIFT; \
	(_PKT) = new_val; \
};

#define MHI_GET_EVENT_RING_INFO(_FIELD, _PKT, _DEST) \
{ \
	_DEST = (_PKT); \
	_DEST &= (((MHI_##_FIELD ## __MASK) << MHI_##_FIELD ## __SHIFT));\
	_DEST >>= MHI_##_FIELD ## __SHIFT; \
};

#define EVENT_CTXT_INTMODT
#define MHI_EVENT_CTXT_INTMODT__MASK (0xFFFF)
#define MHI_EVENT_CTXT_INTMODT__SHIFT (16)
#define MHI_SET_EV_CTXT(_FIELD, _CTXT, _VAL) \
{ \
	u32 new_val = (_VAL << MHI_##_FIELD ## __SHIFT); \
	new_val &= (MHI_##_FIELD ## __MASK << MHI_##_FIELD ## __SHIFT); \
	(_CTXT)->mhi_intmodt &= (~((MHI_##_FIELD ## __MASK) << \
				MHI_##_FIELD ## __SHIFT)); \
	(_CTXT)->mhi_intmodt |= new_val; \
};

#define MHI_GET_EV_CTXT(_FIELD, _CTXT) \
	(((_CTXT)->mhi_intmodt >> MHI_##_FIELD ## __SHIFT) & \
				 MHI_##_FIELD ## __MASK)

#define MHI_READ_FIELD(_val, _mask, _shift) \
	do { \
		_val &= (u32)(_mask); \
		_val >>= (u32)(_shift); \
	} while (0)
#endif
