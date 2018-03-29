/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *  mt_sco_pcm_btcvsd.h
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio btcvsd define
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/
#ifndef MT_SOC_PCM_BTCVSD_H
#define MT_SOC_PCM_BTCVSD_H

/* #include <mach/mt_typedefs.h> */
#include <linux/types.h>

#undef DEBUG_AUDDRV
#ifdef DEBUG_AUDDRV
#define LOGBT(format, args...) pr_warn(format, ##args)
#else
#define LOGBT(format, args...)
#endif


static DECLARE_WAIT_QUEUE_HEAD(BTCVSD_Write_Wait_Queue);
static DECLARE_WAIT_QUEUE_HEAD(BTCVSD_Read_Wait_Queue);
extern spinlock_t auddrv_btcvsd_tx_lock;
extern spinlock_t auddrv_btcvsd_rx_lock;

/*
 *    function implementation
 */

extern struct device *mDev_btcvsd_rx;
extern struct device *mDev_btcvsd_tx;

/*****************************************************************************
 *                                      C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                         E X T E R N A L       R E F E R E N C E S
 *****************************************************************************/


/*****************************************************************************
 *                                              D A T A   T Y P E S
 *****************************************************************************/

#define BT_SCO_PACKET_120 120
#define BT_SCO_PACKET_180 180

#define BT_CVSD_TX_NREADY    (1<<21)
#define BT_CVSD_RX_READY     (1<<22)
#define BT_CVSD_TX_UNDERFLOW (1<<23)
#define BT_CVSD_RX_OVERFLOW  (1<<24)
#define BT_CVSD_INTERRUPT    (1<<31)

#define BT_CVSD_CLEAR (BT_CVSD_TX_NREADY|BT_CVSD_RX_READY|BT_CVSD_TX_UNDERFLOW|BT_CVSD_RX_OVERFLOW|BT_CVSD_INTERRUPT)

/* TX */
#define SCO_TX_ENCODE_SIZE           (60) /* 60 byte (60*8 samples) */
#define SCO_TX_PACKER_BUF_NUM        (8) /* 8 */
#define SCO_TX_PACKET_MASK           (0x7) /* 0x7*/
#define SCO_TX_PCM64K_BUF_SIZE       (SCO_TX_ENCODE_SIZE*2*8) /* 60 * 2 * 8 byte */

/* RX */
#define SCO_RX_PLC_SIZE              (30)
#define SCO_RX_PACKER_BUF_NUM        (16)   /* 16*/
#define SCO_RX_PACKET_MASK           (0xF)   /* 0xF */
#define SCO_RX_PCM64K_BUF_SIZE       (SCO_RX_PLC_SIZE*2*8)
#define SCO_RX_PCM8K_BUF_SIZE        (SCO_RX_PLC_SIZE*2)

#define BTSCO_CVSD_RX_FRAME SCO_RX_PACKER_BUF_NUM
#define BTSCO_CVSD_RX_INBUF_SIZE (BTSCO_CVSD_RX_FRAME*SCO_RX_PLC_SIZE)
#define BTSCO_CVSD_PACKET_VALID_SIZE 2
#define BTSCO_CVSD_RX_TEMPINPUTBUF_SIZE (BTSCO_CVSD_RX_FRAME*(SCO_RX_PLC_SIZE+BTSCO_CVSD_PACKET_VALID_SIZE))

#define BTSCO_CVSD_TX_FRAME SCO_TX_PACKER_BUF_NUM
#define BTSCO_CVSD_TX_OUTBUF_SIZE (BTSCO_CVSD_TX_FRAME*SCO_TX_ENCODE_SIZE)

typedef	uint8_t kal_uint8;
typedef	int8_t kal_int8;
typedef uint16_t kal_uint16;
typedef	uint32_t kal_uint32;
typedef	int32_t kal_int32;
typedef	uint64_t kal_uint64;
typedef	int64_t kal_int64;
typedef bool kal_bool;

