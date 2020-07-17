/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __HFI_COMMON_H__
#define __HFI_COMMON_H__

#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/spinlock.h>
#include <linux/clk-provider.h>
#include "vidc_hfi_api.h"
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"
#include "vidc_hfi.h"
#include "msm_vidc_resources.h"
#include "hfi_packetization.h"
#include "msm_vidc_bus.h"

#define HFI_MASK_QHDR_TX_TYPE			0xFF000000
#define HFI_MASK_QHDR_RX_TYPE			0x00FF0000
#define HFI_MASK_QHDR_PRI_TYPE			0x0000FF00
#define HFI_MASK_QHDR_Q_ID_TYPE			0x000000FF
#define HFI_Q_ID_HOST_TO_CTRL_CMD_Q		0x00
#define HFI_Q_ID_CTRL_TO_HOST_MSG_Q		0x01
#define HFI_Q_ID_CTRL_TO_HOST_DEBUG_Q	0x02
#define HFI_MASK_QHDR_STATUS			0x000000FF

#define VIDC_MAX_UNCOMPRESSED_FMT_PLANES	3

#define VIDC_IFACEQ_NUMQ					3
#define VIDC_IFACEQ_CMDQ_IDX				0
#define VIDC_IFACEQ_MSGQ_IDX				1
#define VIDC_IFACEQ_DBGQ_IDX				2
#define VIDC_IFACEQ_MAX_BUF_COUNT			50
#define VIDC_IFACE_MAX_PARALLEL_CLNTS		16
#define VIDC_IFACEQ_DFLT_QHDR				0x01010000

#define VIDC_MAX_NAME_LENGTH 64
#define VIDC_MAX_PC_SKIP_COUNT 10
#define VIDC_MAX_SUBCACHES 4
#define VIDC_MAX_SUBCACHE_SIZE 52

struct hfi_queue_table_header {
	u32 qtbl_version;
	u32 qtbl_size;
	u32 qtbl_qhdr0_offset;
	u32 qtbl_qhdr_size;
	u32 qtbl_num_q;
	u32 qtbl_num_active_q;
	void *device_addr;
	char name[256];
};

struct hfi_queue_header {
	u32 qhdr_status;
	u32 qhdr_start_addr;
	u32 qhdr_type;
	u32 qhdr_q_size;
	u32 qhdr_pkt_size;
	u32 qhdr_pkt_drop_cnt;
	u32 qhdr_rx_wm;
	u32 qhdr_tx_wm;
	u32 qhdr_rx_req;
	u32 qhdr_tx_req;
	u32 qhdr_rx_irq_status;
	u32 qhdr_tx_irq_status;
	u32 qhdr_read_idx;
	u32 qhdr_write_idx;
};

struct hfi_mem_map_table {
	u32 mem_map_num_entries;
	u32 mem_map_table_base_addr;
};

struct hfi_mem_map {
	u32 virtual_addr;
	u32 physical_addr;
	u32 size;
	u32 attr;
};

#define VIDC_IFACEQ_TABLE_SIZE (sizeof(struct hfi_queue_table_header) \
	+ sizeof(struct hfi_queue_header) * VIDC_IFACEQ_NUMQ)

#define VIDC_IFACEQ_QUEUE_SIZE	(VIDC_IFACEQ_MAX_PKT_SIZE *  \
	VIDC_IFACEQ_MAX_BUF_COUNT * VIDC_IFACE_MAX_PARALLEL_CLNTS)

#define VIDC_IFACEQ_GET_QHDR_START_ADDR(ptr, i)     \
	(void *)((ptr + sizeof(struct hfi_queue_table_header)) + \
		(i * sizeof(struct hfi_queue_header)))

#define QDSS_SIZE 4096
#define SFR_SIZE 4096

#define QUEUE_SIZE (VIDC_IFACEQ_TABLE_SIZE + \
	(VIDC_IFACEQ_QUEUE_SIZE * VIDC_IFACEQ_NUMQ))

#define ALIGNED_QDSS_SIZE ALIGN(QDSS_SIZE, SZ_4K)
#define ALIGNED_SFR_SIZE ALIGN(SFR_SIZE, SZ_4K)
#define ALIGNED_QUEUE_SIZE ALIGN(QUEUE_SIZE, SZ_4K)
#define SHARED_QSIZE ALIGN(ALIGNED_SFR_SIZE + ALIGNED_QUEUE_SIZE + \
			ALIGNED_QDSS_SIZE, SZ_1M)

enum vidc_hw_reg {
	VIDC_HWREG_CTRL_STATUS =  0x1,
	VIDC_HWREG_QTBL_INFO =  0x2,
	VIDC_HWREG_QTBL_ADDR =  0x3,
	VIDC_HWREG_CTRLR_RESET =  0x4,
	VIDC_HWREG_IFACEQ_FWRXREQ =  0x5,
	VIDC_HWREG_IFACEQ_FWTXREQ =  0x6,
	VIDC_HWREG_VHI_SOFTINTEN =  0x7,
	VIDC_HWREG_VHI_SOFTINTSTATUS =  0x8,
	VIDC_HWREG_VHI_SOFTINTCLR =  0x9,
	VIDC_HWREG_HVI_SOFTINTEN =  0xA,
};

