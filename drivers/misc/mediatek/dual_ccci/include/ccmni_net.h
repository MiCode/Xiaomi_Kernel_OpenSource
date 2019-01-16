/************************************
*
*  CCMNI Memory Layout
*
*|------------------------------|
*|                              |
*|                              | 
*|    CCMNI UP-Link Memory      |
*|    (Maintained by MD)        |
*|                              |
*|------------------------------|
*|    CCMNI1 RX Memory          |
*|------------------------------|
*|    CCMNI2 RX Memory          |
*|------------------------------|
*|            ...               |
*|------------------------------|
*|    CCMNIn RX Memory          |
*|------------------------------|
*|    CCMNI1 - Ctrl Memory      |
*|------------------------------|
*|    CCMNI2 - Ctrl Memory      |
*|------------------------------|
*|            ...               |
*|------------------------------|
*|    CCMNIn - Ctrl Memory      |
*|------------------------------|
************************************
*
*  CCMNI ctrl memory layout
*
*|------------------------------|
*|    CCMNI1 - TX Read out      |
*|------------------------------|
*|    CCMNI1 - TX Avai out      |
*|------------------------------|
*|    CCMNI1 - TX Avai in       |
*|------------------------------|
*|    CCMNI1 - TX Queue len     |
*|------------------------------|
*|                              |
*|                              | 
*|    CCMNI1 TX Queue           |
*|                              |
*|                              |
*|------------------------------|
*|    CCMNI1 - RX Read out      |
*|------------------------------|
*|    CCMNI1 - RX Avai out      |
*|------------------------------|
*|    CCMNI1 - RX Avai in       |
*|------------------------------|
*|    CCMNI1 - RX Queue len     |
*|------------------------------|
*|                              |
*|                              | 
*|    CCMNI1 RX Queue           |
*|                              |
*|                              |
*|------------------------------|
*************************************
*
*  CCMNI RX memory layout
*
*|------------------------------|
*|    Buffer[0] 1500B + 28B     |
*|------------------------------|
*|    Buffer[1] 1500B + 28B     |
*|------------------------------|
*|             ...              |
*|------------------------------|
*|    Buffer[n] 1500B + 28B     |
*|------------------------------|
***********************************
*
*CCMNI ctrl Q memory layout (ring buffer)
*For example, ptr_n points to the start address of RX memory's Data field of Buffer[n],
*len_n stands for the length of RX memory's Data field of Buffer[n] (not including End Byte).
*
*|-----------------------------|
*|          ptr_n / len_n      |
*|-----------------------------|
*|            ...              |
*|-----------------------------|
*|          ptr_1 / len_1      |
*|-----------------------------|
*|          ptr_1 / len_1      |
*|-----------------------------|
***********************************/

/****************Data buffer mem layout***************
*|-------------------------------------------------------------------------------|
*| DBG(16B) | Header(4B) | Data Field (1500B Max.) + End Byte (1B) + Blank (nB) | Footer (4B) |
*|-------------------------------------------------------------------------------|
*
*|----------------------------------------------------------|
*|                                          DBG                                              |
*|----------------------------------------------------------|
*| port (1B) | avail in no. (1B) | avail out no. (1B) | read out no. (1B)    |
*|----------------------------------------------------------|
************************************************/

#ifndef __CCCI_CCMNI_H__
#define __CCCI_CCMNI_H__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/timer.h>
#include <linux/if_ether.h>
#include <asm/bitops.h>
#include <asm/dma-mapping.h>
#include <mach/mt_typedefs.h>

#define  CCCI_NETWORK           0
#define  CCMNI_MTU              1500

#define  CCMNI_CHANNEL_OFFSET   2
//#define CCMNI_MAX_CHANNELS        NET_PORT_NUM
//#define CCMNI_CHANNEL_CNT        CCMNI_V2_PORT_NUM //Currently, we use 3 channels.


#define  IPV4_VERSION 0x40
#define  IPV6_VERSION 0x60

//Flags bit used for timer
#define CCMNI_RECV_ACK_PENDING        (0)
#define CCMNI_SEND_PENDING            (1)
#define CCMNI_RECV_PENDING            (2)

#define  CCMNI_TX_QUEUE         8
//To store 256 ptr_n & len_n. Normally we use 100.
//If current channel busy, we may use the rest by negotiating with other channels,
//other channels may release their unused resources.
#define CCMNI_CTRL_Q_RX_SIZE    (256)
#define CCMNI_CTRL_Q_TX_SIZE    (64)

#define CCMNI_RX_ACK_BOUND         (CCMNI_CTRL_Q_RX_SIZE_DEFAULT/4)

#define CCMNI_CTRL_Q_RX_SIZE_DEFAULT    100

#define CCMNI_SINGLE_BUFF_SIZE        (CCMNI_MTU + 28)
#define CCMNI_BUFF_DATA_FIELD_SIZE    (CCMNI_SINGLE_BUFF_SIZE -24)    //1500+1+3

#define CCCI_CCMNI_SMEM_UL_SIZE        (300 * 1024)
#define CCCI_CCMNI_SMEM_DL_SIZE        (300 * CCMNI_SINGLE_BUFF_SIZE)

//Used for negotiating
#define RX_BUF_RESOURCE_LOWER_BOUND (20)
#define RX_BUF_RESOURCE_UPPER_BOUND (80)    //Should be more than 2*RX_BUF_RESOURCE_OCCUPY_CNT
#define RX_BUF_RESOURCE_OCCUPY_CNT  (20)    //((RX_BUF_RESOURCE_UPPER_BOUND - RX_BUF_RESOURCE_LOWER_BOUND)/2 )


typedef struct
{
    unsigned port;
    unsigned avai_in_no;
    unsigned avai_out_no;
    unsigned read_out_no;
} dbg_info_ccmni_t;

#define CCMNI_BUFF_DBG_INFO_SIZE    (sizeof(dbg_info_ccmni_t))
#define CCMNI_BUFF_HEADER_SIZE        (4)
#define CCMNI_BUFF_FOOTER_SIZE        (4)

#define CCMNI_BUFF_HEADER            (0xE1E1E1E1)
#define CCMNI_BUFF_FOOTER            (0xE2E2E2E2)
#define CCMNI_DATA_END                (0xE3)

typedef struct
{
    unsigned char *ptr;
    unsigned len;
} q_ringbuf_ccmni_t;

typedef struct
{
    unsigned avai_in;
    unsigned avai_out;
    unsigned read_out;
    unsigned q_length;        //default 256 for Rx
} buffer_control_ccmni_t;

typedef struct
{
    buffer_control_ccmni_t    rx_control;                                //Down Llink
    q_ringbuf_ccmni_t        q_rx_ringbuff[CCMNI_CTRL_Q_RX_SIZE];    //Down Link, default 256
    buffer_control_ccmni_t    tx_control;                                //Up Link
    q_ringbuf_ccmni_t        q_tx_ringbuff[CCMNI_CTRL_Q_TX_SIZE];    //Up Link, default 64
} shared_mem_ccmni_t;
    
#define CCCI_CCMNI_SMEM_SIZE     (sizeof(shared_mem_ccmni_t))
#define CCMNI_DL_CTRL_MEM_SIZE    ((sizeof(buffer_control_ccmni_t)) + (CCMNI_CTRL_Q_RX_SIZE * (sizeof(q_ringbuf_ccmni_t))))
#define CCMNI_UL_CTRL_MEM_SIZE    ((sizeof(buffer_control_ccmni_t)) + (CCMNI_CTRL_Q_TX_SIZE * (sizeof(q_ringbuf_ccmni_t))))

#endif // __CCCI_CCMNI_H__ 