static const kal_uint32 btsco_PacketValidMask[6][6] = {
		{0x1   , 0x1 << 1, 0x1 << 2, 0x1 << 3, 0x1 << 4 , 0x1 << 5 }, /* 30 */
		{0x1   , 0x1   , 0x2   , 0x2   , 0x4    , 0x4    },  /* 60 */
		{0x1   , 0x1   , 0x1   , 0x2   , 0x2    , 0x2    },  /* 90 */
		{0x1   , 0x1   , 0x1   , 0x1   , 0              , 0      },  /* 120 */
		{0x7   , 0x7 << 3, 0x7 << 6, 0x7 << 9, 0x7 << 12, 0x7 << 15}, /* 10 */
		{0x3   , 0x3 << 1, 0x3 << 3, 0x3 << 4, 0x3 << 6 , 0x3 << 7 }
}; /* 20 */

static const kal_uint8 btsco_PacketInfo[6][6] = {
		{ 30, 6, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE}, /* 30 */
		{ 60, 3, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE}, /* 60 */
		{ 90, 2, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE}, /* 90 */
		{120, 1, BT_SCO_PACKET_120 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_120 / SCO_RX_PLC_SIZE}, /* 120 */
		{ 10, 18, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE}, /* 10 */
		{ 20, 9, BT_SCO_PACKET_180 / SCO_TX_ENCODE_SIZE, BT_SCO_PACKET_180 / SCO_RX_PLC_SIZE}
}; /* 20 */


typedef enum {
		BT_SCO_TXSTATE_IDLE = 0x0,
		BT_SCO_TXSTATE_INIT,
		BT_SCO_TXSTATE_READY,
		BT_SCO_TXSTATE_RUNNING,
		BT_SCO_TXSTATE_ENDING,
		BT_SCO_RXSTATE_IDLE = 0x10,
		BT_SCO_RXSTATE_INIT,
		BT_SCO_RXSTATE_READY,
		BT_SCO_RXSTATE_RUNNING,
		BT_SCO_RXSTATE_ENDING,
		BT_SCO_TXSTATE_DIRECT_LOOPBACK
} CVSD_STATE;

typedef enum {
		BT_SCO_DIRECT_BT2ARM,
		BT_SCO_DIRECT_ARM2BT
} BT_SCO_DIRECT;

typedef enum {
		BT_SCO_CVSD_30 = 0,
		BT_SCO_CVSD_60 = 1,
		BT_SCO_CVSD_90 = 2,
		BT_SCO_CVSD_120 = 3,
		BT_SCO_CVSD_10 = 4,
		BT_SCO_CVSD_20 = 5,
		BT_SCO_CVSD_MAX = 6
} BT_SCO_PACKET_LEN;


typedef struct {
		dma_addr_t pucTXPhysBufAddr;
		dma_addr_t pucRXPhysBufAddr;
		kal_uint8 *pucTXVirtBufAddr;
		kal_uint8 *pucRXVirtBufAddr;
		kal_int32 u4TXBufferSize;
		kal_int32 u4RXBufferSize;
		struct snd_pcm_substream *TX_substream;
		struct snd_pcm_substream *RX_substream;
		struct snd_dma_buffer *TX_btcvsd_dma_buf;
		struct snd_dma_buffer *RX_btcvsd_dma_buf;
} CVSD_MEMBLOCK_T;


typedef struct {
		kal_uint8
		PacketBuf[SCO_RX_PACKER_BUF_NUM][SCO_RX_PLC_SIZE + BTSCO_CVSD_PACKET_VALID_SIZE];
		kal_bool                PacketValid[SCO_RX_PACKER_BUF_NUM];
		int   iPacket_w;
		int   iPacket_r;
		kal_uint8         TempPacketBuf[BT_SCO_PACKET_180];
		kal_bool          fOverflow;
		kal_uint32      u4BufferSize;   /* RX packetbuf size */
} BT_SCO_RX_T;

typedef struct {
		kal_uint8         PacketBuf[SCO_TX_PACKER_BUF_NUM][SCO_TX_ENCODE_SIZE];
		kal_int32        iPacket_w;
		kal_int32        iPacket_r;
		kal_uint8         TempPacketBuf[BT_SCO_PACKET_180];
		kal_bool          fUnderflow;
		kal_uint32      u4BufferSize; /* TX packetbuf size */
} BT_SCO_TX_T;

