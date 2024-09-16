/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/hif/sdio/include/hif.h#1
*/

/*
 * ! \file   "hif_pdma.h"
 *  \brief  MARCO, definition, structure for PDMA.
 *
 *   MARCO, definition, structure for PDMA.
 */

#ifndef _HIF_CQDMA_H
#define _HIF_CQDMA_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef enum _MTK_WCN_HIF_CQDMA_BURST_LEN {
	HIF_CQDMA_BURST_1_8 = 0,
	HIF_CQDMA_BURST_2_8,
	HIF_CQDMA_BURST_3_8,
	HIF_CQDMA_BURST_4_8,
	HIF_CQDMA_BURST_5_8,
	HIF_CQDMA_BURST_6_8,
	HIF_CQDMA_BURST_7_8,
	HIF_CQDMA_BURST_8_8
} MTK_WCN_HIF_CQDMA_BURST_LEN;

typedef enum _MTK_WCN_HIF_CQDMA_RSIZE {
	HIF_CQDMA_RSIZE_0 = 0,    /* transaction size is 1 byte */
	HIF_CQDMA_RSIZE_1,        /* transaction size is 2 byte */
	HIF_CQDMA_RSIZE_2,        /* transaction size is 4 byte */
	HIF_CQDMA_RSIZE_3         /* transaction size is 1 byte */
} MTK_WCN_HIF_CQDMA_RSIZE;

typedef enum _MTK_WCN_HIF_CQDMA_WSIZE {
	HIF_CQDMA_WSIZE_0 = 0,    /* transaction size is 1 byte */
	HIF_CQDMA_WSIZE_1,        /* transaction size is 2 byte */
	HIF_CQDMA_WSIZE_2,        /* transaction size is 4 byte */
	HIF_CQDMA_WSIZE_3         /* transaction size is 1 byte */
} MTK_WCN_HIF_CQDMA_WSIZE;

typedef enum _MTK_WCN_HIF_CQDMA_CONNECT {
	HIF_CQDMA_CONNECT_NO = 0, /* no connect */
	HIF_CQDMA_CONNECT_SET1,   /* connect set1 (req/ack) */
	HIF_CQDMA_CONNECT_SET2,   /* connect set2 (req/ack) */
	HIF_CQDMA_CONNECT_SET3    /* connect set3 (req/ack) */
} MTK_WCN_HIF_CQDMA_CONNECT;


#define CQ_DMA_HIF_BASE             0x11200180

#define CQ_DMA_HIF_INT_FLAG         (0x0000) /* CQ_DMA_G_DMA_3_INT_FLAG     */
#define CQ_DMA_HIF_INT_EN           (0x0004) /* CQ_DMA_G_DMA_3_INT_EN       */
#define CQ_DMA_HIF_EN               (0x0008) /* CQ_DMA_G_DMA_3_EN           */
#define CQ_DMA_HIF_RST              (0x000C) /* CQ_DMA_G_DMA_3_RST          */
#define CQ_DMA_HIF_STOP             (0x0010) /* CQ_DMA_G_DMA_3_STOP         */
#define CQ_DMA_HIF_FLUSH            (0x0014) /* CQ_DMA_G_DMA_3_FLUSH        */
#define CQ_DMA_HIF_CON              (0x0018) /* CQ_DMA_G_DMA_3_CON          */
#define CQ_DMA_HIF_SRC_ADDR         (0x001C) /* CQ_DMA_G_DMA_3_SRC_ADDR     */
#define CQ_DMA_HIF_DST_ADDR         (0x0020) /* CQ_DMA_G_DMA_3_DST_ADDR     */
#define CQ_DMA_HIF_LEN1             (0x0024) /* CQ_DMA_G_DMA_3_LEN1         */
#define CQ_DMA_HIF_LEN2             (0x0028) /* CQ_DMA_G_DMA_3_LEN2         */
#define CQ_DMA_HIF_JUMP_ADDR        (0x002C) /* CQ_DMA_G_DMA_3_JUMP_ADDR    */
#define CQ_DMA_HIF_INT_BUF_SIZE     (0x0030) /* CQ_DMA_G_DMA_3_INT_BUF_SIZE */
#define CQ_DMA_HIF_CONNECT          (0x0034) /* CQ_DMA_G_DMA_3_CONNECT      */
#define CQ_DMA_HIF_DEBUG_STATUS     (0x0050) /* CQ_DMA_G_DMA_3_DEBUG_STATUS */

#define CQ_DMA_HIF_SRC_ADDR2        (0x0060) /* CQ_DMA_G_DMA_3_SRC_ADDR2    */
#define CQ_DMA_HIF_DST_ADDR2        (0x0064) /* CQ_DMA_G_DMA_3_DST_ADDR2    */
#define CQ_DMA_HIF_JUMP_ADDR2       (0x0068) /* CQ_DMA_G_DMA_3_JUMP_ADDR2   */

#define CQ_DMA_HIF_LENGTH           0x006C

/* CQ_DMA_HIF_INT_FLAG */
#define CDH_CR_INT_FLAG             BIT(0)

/* CQ_DMA_HIF_INT_EN */
#define CDH_CR_INTEN_FLAG           BIT(0)

/* CQ_DMA_HIF_EN */
#define CDH_CR_EN                   BIT(0)

/* CQ_DMA_HIF_RST */
#define CDH_CR_HARD_RST             BIT(1)
#define CDH_CR_WARM_RST             BIT(0)

/* CQ_DMA_HIF_STOP */
#define CDH_CR_PAUSE                BIT(1)
#define CDH_CR_STOP                 BIT(0)

/* CQ_DMA_HIF_FLUSH */
#define CDH_CR_FLUSH                BIT(0)

/* CQ_DMA_HIF_CON */
#define CDH_CR_FIX8                 BIT(31)
#define CDH_CR_FIX8_OFFSET          31
#define CDH_CR_RSIZE                BITS(28, 29)
#define CDH_CR_RSIZE_OFFSET         28
#define CDH_CR_WSIZE                BITS(24, 25)
#define CDH_CR_WSIZE_OFFSET         24
#define CDH_CR_WRAP_SEL             BIT(20)
#define CDH_CR_WRAP_SEL_OFFSET      20
#define CDH_CR_BURST_LEN            BITS(16, 18)
#define CDH_CR_BURST_LEN_OFFSET     16
#define CDH_CR_WRAP_EN              BIT(15)
#define CDH_CR_WRAP_EN_OFFSET       15
#define CDH_CR_SLOW_CNT             BITS(5, 14)
#define CDH_CR_RADDR_FIX_EN         BIT(4)
#define CDH_CR_RADDR_FIX_EN_OFFSET  4
#define CDH_CR_WADDR_FIX_EN         BIT(3)
#define CDH_CR_WADDR_FIX_EN_OFFSET  3
#define CDH_CR_SLOW_EN              BIT(2)
#define CDH_CR_FIX_EN               BIT(1)
#define CDH_CR_FIX_EN_OFFSET        1
#define CDH_CR_RESIDUE              BIT(0)

/* CQ_DMA_HIF_CONNECT */
#define CDH_CR_DIR                  BIT(2)
#define CDH_CR_DIR_OFFSET           2
#define CDH_CR_CONNECT              BITS(0, 1)

/* CQ_DMA_HIF_LEN */
#define CDH_CR_LEN                  BITS(0, 19)

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif /* _HIF_CQDMA_H */
