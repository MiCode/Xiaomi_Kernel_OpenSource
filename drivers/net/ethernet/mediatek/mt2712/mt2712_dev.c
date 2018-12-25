/* Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mt2712_yheader.h"
#include "mt2712_yapphdr.h"
#include "mt2712_yregacc.h"

static int tx_complete(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned int OWN;

	TX_NORMAL_DESC_TDES3_OWN_MLF_RD(txdesc->TDES3, OWN);
	if (OWN == 0)
		return 1;
	else
		return 0;
}

unsigned char get_tx_queue_count(void)
{
	unsigned char count;
	unsigned long MAC_HFR2;

	MAC_HFR2_REG_RD(MAC_HFR2);
	count = GET_VALUE(MAC_HFR2, MAC_HFR2_TXQCNT_LPOS, MAC_HFR2_TXQCNT_HPOS);

	return (count + 1);
}

unsigned char get_rx_queue_count(void)
{
	unsigned char count;
	unsigned long MAC_HFR2;

	MAC_HFR2_REG_RD(MAC_HFR2);
	count = GET_VALUE(MAC_HFR2, MAC_HFR2_RXQCNT_LPOS, MAC_HFR2_RXQCNT_HPOS);

	return (count + 1);
}

static unsigned int calculate_per_queue_fifo(unsigned long fifo_size, unsigned char queue_count)
{
	unsigned long q_fifo_size = 0;	/* calculated fifo size per queue */
	unsigned long p_fifo = e_256; /* per queue fifo size programmable value */

	/* calculate Tx/Rx fifo share per queue */
	switch (fifo_size) {
	case 0:
		q_fifo_size = FIFO_SIZE_B(128);
		break;
	case 1:
		q_fifo_size = FIFO_SIZE_B(256);
		break;
	case 2:
		q_fifo_size = FIFO_SIZE_B(512);
		break;
	case 3:
		q_fifo_size = FIFO_SIZE_KB(1);
		break;
	case 4:
		q_fifo_size = FIFO_SIZE_KB(2);
		break;
	case 5:
		q_fifo_size = FIFO_SIZE_KB(4);
		break;
	case 6:
		q_fifo_size = FIFO_SIZE_KB(8);
		break;
	case 7:
		q_fifo_size = FIFO_SIZE_KB(16);
		break;
	case 8:
		q_fifo_size = FIFO_SIZE_KB(32);
		break;
	case 9:
		q_fifo_size = FIFO_SIZE_KB(64);
		break;
	case 10:
		q_fifo_size = FIFO_SIZE_KB(128);
		break;
	case 11:
		q_fifo_size = FIFO_SIZE_KB(256);
		break;
	}

	q_fifo_size = q_fifo_size / queue_count;

	if (q_fifo_size >= FIFO_SIZE_KB(32))
		p_fifo = e_32k;
	else if (q_fifo_size >= FIFO_SIZE_KB(16))
		p_fifo = e_16k;
	else if (q_fifo_size >= FIFO_SIZE_KB(8))
		p_fifo = e_8k;
	else if (q_fifo_size >= FIFO_SIZE_KB(4))
		p_fifo = e_4k;
	else if (q_fifo_size >= FIFO_SIZE_KB(2))
		p_fifo = e_2k;
	else if (q_fifo_size >= FIFO_SIZE_KB(1))
		p_fifo = e_1k;
	else if (q_fifo_size >= FIFO_SIZE_B(512))
		p_fifo = e_512;
	else if (q_fifo_size >= FIFO_SIZE_B(256))
		p_fifo = e_256;

	return p_fifo;
}

static int configure_mtl_queue(unsigned int q_inx, struct prv_data *pdata)
{
	struct tx_queue *queue_data = GET_TX_QUEUE_PTR(q_inx);
	unsigned long retry_count = 1000;
	unsigned long vy_count;
	unsigned long MTL_QTOMR;
	unsigned int p_rx_fifo = e_256, p_tx_fifo = e_256;

	/*Flush Tx Queue */
	MTL_QTOMR_FTQ_UDFWR(q_inx, 0x1);

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count < retry_count) {
			vy_count++;
			mdelay(1);
		} else {
			return -Y_FAILURE;
		}
		MTL_QTOMR_REG_RD(q_inx, MTL_QTOMR);
		if (GET_VALUE(MTL_QTOMR, MTL_QTOMR_FTQ_LPOS, MTL_QTOMR_FTQ_HPOS)
				== 0) {
			break;
		}
	}

	/*Enable Store and Forward mode for TX */
	MTL_QTOMR_TSF_UDFWR(q_inx, 0x1);
	/* Program Tx operating mode */
	MTL_QTOMR_TXQEN_UDFWR(q_inx, queue_data->q_op_mode);
	/* Transmit Queue weight */
	MTL_QW_ISCQW_UDFWR(q_inx, (0x10 + q_inx));

	MTL_QROMR_FEP_UDFWR(q_inx, 0x1);

	p_rx_fifo = calculate_per_queue_fifo(pdata->hw_feat.rx_fifo_size, RX_QUEUE_CNT);
	p_tx_fifo = calculate_per_queue_fifo(pdata->hw_feat.tx_fifo_size, TX_QUEUE_CNT);

	/* Transmit/Receive queue fifo size programmed */
	MTL_QROMR_RQS_UDFWR(q_inx, p_rx_fifo);
	MTL_QTOMR_TQS_UDFWR(q_inx, p_tx_fifo);
	pr_err("Queue%d Tx fifo size %d, Rx fifo size %d\n",
	       q_inx, ((p_tx_fifo + 1) * 256), ((p_rx_fifo + 1) * 256));

	/* flow control will be used only if
	 * each channel gets 8KB or more fifo
	 */
	if (p_rx_fifo >= e_4k) {
		/* Enable Rx FLOW CTRL in MTL and MAC
		 * Programming is valid only if Rx fifo size is greater than
		 * or equal to 8k
		 */
		if ((pdata->flow_ctrl & MTK_FLOW_CTRL_TX) == MTK_FLOW_CTRL_TX) {
			MTL_QROMR_EHFC_UDFWR(q_inx, 0x1);
			/* Set Threshold for Activating Flow Contol space for min 2 frames
			 * ie, (1500 * 1) = 1500 bytes
			 *
			 * Set Threshold for Deactivating Flow Contol for space of
			 * min 1 frame (frame size 1500bytes) in receive fifo
			 */
			if (p_rx_fifo == e_4k) {
				/* This violates the above formula because of FIFO size limit
				 * therefore overflow may occur inspite of this
				 */
				MTL_QROMR_RFD_UDFWR(q_inx, 0x3); /* Full - 3K */
				MTL_QROMR_RFA_UDFWR(q_inx, 0x1); /* Full - 1.5K */
			} else if (p_rx_fifo == e_8k) {
				MTL_QROMR_RFD_UDFWR(q_inx, 0x6); /* Full - 4K */
				MTL_QROMR_RFA_UDFWR(q_inx, 0xA); /* Full - 6K */
			} else if (p_rx_fifo == e_16k) {
				MTL_QROMR_RFD_UDFWR(q_inx, 0x6); /* Full - 4K */
				MTL_QROMR_RFA_UDFWR(q_inx, 0x12); /* Full - 10K */
			} else if (p_rx_fifo == e_32k) {
				MTL_QROMR_RFD_UDFWR(q_inx, 0x6); /* Full - 4K */
				MTL_QROMR_RFA_UDFWR(q_inx, 0x1E); /* Full - 16K */
			}
		}
	}

	return Y_SUCCESS;
}

static int disable_tx_flow_ctrl(unsigned int q_inx)
{
	MAC_QTFCR_TFE_UDFWR(q_inx, 0);

	return Y_SUCCESS;
}

static int enable_tx_flow_ctrl(unsigned int q_inx)
{
	MAC_QTFCR_TFE_UDFWR(q_inx, 1);

	return Y_SUCCESS;
}