typedef struct {
		BT_SCO_TX_T *pTX;
		BT_SCO_RX_T *pRX;
		kal_uint8 *pStructMemory;
		kal_uint8 *pWorkingMemory;
		kal_uint16 uAudId;
		CVSD_STATE uTXState;
		CVSD_STATE uRXState;
		kal_bool  fIsStructMemoryOnMED;
} BT_SCO_T;
extern BT_SCO_T btsco;

extern volatile kal_uint32 *bt_hw_REG_PACKET_W, *bt_hw_REG_PACKET_R;
extern volatile kal_uint32 *bt_hw_REG_CONTROL;

extern CVSD_MEMBLOCK_T BT_CVSD_Mem;

extern kal_uint32 disableBTirq;

extern bool isProbeDone;

/*****************************************************************************
 *    BT SCO Internal Function
*****************************************************************************/
void AudDrv_BTCVSD_DataTransfer(BT_SCO_DIRECT uDir, kal_uint8 *pSrc,
					kal_uint8 *pDst, kal_uint32 uBlockSize, kal_uint32 uBlockNum,
					CVSD_STATE uState);
void AudDrv_BTCVSD_ReadFromBT(BT_SCO_PACKET_LEN uLen,
					kal_uint32 uPacketLength, kal_uint32 uPacketNumber, kal_uint32 uBlockSize,
					kal_uint32 uControl);
void AudDrv_BTCVSD_WriteToBT(BT_SCO_PACKET_LEN uLen,
					kal_uint32 uPacketLength, kal_uint32 uPacketNumber, kal_uint32 uBlockSize);
bool Register_BTCVSD_Irq(void *dev, uint32 irq_number);
int AudDrv_BTCVSD_IRQ_handler(void);
int AudDrv_btcvsd_Allocate_Buffer(kal_uint8 isRX);
int AudDrv_btcvsd_Free_Buffer(kal_uint8 isRX);
ssize_t AudDrv_btcvsd_read(char __user *data, size_t count);
ssize_t AudDrv_btcvsd_write(const char __user *data, size_t count);
void Disable_CVSD_Wakeup(void);
void Enable_CVSD_Wakeup(void);
void Set_BTCVSD_State(unsigned long arg);


/* here is temp address for ioremap BT hardware register */
extern volatile void *BTSYS_PKV_BASE_ADDRESS;
extern volatile void *BTSYS_SRAM_BANK2_BASE_ADDRESS;
extern volatile void *AUDIO_INFRA_BASE_VIRTUAL;


#ifdef CONFIG_OF
static volatile unsigned long btsys_pkv_physical_base;
static volatile unsigned long btsys_sram_bank2_physical_base;
static volatile unsigned long infra_base;
static volatile unsigned long infra_misc_offset;
static volatile unsigned long conn_bt_cvsd_mask;
static volatile unsigned long cvsd_mcu_read_offset;
static volatile unsigned long cvsd_mcu_write_offset;
static volatile unsigned long cvsd_packet_indicator;
#else
#define AUDIO_BTSYS_PKV_PHYSICAL_BASE  (0x18000000)
#define AUDIO_BTSYS_SRAM_BANK2_PHYSICAL_BASE  (0x18080000)
#define AUDIO_INFRA_BASE_PHYSICAL (0x10000000)
#define INFRA_MISC_OFFSET (0x0700)   /* INFRA_MISC address=AUDIO_INFRA_BASE_PHYSICAL + INFRA_MISC_OFFSET */
#define conn_bt_cvsd_mask (0x00000800)   /* bit 11 of INFRA_MISC */
#define CVSD_MCU_READ_OFFSET (0xFD0)
#define CVSD_MCU_WRITE_OFFSET (0xFD4)
#define CVSD_PACKET_INDICATOR (0xFD8)
#endif
#define AP_BT_CVSD_IRQ_LINE (260)
extern u32 btcvsd_irq_number;

#endif
