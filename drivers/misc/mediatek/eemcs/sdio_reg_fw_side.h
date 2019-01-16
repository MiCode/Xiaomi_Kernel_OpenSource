#ifndef __SDIO_REG_FWSIDE_H__
#define __SDIO_REG_FWSIDE_H__



/* Because this SDIO IP is a general IP from DT,
    The Tx & Rx definition is different between DT & WAT.
    So we keep the original TxRx definition in this file,
    and user can find the correct register with same in CSR document.

    For match WAT's TxRx style,
    we do not directly use the register definition here. 
    Instead, we map these registers to WAT style in hif_sdioq_reg.h & hif_sdiodev_reg.h

*/

#define MD_IRQID_SDIO (16)

#define ORG_SDIO_BASE	    (0xBFC00000)
#define ORG_SDIO_IRQID      MD_IRQID_SDIO
#define BASE_ADDR_CISCC     (0xBF230000)

/* CISRAM & Card Capability Bus R/W Interface */
#define ORG_SDIO_CIS0_BASE			(volatile kal_uint32 *)(BASE_ADDR_CISCC+0xC000) 
#define ORG_SDIO_CIS1_BASE			(volatile kal_uint32 *)(BASE_ADDR_CISCC+0xC400)
#define ORG_SDIO_CAPABILITY_BASE    (volatile kal_uint32 *)(BASE_ADDR_CISCC+0xCC00)
#define ORG_SDIO_CIS_READY	        (volatile kal_uint32 *)(BASE_ADDR_CISCC+0xCC40)
#define ORG_SDIO_CAPABILITY_READY	(volatile kal_uint32 *)(BASE_ADDR_CISCC+0xCC44)

 
/* SDIO QMU  Register */

#define ORG_SDIO_HGFCR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0000)
#define ORG_SDIO_HGFISR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0004)
#define ORG_SDIO_HGFIER				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0008)

//#define ORG_SDIO_HSDBDLSR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0010)
#define ORG_SDIO_HSDLSR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0014)
#define ORG_SDIO_HDBGCR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0020)

#define ORG_SDIO_HWFISR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0100)
#define ORG_SDIO_HWFIER				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0104)
#define ORG_SDIO_HWFTE0SR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0110)
#define ORG_SDIO_HWFTE0ER			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0120)
#define ORG_SDIO_HWFRE0SR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0130)
#define ORG_SDIO_HWFRE1SR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0134)
#define ORG_SDIO_HWFRE0ER			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0140)
#define ORG_SDIO_HWFRE1ER			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0144)

#define ORG_SDIO_HWFICR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0150)
#define ORG_SDIO_HWFCR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0154)
#define ORG_SDIO_HWTDCR				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0158)
#define ORG_SDIO_HWTPCCR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x015C)

//#define ORG_SDIO_HWFTQ0SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0160)
#define ORG_SDIO_HWFTQ1SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0164)
#define ORG_SDIO_HWFTQ2SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0168)
#define ORG_SDIO_HWFTQ3SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x016C)
#define ORG_SDIO_HWFTQ4SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0170)
#define ORG_SDIO_HWFTQ5SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0174)
#define ORG_SDIO_HWFTQ6SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0178)
#define ORG_SDIO_HWFTQ7SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x017C)


#define ORG_SDIO_HWFRQ0SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0180)
#define ORG_SDIO_HWFRQ1SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0184)
#define ORG_SDIO_HWFRQ2SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x0188)
#define ORG_SDIO_HWFRQ3SAR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x018C)

#define ORG_SDIO_H2DRM0R				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01A0)
#define ORG_SDIO_H2DRM1R				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01A4)
#define ORG_SDIO_D2HSM0R				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01A8)
#define ORG_SDIO_D2HSM1R				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01AC)
#define ORG_SDIO_D2HSM2R				(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01B0)

#define ORG_SDIO_HWRQ0CR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01C0)
#define ORG_SDIO_HWRQ1CR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01C4)
#define ORG_SDIO_HWRQ2CR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01C8)
#define ORG_SDIO_HWRQ3CR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01CC)

#define ORG_SDIO_HWRLFACR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01E0)
#define ORG_SDIO_HWDMACR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01E8)
#define ORG_SDIO_HWFIOCDR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01EC)
#define ORG_SDIO_HSDIOTOCR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01F0)
#define ORG_SDIO_HSDIOSPCR			(volatile kal_uint32 *)(ORG_SDIO_BASE+0x01F4)


// TxQ0 is removed, use  TxQ1 in HW as TxQ0 in SW
#define ORG_SDIO_TQSAR_n(q_num)			(volatile kal_uint32 *)(((kal_uint32)ORG_SDIO_HWFTQ1SAR) + (0x04 * q_num))
#define ORG_SDIO_RQSAR_n(q_num)			(volatile kal_uint32 *)(((kal_uint32)ORG_SDIO_HWFRQ0SAR) + (0x04 * q_num))
#define ORG_SDIO_RQCR_n(q_num)			(volatile kal_uint32 *)(((kal_uint32)ORG_SDIO_HWRQ0CR) + (0x04 * q_num))