struct vidc_mem_addr {
	u32 align_device_addr;
	u8 *align_virtual_addr;
	u32 mem_size;
	struct msm_smem mem_data;
};

struct vidc_iface_q_info {
	void *q_hdr;
	struct vidc_mem_addr q_array;
};

/*
 * These are helper macros to iterate over various lists within
 * venus_hfi_device->res.  The intention is to cut down on a lot of boiler-plate
 * code
 */

/* Read as "for each 'thing' in a set of 'thingies'" */
#define venus_hfi_for_each_thing(__device, __thing, __thingy) \
	venus_hfi_for_each_thing_continue(__device, __thing, __thingy, 0)

#define venus_hfi_for_each_thing_reverse(__device, __thing, __thingy) \
	venus_hfi_for_each_thing_reverse_continue(__device, __thing, __thingy, \
			(__device)->res->__thingy##_set.count - 1)

/* TODO: the __from parameter technically not required since we can figure it
 * out with some pointer magic (i.e. __thing - __thing##_tbl[0]).  If this macro
 * sees extensive use, probably worth cleaning it up but for now omitting it
 * since it introduces unnecessary complexity.
 */
#define venus_hfi_for_each_thing_continue(__device, __thing, __thingy, __from) \
	for (__thing = &(__device)->res->\
			__thingy##_set.__thingy##_tbl[__from]; \
		__thing < &(__device)->res->__thingy##_set.__thingy##_tbl[0] + \
			((__device)->res->__thingy##_set.count - __from); \
		++__thing)

#define venus_hfi_for_each_thing_reverse_continue(__device, __thing, __thingy, \
		__from) \
	for (__thing = &(__device)->res->\
			__thingy##_set.__thingy##_tbl[__from]; \
		__thing >= &(__device)->res->__thingy##_set.__thingy##_tbl[0]; \
		--__thing)

/* Regular set helpers */
#define venus_hfi_for_each_regulator(__device, __rinfo) \
	venus_hfi_for_each_thing(__device, __rinfo, regulator)

#define venus_hfi_for_each_regulator_reverse(__device, __rinfo) \
	venus_hfi_for_each_thing_reverse(__device, __rinfo, regulator)

#define venus_hfi_for_each_regulator_reverse_continue(__device, __rinfo, \
		__from) \
	venus_hfi_for_each_thing_reverse_continue(__device, __rinfo, \
			regulator, __from)

/* Clock set helpers */
#define venus_hfi_for_each_clock(__device, __cinfo) \
	venus_hfi_for_each_thing(__device, __cinfo, clock)

#define venus_hfi_for_each_clock_reverse(__device, __cinfo) \
	venus_hfi_for_each_thing_reverse(__device, __cinfo, clock)

#define venus_hfi_for_each_clock_reverse_continue(__device, __rinfo, \
		__from) \
	venus_hfi_for_each_thing_reverse_continue(__device, __rinfo, \
			clock, __from)

/* Bus set helpers */
#define venus_hfi_for_each_bus(__device, __binfo) \
	venus_hfi_for_each_thing(__device, __binfo, bus)
#define venus_hfi_for_each_bus_reverse(__device, __binfo) \
	venus_hfi_for_each_thing_reverse(__device, __binfo, bus)

/* Subcache set helpers */
#define venus_hfi_for_each_subcache(__device, __sinfo) \
	venus_hfi_for_each_thing(__device, __sinfo, subcache)
#define venus_hfi_for_each_subcache_reverse(__device, __sinfo) \
	venus_hfi_for_each_thing_reverse(__device, __sinfo, subcache)

#define call_venus_op(d, op, ...)			\
	(((d) && (d)->vpu_ops && (d)->vpu_ops->op) ? \
	((d)->vpu_ops->op(__VA_ARGS__)):0)

/* Internal data used in vidc_hal not exposed to msm_vidc*/
struct hal_data {
	u32 irq;
	phys_addr_t firmware_base;
	u8 __iomem *register_base;
	u32 register_size;
};

struct venus_resources {
	struct msm_vidc_fw fw;
};

enum dsp_flag {
	DSP_INIT = BIT(0),
	DSP_SUSPEND = BIT(1),
};

enum venus_hfi_state {
	VENUS_STATE_DEINIT = 1,
	VENUS_STATE_INIT,
};

enum reset_state {
	INIT = 1,
	ASSERT,
	DEASSERT,
};

struct venus_hfi_device;