static int disable_rx_flow_ctrl(void)
{
	MAC_RFCR_RFE_UDFWR(0);

	return Y_SUCCESS;
}

static int enable_rx_flow_ctrl(void)
{
	MAC_RFCR_RFE_UDFWR(0x1);

	return Y_SUCCESS;
}

static int disable_mmc_interrupts(void)
{
	/* disable all TX interrupts */
	MMC_INTR_MASK_TX_REG_WR(0xffffffff);
	/* disable all RX interrupts */
	MMC_INTR_MASK_RX_REG_WR(0xffffffff);
	MMC_IPC_INTR_MASK_RX_REG_WR(0xffffffff); /* Disable MMC Rx Interrupts for IPC */

	return Y_SUCCESS;
}

static int config_mmc_counters(void)
{
	unsigned long MMC_CNTRL;

	/* set COUNTER RESET */
	/* set RESET ON READ */
	/* set COUNTER PRESET */
	/* set FULL_HALF PRESET */
	MMC_CNTRL_REG_RD(MMC_CNTRL);
	MMC_CNTRL = MMC_CNTRL & (unsigned long)(0x10a);
	MMC_CNTRL = MMC_CNTRL | ((0x1) << 0) | ((0x1) << 2) | ((0x1) << 4) | ((0x1) << 5);
	MMC_CNTRL_REG_WR(MMC_CNTRL);

	return Y_SUCCESS;
}

static int enable_mac_interrupts(void)
{
	unsigned long mac_imr;

	/* Enable following interrupts */
	/* RGSMIIIM - RGMII/SMII interrupt Enable */
	/* PCSLCHGIM -  PCS Link Status Interrupt Enable */
	/* PCSANCIM - PCS AN Completion Interrupt Enable */
	/* PMTIM - PMT Interrupt Enable */
	/* LPIIM - LPI Interrupt Enable */
	MAC_IMR_REG_RD(mac_imr);
	mac_imr = mac_imr & (unsigned long)(0x1008);
	mac_imr = mac_imr | ((0x1) << 0);
	MAC_IMR_REG_WR(mac_imr);

	return Y_SUCCESS;
}

static int configure_mac(struct prv_data *pdata)
{
	unsigned long MAC_MCR;
	unsigned int q_inx;

	for (q_inx = 0; q_inx < RX_QUEUE_CNT; q_inx++)
		MAC_RQC0R_RXQEN_UDFWR(q_inx, 0x2);

	/* Set Tx flow control parameters */
	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++) {
		/* set Pause Time */
		MAC_QTFCR_PT_UDFWR(q_inx, 0xffff);
		/* Assign priority for RX flow control */
		/* Assign priority for TX flow control */
		switch (q_inx) {
		case 0:
			MAC_TQPM0R_PSTQ0_UDFWR(0);
			MAC_RQC2R_PSRQ0_UDFWR(0x1 << q_inx);
			break;
		case 1:
			MAC_TQPM0R_PSTQ1_UDFWR(1);
			MAC_RQC2R_PSRQ1_UDFWR(0x1 << q_inx);
			break;
		case 2:
			MAC_TQPM0R_PSTQ2_UDFWR(2);
			MAC_RQC2R_PSRQ2_UDFWR(0x1 << q_inx);
			break;
		case 3:
			MAC_TQPM0R_PSTQ3_UDFWR(3);
			MAC_RQC2R_PSRQ3_UDFWR(0x1 << q_inx);
			break;
		case 4:
			MAC_TQPM1R_PSTQ4_UDFWR(4);
			MAC_RQC3R_PSRQ4_UDFWR(0x1 << q_inx);
			break;
		case 5:
			MAC_TQPM1R_PSTQ5_UDFWR(5);
			MAC_RQC3R_PSRQ5_UDFWR(0x1 << q_inx);
			break;
		case 6:
			MAC_TQPM1R_PSTQ6_UDFWR(6);
			MAC_RQC3R_PSRQ6_UDFWR(0x1 << q_inx);
			break;
		case 7:
			MAC_TQPM1R_PSTQ7_UDFWR(7);
			MAC_RQC3R_PSRQ7_UDFWR(0x1 << q_inx);
			break;
		}

		if ((pdata->flow_ctrl & MTK_FLOW_CTRL_TX) == MTK_FLOW_CTRL_TX)
			enable_tx_flow_ctrl(q_inx);
		else
			disable_tx_flow_ctrl(q_inx);
	}

	/* Set Rx flow control parameters */
	if ((pdata->flow_ctrl & MTK_FLOW_CTRL_RX) == MTK_FLOW_CTRL_RX)
		enable_rx_flow_ctrl();
	else
		disable_rx_flow_ctrl();

	/* update the MAC address */
	MAC_MA0HR_REG_WR(((pdata->dev->dev_addr[5] << 8) |
			(pdata->dev->dev_addr[4])));
	MAC_MA0LR_REG_WR(((pdata->dev->dev_addr[3] << 24) |
			(pdata->dev->dev_addr[2] << 16) |
			(pdata->dev->dev_addr[1] << 8) |
			(pdata->dev->dev_addr[0])));

	/*Enable MAC Transmit process */
	/*Enable MAC Receive process */
	/*Enable padding - disabled */
	/*Enable CRC stripping - disabled */
	MAC_MCR_REG_RD(MAC_MCR);
	MAC_MCR = MAC_MCR & (unsigned long)(0xffcfff7c);
	MAC_MCR = MAC_MCR | ((0x1) << 0) | ((0x1) << 1) | ((0x1) << 20) | ((0x1) << 21);
	MAC_MCR_REG_WR(MAC_MCR);

	/* disable all MMC intterrupt as MMC are managed in SW and
	 * registers are cleared on each READ eventually
	 */
	disable_mmc_interrupts();
	config_mmc_counters();

	enable_mac_interrupts();

	return Y_SUCCESS;
}

static int enable_dma_interrupts(unsigned int q_inx)
{
	unsigned int tmp;
	unsigned long DMA_SR;
	unsigned long DMA_IER;

	/* clear all the interrupts which are set */
	DMA_SR_REG_RD(q_inx, DMA_SR);
	tmp = DMA_SR;
	DMA_SR_REG_WR(q_inx, tmp);
	/* Enable following interrupts for Queue 0 */
	/* TXSE - Transmit Stopped Enable */
	/* RIE - Receive Interrupt Enable */
	/* RBUE - Receive Buffer Unavailable Enable  */
	/* RSE - Receive Stopped Enable */
	/* AIE - Abnormal Interrupt Summary Enable */
	/* NIE - Normal Interrupt Summary Enable */
	/* FBE - Fatal Bus Error Enable */
	DMA_IER_REG_RD(q_inx, DMA_IER);
	DMA_IER = DMA_IER & (unsigned long)(0x2e00);

	DMA_IER = DMA_IER | ((0x1) << 1) | ((0x1) << 2) |
	    ((0x1) << 6) | ((0x1) << 7) | ((0x1) << 8) | ((0x1) << 14) |
	    ((0x1) << 15) | ((0x1) << 12);

	/* TIE - Transmit Interrupt Enable */
	/* TBUE - Transmit Buffer Unavailable Enable */
	DMA_IER |= ((0x1) << 0) | ((0x1) << 2);

	DMA_IER_REG_WR(q_inx, DMA_IER);

	return Y_SUCCESS;
}