/**
 *	@brief	Configuration value Definition
 */ 

/* Only use in ORG_SDIO_HGFCR */
#define ORG_SDIO_DB_HIF_SEL			0x00000007
#define ORG_SDIO_SPI_MODE			0x00000010
#define ORG_SDIO_EHPI_MODE			0x00000020
#define ORG_SDIO_HINT_AS_FW_OB		0x00000100
#define ORG_SDIO_CARD_IS_18V		0x00000200
#define ORG_SDIO_INT_TER_CYC_MASK	0x00010000
#define ORG_SDIO_HCLK_NO_GATED		0x00020000
#define ORG_SDIO_FORCE_SD_HS		0x00040000
#define ORG_SDIO_SDIO_HCLK_DIS		0x01000000
#define ORG_SDIO_SPI_HCLK_DIS		0x02000000
#define ORG_SDIO_EHPI_HCLK_DIS		0x04000000
#define ORG_SDIO_PB_HCLK_DIS		0x08000000

/* Only use in ORG_SDIO_HWFCR */
#define ORG_SDIO_W_FUNC_RDY			0x00000001
#define ORG_SDIO_TRX_DESC_CHKSUM_EN	0x00000002
#define ORG_SDIO_TRX_DESC_CHKSUM_12	0x00000004
#define ORG_SDIO_TX_CS_OFLD_EN		0x00000008
#define ORG_SDIO_RX_IPV6_CS_OFLD_EN	0x00000010
#define ORG_SDIO_RX_IPV4_CS_OFLD_EN	0x00000020
#define ORG_SDIO_RX_TCP_CS_OFLD_EN	0x00000040
#define ORG_SDIO_RX_UDP_CS_OFLD_EN	0x00000080
#define ORG_SDIO_TX_NO_HEADER       0x00000100
#define ORG_SDIO_RX_NO_TAIL         0x00000200


/* Only use in ORG_SDIO_HWDMACR */
#define ORG_SDIO_AHB_1KBNDRY_PRTCT	0x00000001
#define ORG_SDIO_DEST_BST_TYP		0x00000002
#define ORG_SDIO_DMA_BST_SIZE		0x00000004

/* Only use in ORG_SDIO_HWFIOCDR */
    // TxQ0 is removed, use  TxQ1 in HW as TxQ0 in SW
#define ORG_SDIO_TXQ1_IOC_DIS		0x00000002
#define ORG_SDIO_TXQ_IOC_DIS(n)		(ORG_SDIO_TXQ1_IOC_DIS<<(n))
#define ORG_SDIO_RXQ0_IOC_DIS		0x00000100
#define ORG_SDIO_RXQ_IOC_DIS(n)		(ORG_SDIO_RXQ0_IOC_DIS<<(n))


/* Only use in ORG_SDIO_HSDIOTOCR */
#define ORG_SDIO_TIMEOUT_NUM		0x0000FFFF
#define ORG_SDIO_RD_TIMEOUT_EN		0x00010000
#define ORG_SDIO_WR_TIMEOUT_EN		0x00020000

/* Only use in ORG_SDIO_HSDIOSPCR */
#define ORG_SDIO_CMD_SAMPLE         0x00000001
#define ORG_SDIO_DAT0_SAMPLE        0x00000002
#define ORG_SDIO_DAT1_SAMPLE        0x00000004
#define ORG_SDIO_DAT2_SAMPLE        0x00000008
#define ORG_SDIO_DAT3_SAMPLE        0x00000010


/**
 *	@brief	General Interrupt value Definition
 */ 

/* Only use in ORG_SDIO_HGFISR */
#define ORG_SDIO_DRV_CLR_DB_IOE		0x00000001
#define ORG_SDIO_DRV_SET_DB_IOE		0x00000004
#define ORG_SDIO_SET_RES			0x00000010
#define ORG_SDIO_SET_ABT			0x00000020
#define ORG_SDIO_DB_INT				0x00000040
#define ORG_SDIO_CRC_ERROR_INT		0x00000100
#define ORG_SDIO_CHG_TO_18V_REQ		0x00000200


/* Only use in ORG_SDIO_HWFISR */
#define ORG_SDIO_DRV_SET_FW_OWN		0x00000001
#define ORG_SDIO_DRV_CLR_FW_OWN		0x00000002
#define ORG_SDIO_D2HSM2R_RD			0x00000004
#define ORG_SDIO_RD_TIMEOUT			0x00000008
#define ORG_SDIO_WR_TIMEOUT			0x00000010
#define ORG_SDIO_TX_EVENT_0			0x00000100
#define ORG_SDIO_RX_EVENT_0			0x00001000
#define ORG_SDIO_RX_EVENT_1			0x00002000

#define ORG_SDIO_H2D_SW_INT0		0x00010000
#define ORG_SDIO_H2D_SW_INT(n)		(ORG_SDIO_H2D_SW_INT0<<(n))




