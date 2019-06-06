/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __HAL_BTIFD_DMA_PUB_H_
#define __HAL_BTIFD_DMA_PUB_H_

#include <linux/dma-mapping.h>

#include "plat_common.h"

enum _ENUM_DMA_CTRL_ {
	DMA_CTRL_DISABLE = 0,
	DMA_CTRL_ENABLE = DMA_CTRL_DISABLE + 1,
	DMA_CTRL_BOTH,
};

/*****************************************************************************
 * FUNCTION
 *  hal_tx_dma_info_get
 * DESCRIPTION
 *  get btif tx dma channel's information
 * PARAMETERS
 *  dma_dir        [IN]         DMA's direction
 * RETURNS
 *  pointer to btif dma's information structure
 *****************************************************************************/
struct _MTK_DMA_INFO_STR_ *hal_btif_dma_info_get(enum _ENUM_DMA_DIR_ dma_dir);

/*****************************************************************************
 * FUNCTION
 *  hal_btif_dma_hw_init
 * DESCRIPTION
 *  control clock output enable/disable of DMA module
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_btif_dma_hw_init(struct _MTK_DMA_INFO_STR_ *p_dma_info);

/*****************************************************************************
 * FUNCTION
 *  hal_btif_clk_ctrl
 * DESCRIPTION
 *  control clock output enable/disable of DMA module
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_btif_dma_clk_ctrl(struct _MTK_DMA_INFO_STR_ *p_dma_info,
			  enum _ENUM_CLOCK_CTRL_ flag);

/*****************************************************************************
 * FUNCTION
 *  hal_tx_dma_ctrl
 * DESCRIPTION
 *  enable/disable Tx DMA channel
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 *  ctrl_id      [IN]        enable/disable ID
 *  dma_dir      [IN]        DMA's direction
 * RETURNS
 *  0 means success; negative means fail
 *****************************************************************************/
int hal_btif_dma_ctrl(struct _MTK_DMA_INFO_STR_ *p_dma_info,
		      enum _ENUM_DMA_CTRL_  ctrl_id);

/*****************************************************************************
 * FUNCTION
 *  hal_btif_dma_rx_cb_reg
 * DESCRIPTION
 * register rx callback function to dma module
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 *  rx_cb        [IN]        function pointer to btif
 * RETURNS
 *  0 means success; negative means fail
 *****************************************************************************/
int hal_btif_dma_rx_cb_reg(struct _MTK_DMA_INFO_STR_ *p_dma_info,
			   dma_rx_buf_write rx_cb);

/*****************************************************************************
 * FUNCTION
 *  hal_tx_vfifo_reset
 * DESCRIPTION
 *  reset tx virtual fifo information, except memory information
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 *  dma_dir      [IN]        DMA's direction
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_btif_vfifo_reset(struct _MTK_DMA_INFO_STR_ *p_dma_info);

/*****************************************************************************
 * FUNCTION
 *  hal_tx_dma_irq_handler
 * DESCRIPTION
 *  lower level tx interrupt handler
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_tx_dma_irq_handler(struct _MTK_DMA_INFO_STR_ *p_dma_info);

/*****************************************************************************
 * FUNCTION
 *  hal_dma_send_data
 * DESCRIPTION
 *  send data through btif in DMA mode
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 *  p_buf        [IN]        pointer to rx data buffer
 *  max_len      [IN]        tx buffer length
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_dma_send_data(struct _MTK_DMA_INFO_STR_ *p_dma_info,
		      const unsigned char *p_buf, const unsigned int buf_len);

/*****************************************************************************
 * FUNCTION
 *  hal_dma_is_tx_complete
 * DESCRIPTION
 *  get tx complete flag
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 * RETURNS
 *  true means tx complete, false means tx in process
 *****************************************************************************/
bool hal_dma_is_tx_complete(struct _MTK_DMA_INFO_STR_ *p_dma_info);

/*****************************************************************************
 * FUNCTION
 *  hal_dma_get_ava_room
 * DESCRIPTION
 *  get tx available room
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 * RETURNS
 *  available room  size
 *****************************************************************************/
int hal_dma_get_ava_room(struct _MTK_DMA_INFO_STR_ *p_dma_info);

/*****************************************************************************
 * FUNCTION
 *  hal_dma_is_tx_allow
 * DESCRIPTION
 *  is tx operation allowed by DMA
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 * RETURNS
 *  true if tx operation is allowed; false if tx is not allowed
 *****************************************************************************/
bool hal_dma_is_tx_allow(struct _MTK_DMA_INFO_STR_ *p_dma_info);

/*****************************************************************************
 * FUNCTION
 *  hal_rx_dma_irq_handler
 * DESCRIPTION
 *  lower level rx interrupt handler
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 *  p_buf        [IN/OUT]    pointer to rx data buffer
 *  max_len      [IN]        max length of rx buffer
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_rx_dma_irq_handler(struct _MTK_DMA_INFO_STR_ *p_dma_info,
			   unsigned char *p_buf, const unsigned int max_len);

/*****************************************************************************
 * FUNCTION
 *  hal_dma_dump_reg
 * DESCRIPTION
 *  dump BTIF module's information when needed
 * PARAMETERS
 *  p_dma_info   [IN]        pointer to BTIF dma channel's information
 *  flag         [IN]        register id flag
 * RETURNS
 *  0 means success, negative means fail
 *****************************************************************************/
int hal_dma_dump_reg(struct _MTK_DMA_INFO_STR_ *p_dma_info,
		     enum _ENUM_BTIF_REG_ID_ flag);

int hal_dma_pm_ops(struct _MTK_DMA_INFO_STR_ *p_dma_info,
		   enum _MTK_BTIF_PM_OPID_ opid);

int hal_dma_tx_has_pending(struct _MTK_DMA_INFO_STR_ *p_dma_info);
int hal_dma_rx_has_pending(struct _MTK_DMA_INFO_STR_ *p_dma_info);
int hal_rx_dma_lock(bool enable);

#endif /*__HAL_BTIFD_DMA_PUB_H_*/