static int configure_dma_channel(unsigned int q_inx, struct prv_data *pdata)
{
	struct rx_wrapper_descriptor *rx_desc_data =
		GET_RX_WRAPPER_DESC(q_inx);

	/*Enable OSF mode */
	DMA_TCR_OSP_UDFWR(q_inx, 0x1);

	/*Select Rx Buffer size = 2048bytes */
	switch (pdata->rx_buffer_len) {
	case 16384:
		DMA_RCR_RBSZ_UDFWR(q_inx, 16384);
		break;
	case 8192:
		DMA_RCR_RBSZ_UDFWR(q_inx, 8192);
		break;
	case 4096:
		DMA_RCR_RBSZ_UDFWR(q_inx, 4096);
		break;
	default:		/* default is 2K */
		DMA_RCR_RBSZ_UDFWR(q_inx, 2048);
		break;
	}
	/* program RX watchdog timer */
	if (rx_desc_data->use_riwt)
		DMA_RIWTR_RWT_UDFWR(q_inx, rx_desc_data->rx_riwt);
	else
		DMA_RIWTR_RWT_UDFWR(q_inx, 0);
	pr_err("%s Rx watchdog timer\n",
	       (rx_desc_data->use_riwt ? "Enabled" : "Disabled"));

	enable_dma_interrupts(q_inx);
	/* set PBLX8 */
	DMA_CR_PBLX8_UDFWR(q_inx, 0x1);
	/* set TX PBL = 256 */
	DMA_TCR_PBL_UDFWR(q_inx, 32);
	/* set RX PBL = 256 */
	DMA_RCR_PBL_UDFWR(q_inx, 32);

	/* To get Best Performance */
	DMA_SBUS_BLEN16_UDFWR(1);
	DMA_SBUS_BLEN8_UDFWR(1);
	DMA_SBUS_BLEN4_UDFWR(1);
	DMA_SBUS_RD_OSR_LMT_UDFWR(2);

	/* start TX DMA */
	DMA_TCR_ST_UDFWR(q_inx, 0x1);

	/* start RX DMA */
	DMA_RCR_ST_UDFWR(q_inx, 0x1);

	return Y_SUCCESS;
}

static int yinit(struct prv_data *pdata)
{
	unsigned int q_inx;

	/* reset mmc counters */
	MMC_CNTRL_REG_WR(0x1);

	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++)
		configure_mtl_queue(q_inx, pdata);

	/* Mapping MTL Rx queue and DMA Rx channel. */
	MTL_RQDCM0R_REG_WR(0x3020100);
	MTL_RQDCM1R_REG_WR(0x7060504);

	configure_mac(pdata);

	/* Setting INCRx */
	DMA_SBUS_REG_WR(0x0);
	for (q_inx = 0; q_inx < TX_QUEUE_CNT; q_inx++)
		configure_dma_channel(q_inx, pdata);

	return Y_SUCCESS;
}

static int yexit(void)
{
	unsigned long retry_count = 1000;
	unsigned long vy_count;
	unsigned long DMA_BMR;

	/*issue a software reset */
	DMA_BMR_SWR_UDFWR(0x1);
	/*DELAY IMPLEMENTATION USING udelay() */
	usleep_range(10, 20);

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count < retry_count) {
			vy_count++;
			mdelay(1);
		} else {
			return -Y_FAILURE;
		}
		DMA_BMR_REG_RD(DMA_BMR);
		if (GET_VALUE(DMA_BMR, DMA_BMR_SWR_LPOS, DMA_BMR_SWR_HPOS) == 0)
			break;
	}

	return Y_SUCCESS;
}

static int tx_descriptor_reset(unsigned int idx, struct prv_data *pdata, unsigned int q_inx)
{
	struct s_TX_NORMAL_DESC *TX_NORMAL_DESC =
		GET_TX_DESC_PTR(q_inx, idx);

	/* update buffer 1 address pointer to zero */
	TX_NORMAL_DESC_TDES0_ML_WR(TX_NORMAL_DESC->TDES0, 0);
	/* update buffer 2 address pointer to zero */
	TX_NORMAL_DESC_TDES1_ML_WR(TX_NORMAL_DESC->TDES1, 0);
	/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
	TX_NORMAL_DESC_TDES2_ML_WR(TX_NORMAL_DESC->TDES2, 0);
	/* set all other control bits (OWN, CTXT, FD, LD, CPC, CIC etc) to zero */
	TX_NORMAL_DESC_TDES3_ML_WR(TX_NORMAL_DESC->TDES3, 0);

	return Y_SUCCESS;
}

/* \brief This sequence is used to reinitialize the RX descriptor fields,
 * so that device can reuse the descriptors
 * \param[in] idx
 * \param[in] pdata
 */

static void rx_descriptor_reset(unsigned int idx,
				struct prv_data *pdata,
				unsigned int inte,
				unsigned int q_inx)
{
	struct rx_buffer *buffer = GET_RX_BUF_PTR(q_inx, idx);
	struct s_RX_NORMAL_DESC *RX_NORMAL_DESC = GET_RX_DESC_PTR(q_inx, idx);

	memset(RX_NORMAL_DESC, 0, sizeof(struct s_RX_NORMAL_DESC));
	/* update buffer 1 address pointer */
	RX_NORMAL_DESC_RDES0_ML_WR(RX_NORMAL_DESC->RDES0, buffer->dma);
	/* set to zero */
	RX_NORMAL_DESC_RDES1_ML_WR(RX_NORMAL_DESC->RDES1, 0);

	/* set buffer 2 address pointer to zero */
	RX_NORMAL_DESC_RDES2_ML_WR(RX_NORMAL_DESC->RDES2, 0);
	/* set control bits - OWN, INTE and BUF1V */
	RX_NORMAL_DESC_RDES3_ML_WR(RX_NORMAL_DESC->RDES3, (0X81000000 | inte));
}

/* \brief This sequence is used to initialize the rx descriptors.
 * \param[in] pdata
 */

static void rx_descriptor_init(struct prv_data *pdata, unsigned int q_inx)
{
	struct rx_wrapper_descriptor *rx_desc_data =
	    GET_RX_WRAPPER_DESC(q_inx);
	struct rx_buffer *buffer =
	    GET_RX_BUF_PTR(q_inx, rx_desc_data->cur_rx);
	struct s_RX_NORMAL_DESC *RX_NORMAL_DESC =
	    GET_RX_DESC_PTR(q_inx, rx_desc_data->cur_rx);
	int i;
	int start_index = rx_desc_data->cur_rx;
	int last_index;

	/* initialize all desc */

	for (i = 0; i < RX_DESC_CNT; i++) {
		memset(RX_NORMAL_DESC, 0, sizeof(struct s_RX_NORMAL_DESC));
		/* update buffer 1 address pointer */
		RX_NORMAL_DESC_RDES0_ML_WR(RX_NORMAL_DESC->RDES0, buffer->dma);
		/* set to zero  */
		RX_NORMAL_DESC_RDES1_ML_WR(RX_NORMAL_DESC->RDES1, 0);

		/* set buffer 2 address pointer to zero */
		RX_NORMAL_DESC_RDES2_ML_WR(RX_NORMAL_DESC->RDES2, 0);
		/* set control bits - OWN, INTE and BUF1V */
		RX_NORMAL_DESC_RDES3_ML_WR(RX_NORMAL_DESC->RDES3, (0xc1000000));

		buffer->inte = (1 << 30);

		/* reconfigure INTE bit if RX watchdog timer is enabled */
		if (rx_desc_data->use_riwt) {
			if ((i % rx_desc_data->rx_coal_frames) != 0) {
				unsigned int RDES3 = 0;

				RX_NORMAL_DESC_RDES3_ML_RD(RX_NORMAL_DESC->RDES3, RDES3);
				/* reset INTE */
				RX_NORMAL_DESC_RDES3_ML_WR(RX_NORMAL_DESC->RDES3, (RDES3 & ~(1 << 30)));
				buffer->inte = 0;
			}
		}

		INCR_RX_DESC_INDEX(rx_desc_data->cur_rx, 1);
		RX_NORMAL_DESC =
			GET_RX_DESC_PTR(q_inx, rx_desc_data->cur_rx);
		buffer = GET_RX_BUF_PTR(q_inx, rx_desc_data->cur_rx);
	}
	/* update the total no of Rx descriptors count */
	DMA_RDRLR_REG_WR(q_inx, (RX_DESC_CNT - 1));
	/* update the Rx Descriptor Tail Pointer */
	last_index = GET_CURRENT_RCVD_LAST_DESC_INDEX(start_index, 0);
	DMA_RDTP_RPDR_REG_WR(q_inx, GET_RX_DESC_DMA_ADDR(q_inx, last_index));
	/* update the starting address of desc chain/ring */
	DMA_RDLAR_REG_WR(q_inx, GET_RX_DESC_DMA_ADDR(q_inx, start_index));
}