struct venus_hfi_vpu_ops {
	void (*interrupt_init)(struct venus_hfi_device *device, u32 sid);
	void (*setup_ucregion_memmap)(struct venus_hfi_device *device, u32 sid);
	void (*clock_config_on_enable)(struct venus_hfi_device *device,
		u32 sid);
	int (*reset_ahb2axi_bridge)(struct venus_hfi_device *device, u32 sid);
	void (*power_off)(struct venus_hfi_device *device);
	int (*prepare_pc)(struct venus_hfi_device *device);
	void (*raise_interrupt)(struct venus_hfi_device *device, u32 sid);
	bool (*watchdog)(u32 intr_status);
	void (*noc_error_info)(struct venus_hfi_device *device);
	void (*core_clear_interrupt)(struct venus_hfi_device *device);
	int (*boot_firmware)(struct venus_hfi_device *device, u32 sid);
};

struct venus_hfi_device {
	struct list_head list;
	struct list_head sess_head;
	u32 intr_status;
	u32 device_id;
	u32 clk_freq;
	u32 last_packet_type;
	struct msm_vidc_bus_data bus_vote;
	bool power_enabled;
	struct mutex lock;
	msm_vidc_callback callback;
	struct vidc_mem_addr iface_q_table;
	struct vidc_mem_addr dsp_iface_q_table;
	struct vidc_mem_addr qdss;
	struct vidc_mem_addr sfr;
	struct vidc_mem_addr mem_addr;
	struct vidc_iface_q_info iface_queues[VIDC_IFACEQ_NUMQ];
	struct vidc_iface_q_info dsp_iface_queues[VIDC_IFACEQ_NUMQ];
	u32 dsp_flags;
	struct hal_data *hal_data;
	struct workqueue_struct *vidc_workq;
	struct workqueue_struct *venus_pm_workq;
	int spur_count;
	int reg_count;
	struct venus_resources resources;
	struct msm_vidc_platform_resources *res;
	enum venus_hfi_state state;
	struct hfi_packetization_ops *pkt_ops;
	enum hfi_packetization_type packetization_type;
	struct msm_vidc_cb_info *response_pkt;
	u8 *raw_packet;
	struct pm_qos_request qos;
	unsigned int skip_pc_count;
	struct venus_hfi_vpu_ops *vpu_ops;
};

void venus_hfi_delete_device(void *device);

int venus_hfi_initialize(struct hfi_device *hdev, u32 device_id,
		struct msm_vidc_platform_resources *res,
		hfi_cmd_response_callback callback);

void __write_register(struct venus_hfi_device *device,
		u32 reg, u32 value, u32 sid);
int __read_register(struct venus_hfi_device *device, u32 reg, u32 sid);
void __disable_unprepare_clks(struct venus_hfi_device *device);
int __disable_regulators(struct venus_hfi_device *device);
int __unvote_buses(struct venus_hfi_device *device, u32 sid);
int __reset_ahb2axi_bridge_common(struct venus_hfi_device *device, u32 sid);
int __prepare_pc(struct venus_hfi_device *device);

/* AR50 specific */
void __interrupt_init_ar50(struct venus_hfi_device *device, u32 sid);
/* IRIS1 specific */
void __interrupt_init_iris1(struct venus_hfi_device *device, u32 sid);
void __setup_dsp_uc_memmap_iris1(struct venus_hfi_device *device);
void __clock_config_on_enable_iris1(struct venus_hfi_device *device,
		u32 sid);
void __setup_ucregion_memory_map_iris1(struct venus_hfi_device *device,
		u32 sid);
/* IRIS2 specific */
void __interrupt_init_iris2(struct venus_hfi_device *device, u32 sid);
void __setup_ucregion_memory_map_iris2(struct venus_hfi_device *device,
		u32 sid);
void __power_off_iris2(struct venus_hfi_device *device);
int __prepare_pc_iris2(struct venus_hfi_device *device);
void __raise_interrupt_iris2(struct venus_hfi_device *device, u32 sid);
bool __watchdog_iris2(u32 intr_status);
void __noc_error_info_iris2(struct venus_hfi_device *device);
void __core_clear_interrupt_iris2(struct venus_hfi_device *device);
int __boot_firmware_iris2(struct venus_hfi_device *device, u32 sid);

/* AR50_LITE specific */
void __interrupt_init_ar50_lt(struct venus_hfi_device *device, u32 sid);
void __setup_ucregion_memory_map_ar50_lt(struct venus_hfi_device *device, u32 sid);
void __power_off_ar50_lt(struct venus_hfi_device *device);
int __prepare_pc_ar50_lt(struct venus_hfi_device *device);
void __raise_interrupt_ar50_lt(struct venus_hfi_device *device, u32 sid);
void __core_clear_interrupt_ar50_lt(struct venus_hfi_device *device);
int __boot_firmware_ar50_lt(struct venus_hfi_device *device, u32 sid);

#endif
