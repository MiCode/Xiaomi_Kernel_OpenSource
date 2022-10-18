/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_PRESIL_HW_ACCESS_H_
#define _CAM_PRESIL_HW_ACCESS_H_

#include <linux/interrupt.h>

/*
 * enum cam_presil_err - return code from presil apis
 *
 * @CAM_PRESIL_SUCCESS         : Success
 * @CAM_PRESIL_FAILED          : Failed
 * @CAM_PRESIL_BLOCKED         : not presil hw
 * @CAM_PRESIL_BLOCKED_BOOTUP  : presil hw but at boot presil not connected
 *
 */
enum cam_presil_err {
	CAM_PRESIL_SUCCESS = 0x0,
	CAM_PRESIL_FAILED,
	CAM_PRESIL_BLOCKED,
	CAM_PRESIL_BLOCKED_BOOTUP,
};

/**
 * struct cam_presil_intr_reginfo - register received with irq
 * callback
 */
struct cam_presil_intr_reginfo
{
	uint32_t intr_en_off;
	uint32_t intr_status_off;
	uint32_t intr_en_val;
	uint32_t intr_status_val;
};

/**
 * struct cam_presil_intr_regwrinfo - reg val pair from pchost
 */
struct cam_presil_intr_regwrinfo
{
	void *   reg_off;
	uint32_t reg_val;
};

#define CAM_MODE_MAX_REG_CNT 25

/**
 * struct cam_presil_irq_data - data received along with irq cb
 * from pchost
 */
struct cam_presil_irq_data
{
	uint32_t                         irq_num;
	uint32_t                         irq_reg_count;
	struct cam_presil_intr_reginfo   intr_reg[CAM_MODE_MAX_REG_CNT];
	uint32_t                         irq_wr_count;
	struct cam_presil_intr_regwrinfo intr_wr_reg[CAM_MODE_MAX_REG_CNT];
	uint32_t                         magic;
};

/*
 *  cam_presil_subscribe_device_irq()
 *
 * @brief         :  Register for irq from presil framework.
 *
 * @irq_num       :  Unique irq number
 * @irq_handler   :  handler callback
 * @irq_priv_data :  Callback data
 * @irq_name      :  Irq name
 *
 * @return true or false.
 */
bool cam_presil_subscribe_device_irq(int irq_num, irq_handler_t irq_handler,
	void* irq_priv_data, const char *irq_name);

/*
 *  cam_presil_subscribe_device_irq()
 *
 * @brief   :  Un-Register for irq from presil framework.
 *
 * @irq_num :  Unique irq number
 *
 * @return true or false.
 */
bool cam_presil_unsubscribe_device_irq(int irq_num);

/*
 *  cam_presil_register_read()
 *
 * @brief   :  register read from presil hw.
 *
 * @addr    :  Register offset
 * @pValue  :  Value read from hw
 *
 * @return:  Success or Failure
 */
int cam_presil_register_read(void *addr, uint32_t *pValue);

/*
 *  cam_presil_register_write()
 *
 * @brief   :  register write to presil hw.
 *
 * @addr    :  Register offset
 * @pValue  :  Value to write to hw
 * @flags   :  Flags
 *
 * @return:  Success or Failure
 */
int cam_presil_register_write(void *addr, uint32_t value, uint32_t flags);

/*
 *  cam_presil_send_buffer()
 *
 * @brief        :  Copy buffer content to presil hw memory.
 *
 * @dma_buf_uint :  Not fd , it is dma_buf ptr to be sent to
 * 	presil umd daemon
 * @mmu_hdl      :  Iommu handle
 * @offset       :  Offset to start copy
 * @size         :  Size of copy
 * @addr32       :  Iova to start copy at
 *
 * @return:  Success or Failure
 */
int cam_presil_send_buffer(uint64_t dma_buf_uint, int mmu_hdl, uint32_t offset,
	uint32_t size, uint32_t addr32);

/*
 *  cam_presil_retrieve_buffer()
 *
 * @brief        :  Copy buffer content back from presil hw memory.
 *
 * @dma_buf_uint :  Not fd , it is dma_buf ptr to be sent to
 * 	presil umd daemon
 * @mmu_hdl      :  Iommu handle
 * @offset       :  Offset to start copy
 * @size         :  Size of copy
 * @addr32       :  Iova to start copy at
 *
 * @return:  Success or Failure
 */
int cam_presil_retrieve_buffer(uint64_t dma_buf_uint,
	int mmu_hdl, uint32_t offset, uint32_t size, uint32_t addr32);

/*
 *  cam_presil_readl_poll_timeout()
 *
 * @brief         :  Custom register poll function for presil hw.
 *
 * @mem_address   :  Reg offset to poll
 * @val           :  Value to compare
 * @max_try_count :  Max number of tries
 * @interval_msec :  Interval between tries
 *
 * @return:  Success or Failure
 */
int cam_presil_readl_poll_timeout(void __iomem *mem_address, uint32_t val,
	int max_try_count, int interval_msec);

/*
 *  cam_presil_hfi_write_cmd()
 *
 * @brief   :  Write HFI command to presil hw.
 *
 * @addr    :  Pointer to HFI command
 * @cmdlen  :  Length
 *
 * @return:  Success or Failure
 */
int cam_presil_hfi_write_cmd(void *addr, uint32_t cmdlen);

/*
 *  cam_presil_hfi_read_message()
 *
 * @brief         :  Read HFI response message from presil hw.
 *
 * @pmsg          :  Pointer to HFI message buffer
 * @q_id          :  Length
 * @words_read    :  Response message
 * @interval_msec :  Interval between tries
 *
 * @return:  Success or Failure
 */
int cam_presil_hfi_read_message(uint32_t *pmsg, uint8_t q_id,
	uint32_t *words_read);

/**
 * @brief : API to check if camera driver running in presil
 *        enabled mode
 *
 * @return true or false.
 */
bool cam_presil_mode_enabled(void);
#endif /* _CAM_PRESIL_HW_ACCESS_H_ */