/* \brief This sequence is used to initialize the tx descriptors.
 * \param[in] pdata
 */

static void tx_descriptor_init(struct prv_data *pdata, unsigned int q_inx)
{
	struct tx_wrapper_descriptor *tx_desc_data =
		GET_TX_WRAPPER_DESC(q_inx);
	struct s_TX_NORMAL_DESC *TX_NORMAL_DESC =
		GET_TX_DESC_PTR(q_inx, tx_desc_data->cur_tx);
	int i;
	int start_index = tx_desc_data->cur_tx;

	/* initialze all descriptors. */

	for (i = 0; i < TX_DESC_CNT; i++) {
		/* update buffer 1 address pointer to zero */
		TX_NORMAL_DESC_TDES0_ML_WR(TX_NORMAL_DESC->TDES0, 0);
		/* update buffer 2 address pointer to zero */
		TX_NORMAL_DESC_TDES1_ML_WR(TX_NORMAL_DESC->TDES1, 0);
		/* set all other control bits (IC, TTSE, B2L & B1L) to zero */
		TX_NORMAL_DESC_TDES2_ML_WR(TX_NORMAL_DESC->TDES2, 0);
		/* set all other control bits (OWN, CTXT, FD, LD, CPC, CIC etc) to zero */
		TX_NORMAL_DESC_TDES3_ML_WR(TX_NORMAL_DESC->TDES3, 0);

		INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
		TX_NORMAL_DESC = GET_TX_DESC_PTR(q_inx, tx_desc_data->cur_tx);
	}
	/* update the total no of Tx descriptors count */
	DMA_TDRLR_REG_WR(q_inx, (TX_DESC_CNT - 1));
	/* update the starting address of desc chain/ring */
	DMA_TDLAR_REG_WR(q_inx, GET_TX_DESC_DMA_ADDR(q_inx, start_index));
}

static int get_tx_descriptor_last(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned long LD;

	/* check TDES3.LD bit */
	TX_NORMAL_DESC_TDES3_LD_MLF_RD(txdesc->TDES3, LD);
	if (LD == 1)
		return 1;
	else
		return 0;
}

static int get_tx_descriptor_ctxt(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned long CTXT;

	/* check TDES3.CTXT bit */
	TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(txdesc->TDES3, CTXT);
	if (CTXT == 1)
		return 1;
	else
		return 0;
}

static void update_rx_tail_ptr(unsigned int q_inx, unsigned int dma_addr)
{
	DMA_RDTP_RPDR_REG_WR(q_inx, dma_addr);
}

static int disable_rx_interrupt(unsigned int q_inx)
{
	DMA_IER_RBUE_UDFWR(q_inx, 0);
	DMA_IER_RIE_UDFWR(q_inx, 0);

	return Y_SUCCESS;
}

/* \brief This sequence is used to enable given DMA channel rx interrupts
 * \param[in] q_inx
 * \return Success or Failure
 * \retval  0 Success
 * \retval -1 Failure
 */

static int enable_rx_interrupt(unsigned int q_inx)
{
	DMA_IER_RBUE_UDFWR(q_inx, 0x1);
	DMA_IER_RIE_UDFWR(q_inx, 0x1);

	return Y_SUCCESS;
}

static int set_full_duplex(void)
{
	MAC_MCR_DM_UDFWR(0x1);

	return Y_SUCCESS;
}

static int set_half_duplex(void)
{
	MAC_MCR_DM_UDFWR(0);

	return Y_SUCCESS;
}

static int set_mii_speed_10(void)
{
	MAC_MCR_PS_UDFWR(0x1);
	MAC_MCR_FES_UDFWR(0);

	return Y_SUCCESS;
}

static int set_mii_speed_100(void)
{
	MAC_MCR_PS_UDFWR(0x1);
	MAC_MCR_FES_UDFWR(0x1);

	return Y_SUCCESS;
}

static int set_gmii_speed(void)
{
	MAC_MCR_PS_UDFWR(0);
	MAC_MCR_FES_UDFWR(0);

	return Y_SUCCESS;
}

static int write_phy_regs(int phy_id, int phy_reg, int phy_reg_data)
{
	unsigned long retry_count = 1000;
	unsigned long vy_count;
	unsigned long MAC_GMIIAR;

	/* wait for any previous MII read/write operation to complete */
	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count < retry_count) {
			vy_count++;
			mdelay(1);
		} else {
			return -Y_FAILURE;
		}
		MAC_GMIIAR_REG_RD(MAC_GMIIAR);
		if (GET_VALUE(MAC_GMIIAR, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0)
			break;
	}
	/* write the data */
	MAC_GMIIDR_GD_UDFWR(phy_reg_data);
	/* initiate the MII write operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select write operation */
	/* set busy bit */
	MAC_GMIIAR_REG_RD(MAC_GMIIAR);
	MAC_GMIIAR = MAC_GMIIAR & (unsigned long)(0x12);
	MAC_GMIIAR =
	    MAC_GMIIAR | ((phy_id) << 21) | ((phy_reg) << 16) | ((0x0) << 8)
	    | ((0x1) << 2) | ((0x1) << 0);
	MAC_GMIIAR_REG_WR(MAC_GMIIAR);

	/*DELAY IMPLEMENTATION USING udelay() */
	usleep_range(10, 20);
	/* wait for MII write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count < retry_count) {
			vy_count++;
			mdelay(1);
		} else {
			return -Y_FAILURE;
		}
		MAC_GMIIAR_REG_RD(MAC_GMIIAR);
		if (GET_VALUE(MAC_GMIIAR, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0)
			break;
	}

	return Y_SUCCESS;
}

/* \brief This sequence is used to read the phy registers
 * \param[in] phy_id
 * \param[in] phy_reg
 * \param[out] phy_reg_data
 * \return Success or Failure
 * \retval  0 Success
 * \retval -1 Failure
 */

static int read_phy_regs(int phy_id, int phy_reg, int *phy_reg_data)
{
	unsigned long retry_count = 1000;
	unsigned long vy_count;
	unsigned long MAC_GMIIAR;
	unsigned long MAC_GMIIDR;

	/* wait for any previous MII read/write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count < retry_count) {
			vy_count++;
			mdelay(1);
		} else {
			return -Y_FAILURE;
		}
		MAC_GMIIAR_REG_RD(MAC_GMIIAR);
		if (GET_VALUE(MAC_GMIIAR, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0)
			break;
	}
	/* initiate the MII read operation by updating desired */
	/* phy address/id (0 - 31) */
	/* phy register offset */
	/* CSR Clock Range (20 - 35MHz) */
	/* Select read operation */
	/* set busy bit */
	MAC_GMIIAR_REG_RD(MAC_GMIIAR);
	MAC_GMIIAR = MAC_GMIIAR & (unsigned long)(0x12);
	MAC_GMIIAR =
	    MAC_GMIIAR | ((phy_id) << 21) | ((phy_reg) << 16) | ((0x0) << 8)
	    | ((0x3) << 2) | ((0x1) << 0);
	MAC_GMIIAR_REG_WR(MAC_GMIIAR);

	/*DELAY IMPLEMENTATION USING udelay() */
	usleep_range(10, 20);
	/* wait for MII write operation to complete */

	/*Poll Until Poll Condition */
	vy_count = 0;
	while (1) {
		if (vy_count < retry_count) {
			vy_count++;
			mdelay(1);
		} else {
			return -Y_FAILURE;
		}
		MAC_GMIIAR_REG_RD(MAC_GMIIAR);
		if (GET_VALUE(MAC_GMIIAR, MAC_GMIIAR_GB_LPOS, MAC_GMIIAR_GB_HPOS) == 0)
			break;
	}
	/* read the data */
	MAC_GMIIDR_REG_RD(MAC_GMIIDR);
	*phy_reg_data = GET_VALUE(MAC_GMIIDR, MAC_GMIIDR_GD_LPOS, MAC_GMIIDR_GD_HPOS);

	return Y_SUCCESS;
}