/**
 *	@brief	FW Set Interrupt to Host value Definition
 */

/* Only use in ORG_SDIO_HWFICR */
#define ORG_SDIO_FW_OWN_BACK_SET	0x00000010
#define ORG_SDIO_FW_OWN_READ        0x00000010
#define ORG_SDIO_D2H_SW_INT0		0x00000100
#define ORG_SDIO_D2H_SW_INT(n)		(ORG_SDIO_D2H_SW_INT0<<(n))


/**
 *	@brief	Queue Related Interrupt value Definition
 */   
     // TxQ0 is removed, use  TxQ1 in HW as TxQ0 in SW
#define ORG_SDIO_TXQ1_RDY			0x00000002
#define ORG_SDIO_TXP1_OVERFLOW		0x00000200
#define ORG_SDIO_TXQ1_CHKSUM_ERR	0x00020000
#define ORG_SDIO_TXQ1_LEN_ERR       0x02000000
#define ORG_SDIO_TXQ_RDY(n)			(ORG_SDIO_TXQ1_RDY<<(n))
#define ORG_SDIO_TXP_OVERFLOW(n)	(ORG_SDIO_TXP1_OVERFLOW<<(n))
#define ORG_SDIO_TXQ_CHKSUM_ERR(n)	(ORG_SDIO_TXQ1_CHKSUM_ERR<<(n))
#define ORG_SDIO_TXQ_LEN_ERR(n)	    (ORG_SDIO_TXQ1_LEN_ERR<<(n))


#define ORG_SDIO_RXQ0_DONE			0x00000001
#define ORG_SDIO_RXP0_UNDERFLOW		0x00000100
#define ORG_SDIO_RXQ0_CHKSUM_ERR	0x00010000
#define ORG_SDIO_RXP0_OVERFLOW		0x01000000
#define ORG_SDIO_RXQ0_OWN_CLEAR		0x00000001
#define ORG_SDIO_RXQ0_LEN_ERR		0x00000100
#define ORG_SDIO_RXQ_DONE(n)		(ORG_SDIO_RXQ0_DONE<<(n))
#define ORG_SDIO_RXP_UNDERFLOW(n)	(ORG_SDIO_RXP0_UNDERFLOW<<(n))
#define ORG_SDIO_RXQ_CHKSUM_ERR(n)	(ORG_SDIO_RXQ0_CHKSUM_ERR<<(n))
#define ORG_SDIO_RXP_OVERFLOW(n)	(ORG_SDIO_RXP0_OVERFLOW<<(n))
#define ORG_SDIO_RXQ_OWN_CLEAR(n)	(ORG_SDIO_RXQ0_OWN_CLEAR<<(n))
#define ORG_SDIO_RXQ_LEN_ERR(n)	    (ORG_SDIO_RXQ0_LEN_ERR<<(n))



 /**
  *  @brief  Queue Control value Definition
  */							
      // TxQ0 is removed, use  TxQ1 in HW as TxQ0 in SW
#define ORG_SDIO_TXQ1_STOP			0x00000002
#define ORG_SDIO_TXQ1_START			0x00000200
#define ORG_SDIO_TXQ1_RESUME		0x00020000
#define ORG_SDIO_TXQ1_STATUS		0x02000000
#define ORG_SDIO_TXQ_STOP(n)		(ORG_SDIO_TXQ1_STOP<<(n))
#define ORG_SDIO_TXQ_START(n)		(ORG_SDIO_TXQ1_START<<(n))
#define ORG_SDIO_TXQ_RESUME(n)		(ORG_SDIO_TXQ1_RESUME<<(n))
#define ORG_SDIO_TXQ_STATUS(n)		(ORG_SDIO_TXQ1_STATUS<<(n))

/* Only use in ORG_SDIO_HWTPCCR */
#define ORG_SDIO_INC_TQ_CNT			0x000000FF
#define ORG_SDIO_TQ_INDEX			0x0000F000
#define ORG_SDIO_TQ_INDEX_OFFSET	(12)
#define ORG_SDIO_TQ_CNT_RESET		0x00010000

  
#define ORG_SDIO_RXQ_STOP			0x00010000
#define ORG_SDIO_RXQ_START			0x00020000
#define ORG_SDIO_RXQ_RESUME			0x00040000
#define ORG_SDIO_RXQ_STATUS			0x00080000
#define ORG_SDIO_RXQ_PKT_LEN		0x0000FFFF

/* Only use in ORG_SDIO_HWRLFACR */
#define ORG_SDIO_RX0_LEN_FIFO_AVAIL_CNT			0x0000001F
#define ORG_SDIO_RX_LEN_FIFO_AVAIL_CNT_OFFSET	(8)
#define ORG_SDIO_RX_LEN_FIFO_AVAIL_CNT(n)		(ORG_SDIO_RX0_LEN_FIFO_AVAIL_CNT<<(n*ORG_SDIO_RX_LEN_FIFO_AVAIL_CNT_OFFSET))


#endif