static int drop_tx_status_enabled(void)
{
	unsigned long MTL_OMR;

	MTL_OMR_REG_RD(MTL_OMR);

	return GET_VALUE(MTL_OMR, MTL_OMR_DTXSTS_LPOS, MTL_OMR_DTXSTS_HPOS);
}

static int config_sub_second_increment(unsigned long ptp_clock)
{
	unsigned long val;
	unsigned long MAC_TCR;

	MAC_TCR_REG_RD(MAC_TCR);

	/* convert the PTP_CLOCK to nano second */
	/*  formula is : ((1/ptp_clock) * 1000000000) */
	/*  where, ptp_clock = 50MHz if FINE correction */
	/*  and ptp_clock = SYSCLOCK if COARSE correction */
	if (GET_VALUE(MAC_TCR, MAC_TCR_TSCFUPDT_LPOS, MAC_TCR_TSCFUPDT_HPOS) == 1)
		val = ((1 * 1000000000ull) / 50000000);
	else
		val = ((1 * 1000000000ull) / ptp_clock);

	/* 0.465ns accurecy */
	if (GET_VALUE(MAC_TCR, MAC_TCR_TSCTRLSSR_LPOS, MAC_TCR_TSCTRLSSR_HPOS) == 0)
		val = (val * 1000) / 465;

	MAC_SSIR_SSINC_UDFWR(val);

	return Y_SUCCESS;
}

static unsigned long long get_systime(void)
{
	unsigned long long ns;
	unsigned long mac_stnsr;
	unsigned long mac_stsr;

	MAC_STNSR_REG_RD(mac_stnsr);
	ns = GET_VALUE(mac_stnsr, MAC_STNSR_TSSS_LPOS, MAC_STNSR_TSSS_HPOS);
	/* convert sec/high time value to nanosecond */
	MAC_STSR_REG_RD(mac_stsr);
	ns = ns + (mac_stsr * 1000000000ull);

	return ns;
}

static int adjust_systime(unsigned int sec, unsigned int nsec, int add_sub, bool one_nsec_accuracy)
{
	unsigned long retry_count = 100000;
	unsigned long vy_count;
	unsigned long MAC_TCR;

	/* wait for previous(if any) time adjust/update to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retry_count)
			return -Y_FAILURE;

		MAC_TCR_REG_RD(MAC_TCR);
		if (GET_VALUE(MAC_TCR, MAC_TCR_TSUPDT_LPOS, MAC_TCR_TSUPDT_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	if (add_sub) {
	/* If the new sec value needs to be subtracted with
	 * the system time, then MAC_STSUR reg should be
	 * programmed with (2^32 <new_sec_value>)
	 */
	sec = (0x100000000ull - sec);

	/* If the new nsec value need to be subtracted with
	 * the system time, then MAC_STNSUR.TSSS field should be
	 * programmed with,
	 * (10^9 - <new_nsec_value>) if MAC_TCR.TSCTRLSSR is set or
	 * (2^31 - <new_nsec_value> if MAC_TCR.TSCTRLSSR is reset)
	 */
	if (one_nsec_accuracy)
		nsec = (0x3B9ACA00 - nsec);
	else
		nsec = (0X80000000 - nsec);
	}

	MAC_STSUR_REG_WR(sec);
	MAC_STNSUR_TSSS_UDFWR(nsec);
	MAC_STNSUR_ADDSUB_UDFWR(add_sub);

	/* issue command to initialize system time with the value */
	/* specified in MAC_STSUR and MAC_STNSUR. */
	MAC_TCR_TSUPDT_UDFWR(0x1);
	/* wait for present time initialize to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retry_count)
			return -Y_FAILURE;

		MAC_TCR_REG_RD(MAC_TCR);
		if (GET_VALUE(MAC_TCR, MAC_TCR_TSUPDT_LPOS, MAC_TCR_TSUPDT_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	return Y_SUCCESS;
}

static int config_addend(unsigned int data)
{
	unsigned long retry_count = 100000;
	unsigned long vy_count;
	unsigned long MAC_TCR;

	/* wait for previous(if any) added update to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retry_count)
			return -Y_FAILURE;

		MAC_TCR_REG_RD(MAC_TCR);
		if (GET_VALUE(MAC_TCR, MAC_TCR_TSADDREG_LPOS, MAC_TCR_TSADDREG_HPOS) == 0)
			break;
		vy_count++;
		mdelay(1);
	}

	MAC_TAR_REG_WR(data);
	/* issue command to update the added value */
	MAC_TCR_TSADDREG_UDFWR(0x1);
	/* wait for present added update to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retry_count)
			return -Y_FAILURE;

		MAC_TCR_REG_RD(MAC_TCR);
		if (GET_VALUE(MAC_TCR, MAC_TCR_TSADDREG_LPOS, MAC_TCR_TSADDREG_HPOS) == 0)
			break;

		vy_count++;
		mdelay(1);
	}

	return Y_SUCCESS;
}

static int init_systime(unsigned int sec, unsigned int nsec)
{
	unsigned long retry_count = 100000;
	unsigned long vy_count;
	unsigned long MAC_TCR;

	/* wait for previous(if any) time initialize to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retry_count)
			return -Y_FAILURE;

		MAC_TCR_REG_RD(MAC_TCR);

		if (GET_VALUE(MAC_TCR, MAC_TCR_TSINIT_LPOS, MAC_TCR_TSINIT_HPOS) == 0)
			break;

		vy_count++;
		mdelay(1);
	}
	MAC_STSUR_REG_WR(sec);
	MAC_STNSUR_REG_WR(nsec);
	/* issue command to initialize system time with the value */
	/* specified in MAC_STSUR and MAC_STNSUR. */
	MAC_TCR_TSINIT_UDFWR(0x1);
	/* wait for present time initialize to complete. */

	/*Poll*/
	vy_count = 0;
	while (1) {
		if (vy_count > retry_count)
			return -Y_FAILURE;

		MAC_TCR_REG_RD(MAC_TCR);
		if (GET_VALUE(MAC_TCR, MAC_TCR_TSINIT_LPOS, MAC_TCR_TSINIT_HPOS) == 0)
			break;

		vy_count++;
		mdelay(1);
	}

	return Y_SUCCESS;
}

static int config_hw_time_stamping(unsigned int config_val)
{
	MAC_TCR_REG_WR(config_val);

	return Y_SUCCESS;
}

static unsigned long long get_rx_tstamp(struct s_RX_CONTEXT_DESC *rxdesc)
{
	unsigned long long ns;
	unsigned long rdes1;

	RX_CONTEXT_DESC_RDES0_ML_RD(rxdesc->RDES0, ns);
	RX_CONTEXT_DESC_RDES1_ML_RD(rxdesc->RDES1, rdes1);
	ns = ns + (rdes1 * 1000000000ull);

	return ns;
}

static unsigned int get_rx_tstamp_status(struct s_RX_CONTEXT_DESC *rxdesc)
{
	unsigned int OWN;
	unsigned int CTXT;
	unsigned int RDES0;
	unsigned int RDES1;

	/* check for own bit and CTXT bit */
	RX_CONTEXT_DESC_RDES3_OWN_MLF_RD(rxdesc->RDES3, OWN);
	RX_CONTEXT_DESC_RDES3_CTXT_MLF_RD(rxdesc->RDES3, CTXT);
	if ((OWN == 0) && (CTXT == 0x1)) {
		RX_CONTEXT_DESC_RDES0_ML_RD(rxdesc->RDES0, RDES0);
		RX_CONTEXT_DESC_RDES1_ML_RD(rxdesc->RDES1, RDES1);
		if ((RDES0 == 0xffffffff) && (RDES1 == 0xffffffff))
			return 2; /* time stamp is corrupted */
		else
			return 1; /* time stamp is valid */
	} else {
		return 0; /* no CONTEX desc to hold time stamp value */
	}
}

static unsigned int rx_tstamp_available(struct s_RX_NORMAL_DESC *rxdesc)
{
	unsigned int RS1V;
	unsigned int TSA;

	RX_NORMAL_DESC_RDES3_RS1V_MLF_RD(rxdesc->RDES3, RS1V);
	if (RS1V == 1) {
		RX_NORMAL_DESC_RDES1_TSA_MLF_RD(rxdesc->RDES1, TSA);
		return TSA;
	} else {
		return 0;
	}
}

static unsigned long long get_tx_tstamp_via_reg(void)
{
	unsigned long long ns;
	unsigned long mac_ttn;

	MAC_TTSN_TXTSSTSLO_UDFRD(ns);
	MAC_TTN_TXTSSTSHI_UDFRD(mac_ttn);
	ns = ns + (mac_ttn * 1000000000ull);

	return ns;
}

static unsigned int get_tx_tstamp_status_via_reg(void)
{
	unsigned long MAC_TCR;
	unsigned long MAC_TTSN;

	/* device is configured to overwrite the timesatmp of */
	/* eariler packet if driver has not yet read it. */
	MAC_TCR_REG_RD(MAC_TCR);
	if (GET_VALUE(MAC_TCR, MAC_TCR_TXTSSTSM_LPOS, MAC_TCR_TXTSSTSM_HPOS) == 0) {
		/* timesatmp of the current pkt is ignored or not captured */
		MAC_TTSN_REG_RD(MAC_TTSN);
		if (GET_VALUE(MAC_TTSN, MAC_TTSN_TXTSSTSMIS_LPOS, MAC_TTSN_TXTSSTSMIS_HPOS) == 1)
			return 0;
		else
			return 1;
	}

	return 0;
}

static unsigned long long get_tx_tstamp(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned long long ns;
	unsigned long tdes1;

	TX_NORMAL_DESC_TDES0_ML_RD(txdesc->TDES0, ns);
	TX_NORMAL_DESC_TDES1_ML_RD(txdesc->TDES1, tdes1);
	ns = ns + (tdes1 * 1000000000ull);

	return ns;
}

static unsigned int get_tx_tstamp_status(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned int TDES3;

	TX_NORMAL_DESC_TDES3_ML_RD(txdesc->TDES3, TDES3);

	return (TDES3 & 0x20000);
}

static int tx_aborted_error(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned int TDES3;

	/* check for TDES3.LC and TDES3.EC */
	TX_NORMAL_DESC_TDES3_ML_RD(txdesc->TDES3, TDES3);
	if (((TDES3 & 0x200) == 0x200) || ((TDES3 & 0x100) == 0x100))
		return 1;
	else
		return 0;
}

static int tx_carrier_lost_error(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned int TDES3;

	/* check TDES3.LoC and TDES3.NC bits */
	TX_NORMAL_DESC_TDES3_ML_RD(txdesc->TDES3, TDES3);
	if (((TDES3 & 0X800) == 0X800) || ((TDES3 & 0x400) == 0x400))
		return 1;
	else
		return 0;
}

static int tx_fifo_underrun(struct s_TX_NORMAL_DESC *txdesc)
{
	unsigned int TDES3;

	/* check TDES3.UF bit */
	TX_NORMAL_DESC_TDES3_ML_RD(txdesc->TDES3, TDES3);
	if ((TDES3 & 0x4) == 0x4)
		return 1;
	else
		return 0;
}

static int stop_dma_rx(unsigned int q_inx)
{
	unsigned long retry_count = 10;
	unsigned long vy_count;
	unsigned long DMA_DSR0;
	unsigned long DMA_DSR1;
	unsigned long DMA_DSR2;

	/* issue Rx dma stop command */
	DMA_RCR_ST_UDFWR(q_inx, 0);

	/* wait for Rx DMA to stop, ie wait till Rx DMA
	 * goes in either Running or Suspend state.
	 */
	if (q_inx == 0) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 0 stop failed, DSR0 = %#lx\n", DMA_DSR0);
				return -Y_FAILURE;
			}

			DMA_DSR0_REG_RD(DMA_DSR0);
			if ((GET_VALUE(DMA_DSR0, DMA_DSR0_RPS0_LPOS, DMA_DSR0_RPS0_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_RPS0_LPOS, DMA_DSR0_RPS0_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_RPS0_LPOS, DMA_DSR0_RPS0_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 1) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 1 stop failed, DSR0 = %#lx\n", DMA_DSR0);
				return -Y_FAILURE;
			}

			DMA_DSR0_REG_RD(DMA_DSR0);
			if ((GET_VALUE(DMA_DSR0, DMA_DSR0_RPS1_LPOS, DMA_DSR0_RPS1_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_RPS1_LPOS, DMA_DSR0_RPS1_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_RPS1_LPOS, DMA_DSR0_RPS1_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 2) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 2 stop failed, DSR0 = %#lx\n", DMA_DSR0);
				return -Y_FAILURE;
			}

			DMA_DSR0_REG_RD(DMA_DSR0);
			if ((GET_VALUE(DMA_DSR0, DMA_DSR0_RPS2_LPOS, DMA_DSR0_RPS2_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_RPS2_LPOS, DMA_DSR0_RPS2_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_RPS2_LPOS, DMA_DSR0_RPS2_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 3) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 3 stop failed, DSR0 = %#lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_RPS3_LPOS, DMA_DSR1_RPS3_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS3_LPOS, DMA_DSR1_RPS3_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS3_LPOS, DMA_DSR1_RPS3_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 4) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 4 stop failed, DSR0 = %#lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_RPS4_LPOS, DMA_DSR1_RPS4_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS4_LPOS, DMA_DSR1_RPS4_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS4_LPOS, DMA_DSR1_RPS4_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 5) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 5 stop failed, DSR0 = %#lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_RPS5_LPOS, DMA_DSR1_RPS5_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS5_LPOS, DMA_DSR1_RPS5_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS5_LPOS, DMA_DSR1_RPS5_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 6) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 6 stop failed, DSR0 = %#lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_RPS6_LPOS, DMA_DSR1_RPS6_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS6_LPOS, DMA_DSR1_RPS6_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_RPS6_LPOS, DMA_DSR1_RPS6_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 7) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Rx Channel 7 stop failed, DSR0 = %#lx\n", DMA_DSR2);
				return -Y_FAILURE;
			}

			DMA_DSR2_REG_RD(DMA_DSR2);
			if ((GET_VALUE(DMA_DSR2, DMA_DSR2_RPS7_LPOS, DMA_DSR2_RPS7_HPOS) == 0x3) ||
			    (GET_VALUE(DMA_DSR2, DMA_DSR2_RPS7_LPOS, DMA_DSR2_RPS7_HPOS) == 0x4) ||
			    (GET_VALUE(DMA_DSR2, DMA_DSR2_RPS7_LPOS, DMA_DSR2_RPS7_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	}

	return Y_SUCCESS;
}

static int start_dma_rx(unsigned int q_inx)
{
	DMA_RCR_ST_UDFWR(q_inx, 0x1);

	return Y_SUCCESS;
}

static int stop_dma_tx(unsigned int q_inx)
{
	unsigned long retry_count = 10;
	unsigned long vy_count;
	unsigned long DMA_DSR0;
	unsigned long DMA_DSR1;
	unsigned long DMA_DSR2;

	/* issue Tx dma stop command */
	DMA_TCR_ST_UDFWR(q_inx, 0);

	/* wait for Tx DMA to stop, ie wait till Tx DMA
	 * goes in Suspend state or stopped state.
	 */
	if (q_inx == 0) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 0 stop failed, DSR0 = %lx\n", DMA_DSR0);
				return -Y_FAILURE;
			}

			DMA_DSR0_REG_RD(DMA_DSR0);
			if ((GET_VALUE(DMA_DSR0, DMA_DSR0_TPS0_LPOS, DMA_DSR0_TPS0_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_TPS0_LPOS, DMA_DSR0_TPS0_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 1) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 1 stop failed, DSR0 = %lx\n", DMA_DSR0);
				return -Y_FAILURE;
			}

			DMA_DSR0_REG_RD(DMA_DSR0);
			if ((GET_VALUE(DMA_DSR0, DMA_DSR0_TPS1_LPOS, DMA_DSR0_TPS1_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_TPS1_LPOS, DMA_DSR0_TPS1_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 2) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 2 stop failed, DSR0 = %lx\n", DMA_DSR0);
				return -Y_FAILURE;
			}

			DMA_DSR0_REG_RD(DMA_DSR0);
			if ((GET_VALUE(DMA_DSR0, DMA_DSR0_TPS2_LPOS, DMA_DSR0_TPS2_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR0, DMA_DSR0_TPS2_LPOS, DMA_DSR0_TPS2_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 3) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 3 stop failed, DSR0 = %lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_TPS3_LPOS, DMA_DSR1_TPS3_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_TPS3_LPOS, DMA_DSR1_TPS3_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 4) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 4 stop failed, DSR0 = %lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_TPS4_LPOS, DMA_DSR1_TPS4_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_TPS4_LPOS, DMA_DSR1_TPS4_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 5) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 5 stop failed, DSR0 = %lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_TPS5_LPOS, DMA_DSR1_TPS5_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_TPS5_LPOS, DMA_DSR1_TPS5_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 6) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 6 stop failed, DSR0 = %lx\n", DMA_DSR1);
				return -Y_FAILURE;
			}

			DMA_DSR1_REG_RD(DMA_DSR1);
			if ((GET_VALUE(DMA_DSR1, DMA_DSR1_TPS6_LPOS, DMA_DSR1_TPS6_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR1, DMA_DSR1_TPS6_LPOS, DMA_DSR1_TPS6_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	} else if (q_inx == 7) {
		/*Poll*/
		vy_count = 0;
		while (1) {
			if (vy_count > retry_count) {
				pr_err("ERROR: Channel 7 stop failed, DSR0 = %lx\n", DMA_DSR2);
				return -Y_FAILURE;
			}

			DMA_DSR2_REG_RD(DMA_DSR2);
			if ((GET_VALUE(DMA_DSR2, DMA_DSR2_TPS7_LPOS, DMA_DSR2_TPS7_HPOS) == 0x6) ||
			    (GET_VALUE(DMA_DSR2, DMA_DSR2_TPS7_LPOS, DMA_DSR2_TPS7_HPOS) == 0x0))
				break;
			vy_count++;
			mdelay(1);
		}
	}

	return Y_SUCCESS;
}

/* \param[in] q_inx
 * \return Success or Failure
 * \retval  0 Success
 * \retval -1 Failure
 */

static int start_dma_tx(unsigned int q_inx)
{
	DMA_TCR_ST_UDFWR(q_inx, 0x1);

	return Y_SUCCESS;
}

static int stop_mac_tx_rx(void)
{
	unsigned long MAC_MCR;

	MAC_MCR_REG_RD(MAC_MCR);
	MAC_MCR = MAC_MCR & (unsigned long)(0xffffff7c);
	MAC_MCR = MAC_MCR | ((0) << 1) | ((0) << 0);
	MAC_MCR_REG_WR(MAC_MCR);

	return Y_SUCCESS;
}

static int start_mac_tx_rx(void)
{
	unsigned long MAC_MCR;

	MAC_MCR_REG_RD(MAC_MCR);
	MAC_MCR = MAC_MCR & (unsigned long)(0xffffff7c);
	MAC_MCR = MAC_MCR | ((0x1) << 1) | ((0x1) << 0);
	MAC_MCR_REG_WR(MAC_MCR);

	return Y_SUCCESS;
}

static int config_mac_pkt_filter_reg(unsigned char pr_mode,
				     unsigned char huc_mode,
				     unsigned char hmc_mode,
				     unsigned char pm_mode,
				     unsigned char hpf_mode)
{
	unsigned long MAC_MPFR;

	/* configure device in differnet modes */
	/* promiscuous, hash unicast, hash multicast, */
	/* all multicast and perfect/hash filtering mode. */
	MAC_MPFR_REG_RD(MAC_MPFR);
	MAC_MPFR = MAC_MPFR & (unsigned long)(0X803103e8);
	MAC_MPFR = MAC_MPFR | ((pr_mode) << 0) | ((huc_mode) << 1) | ((hmc_mode) << 2) |
		   ((pm_mode) << 4) | ((hpf_mode) << 10);
	MAC_MPFR_REG_WR(MAC_MPFR);

	return Y_SUCCESS;
}

static int config_rx_watchdog_timer(unsigned int q_inx, u32 riwt)
{
	DMA_RIWTR_RWT_UDFWR(q_inx, riwt);

	return Y_SUCCESS;
}

static int update_mac_addr32_127_low_high_reg(int idx, unsigned char addr[])
{
	MAC_MA32_127LR_REG_WR(idx, (addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24)));
	MAC_MA32_127HR_ADDRHI_UDFWR(idx, (addr[4] | (addr[5] << 8)));
	MAC_MA32_127HR_AE_UDFWR(idx, 0x1);

	return Y_SUCCESS;
}

static int update_mac_addr1_31_low_high_reg(int idx, unsigned char addr[])
{
	MAC_MA1_31LR_REG_WR(idx, (addr[0] | (addr[1] << 8) | (addr[2] << 16) | (addr[3] << 24)));
	MAC_MA1_31HR_ADDRHI_UDFWR(idx, (addr[4] | (addr[5] << 8)));
	MAC_MA1_31HR_AE_UDFWR(idx, 0x1);

	return Y_SUCCESS;
}

static int update_hash_table_reg(int idx, unsigned int data)
{
	MAC_HTR_REG_WR(idx, data);

	return Y_SUCCESS;
}

static void pre_transmit(struct prv_data *pdata,	unsigned int q_inx)
{
	struct tx_wrapper_descriptor *tx_desc_data =
	    GET_TX_WRAPPER_DESC(q_inx);
	struct tx_buffer *buffer =
	    GET_TX_BUF_PTR(q_inx, tx_desc_data->cur_tx);
	struct s_TX_NORMAL_DESC *TX_NORMAL_DESC =
	    GET_TX_DESC_PTR(q_inx, tx_desc_data->cur_tx);
	int i;
	int start_index = tx_desc_data->cur_tx;
	int last_index;
	struct s_tx_pkt_features *tx_pkt_features = GET_TX_PKT_FEATURES_PTR;
	unsigned int ptp_enable = 0;
	int total_len = 0;

	/* update the first buffer pointer and length */
	TX_NORMAL_DESC_TDES0_ML_WR(TX_NORMAL_DESC->TDES0, buffer->dma);
	TX_NORMAL_DESC_TDES2_HL_B1L_MLF_WR(TX_NORMAL_DESC->TDES2, buffer->len);
	if (buffer->dma2 != 0) {
		/* update the second buffer pointer and length */
		TX_NORMAL_DESC_TDES1_ML_WR(TX_NORMAL_DESC->TDES1, buffer->dma2);
		TX_NORMAL_DESC_TDES2_B2L_MLF_WR(TX_NORMAL_DESC->TDES2, buffer->len2);
	}

	/* update total length of packet */
	GET_TX_TOT_LEN(GET_TX_BUF_PTR(q_inx, 0), tx_desc_data->cur_tx,
		       GET_CURRENT_XFER_DESC_CNT(q_inx), total_len);
	TX_NORMAL_DESC_TDES3_FL_MLF_WR(TX_NORMAL_DESC->TDES3, total_len);

	/* Mark it as First Descriptor */
	TX_NORMAL_DESC_TDES3_FD_MLF_WR(TX_NORMAL_DESC->TDES3, 0x1);
	/* Enable CRC and Pad Insertion (NOTE: set this only
	 * for FIRST descriptor)
	 */
	TX_NORMAL_DESC_TDES3_CPC_MLF_WR(TX_NORMAL_DESC->TDES3, 0);
	/* Mark it as NORMAL descriptor */
	TX_NORMAL_DESC_TDES3_CTXT_MLF_WR(TX_NORMAL_DESC->TDES3, 0);

	/* enable timestamping */
	TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_RD(tx_pkt_features->pkt_attributes, ptp_enable);
	if (ptp_enable)
		TX_NORMAL_DESC_TDES2_TTSE_MLF_WR(TX_NORMAL_DESC->TDES2, 0x1);

	INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
	TX_NORMAL_DESC = GET_TX_DESC_PTR(q_inx, tx_desc_data->cur_tx);
	buffer = GET_TX_BUF_PTR(q_inx, tx_desc_data->cur_tx);

	for (i = 1; i < GET_CURRENT_XFER_DESC_CNT(q_inx); i++) {
		/* update the first buffer pointer and length */
		TX_NORMAL_DESC_TDES0_ML_WR(TX_NORMAL_DESC->TDES0, buffer->dma);
		TX_NORMAL_DESC_TDES2_HL_B1L_MLF_WR(TX_NORMAL_DESC->TDES2, buffer->len);
		if (buffer->dma2 != 0) {
			/* update the second buffer pointer and length */
			TX_NORMAL_DESC_TDES1_ML_WR(TX_NORMAL_DESC->TDES1, buffer->dma2);
			TX_NORMAL_DESC_TDES2_B2L_MLF_WR(TX_NORMAL_DESC->TDES2, buffer->len2);
		}

		/* set own bit */
		TX_NORMAL_DESC_TDES3_OWN_MLF_WR(TX_NORMAL_DESC->TDES3, 0x1);
		/* Mark it as NORMAL descriptor */
		TX_NORMAL_DESC_TDES3_CTXT_MLF_WR(TX_NORMAL_DESC->TDES3, 0);

		INCR_TX_DESC_INDEX(tx_desc_data->cur_tx, 1);
		TX_NORMAL_DESC = GET_TX_DESC_PTR(q_inx, tx_desc_data->cur_tx);
		buffer = GET_TX_BUF_PTR(q_inx, tx_desc_data->cur_tx);
	}
	/* Mark it as LAST descriptor */
	last_index =
		GET_CURRENT_XFER_LAST_DESC_INDEX(q_inx, start_index, 0);
	TX_NORMAL_DESC = GET_TX_DESC_PTR(q_inx, last_index);
	TX_NORMAL_DESC_TDES3_LD_MLF_WR(TX_NORMAL_DESC->TDES3, 0x1);
	/* set Interrupt on Completion for last descriptor */
	TX_NORMAL_DESC_TDES2_IC_MLF_WR(TX_NORMAL_DESC->TDES2, 0x1);

	/* set OWN bit of FIRST descriptor at end to avoid race condition */
	TX_NORMAL_DESC = GET_TX_DESC_PTR(q_inx, start_index);
	TX_NORMAL_DESC_TDES3_OWN_MLF_WR(TX_NORMAL_DESC->TDES3, 0x1);

	/* issue a poll command to Tx DMA by writing address
	 * of next immediate free descriptor
	 */
	last_index = GET_CURRENT_XFER_LAST_DESC_INDEX(q_inx, start_index, 1);
	DMA_TDTP_TPDR_REG_WR(q_inx, GET_TX_DESC_DMA_ADDR(q_inx, last_index));
}

void init_function_ptrs_dev(struct hw_if_struct *hw_if)
{
	hw_if->tx_complete = tx_complete;
	hw_if->init = yinit;
	hw_if->exit = yexit;

	/* Descriptor related Sequences have to be initialized here */
	hw_if->tx_desc_init = tx_descriptor_init;
	hw_if->rx_desc_init = rx_descriptor_init;
	hw_if->rx_desc_reset = rx_descriptor_reset;
	hw_if->tx_desc_reset = tx_descriptor_reset;
	hw_if->get_tx_desc_ls = get_tx_descriptor_last;
	hw_if->get_tx_desc_ctxt = get_tx_descriptor_ctxt;
	hw_if->update_rx_tail_ptr = update_rx_tail_ptr;

	/* for FLOW ctrl */
	hw_if->enable_rx_flow_ctrl = enable_rx_flow_ctrl;
	hw_if->disable_rx_flow_ctrl = disable_rx_flow_ctrl;
	hw_if->enable_tx_flow_ctrl = enable_tx_flow_ctrl;
	hw_if->disable_tx_flow_ctrl = disable_tx_flow_ctrl;
	hw_if->disable_rx_interrupt = disable_rx_interrupt;
	hw_if->enable_rx_interrupt = enable_rx_interrupt;

	hw_if->write_phy_regs = write_phy_regs;
	hw_if->read_phy_regs = read_phy_regs;
	hw_if->set_full_duplex = set_full_duplex;
	hw_if->set_half_duplex = set_half_duplex;
	hw_if->set_mii_speed_10 = set_mii_speed_10;
	hw_if->set_mii_speed_100 = set_mii_speed_100;
	hw_if->set_gmii_speed = set_gmii_speed;

	/* for hw time stamping */
	hw_if->config_hw_time_stamping = config_hw_time_stamping;
	hw_if->config_sub_second_increment = config_sub_second_increment;
	hw_if->init_systime = init_systime;
	hw_if->config_addend = config_addend;
	hw_if->adjust_systime = adjust_systime;
	hw_if->get_systime = get_systime;
	hw_if->get_tx_tstamp_status = get_tx_tstamp_status;
	hw_if->get_tx_tstamp = get_tx_tstamp;
	hw_if->get_tx_tstamp_status_via_reg = get_tx_tstamp_status_via_reg;
	hw_if->get_tx_tstamp_via_reg = get_tx_tstamp_via_reg;
	hw_if->rx_tstamp_available = rx_tstamp_available;
	hw_if->get_rx_tstamp_status = get_rx_tstamp_status;
	hw_if->get_rx_tstamp = get_rx_tstamp;
	hw_if->drop_tx_status_enabled = drop_tx_status_enabled;

	hw_if->tx_aborted_error = tx_aborted_error;
	hw_if->tx_carrier_lost_error = tx_carrier_lost_error;
	hw_if->tx_fifo_underrun = tx_fifo_underrun;
	hw_if->start_dma_rx = start_dma_rx;
	hw_if->stop_dma_rx = stop_dma_rx;
	hw_if->start_dma_tx = start_dma_tx;
	hw_if->stop_dma_tx = stop_dma_tx;
	hw_if->start_mac_tx_rx = start_mac_tx_rx;
	hw_if->stop_mac_tx_rx = stop_mac_tx_rx;
	hw_if->config_mac_pkt_filter_reg = config_mac_pkt_filter_reg;
	hw_if->pre_xmit = pre_transmit;

	/* for RX watchdog timer */
	hw_if->config_rx_watchdog = config_rx_watchdog_timer;
	hw_if->update_mac_addr32_127_low_high_reg = update_mac_addr32_127_low_high_reg;
	hw_if->update_mac_addr1_31_low_high_reg = update_mac_addr1_31_low_high_reg;
	hw_if->update_hash_table_reg = update_hash_table_reg;
}
