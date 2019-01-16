#ifndef _EEMCS_CHAR_H
#define _EEMCS_CHAR_H
#include <asm/ioctl.h>
#include <asm/atomic.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include "eemcs_ccci.h"
#include "eemcs_kal.h"

#define EEMCS_DEV_MAJOR 183
#define EEMCS_DEV_NAME "eemcs"
#define EEMCS_CDEV_MAX_NUM (END_OF_NORMAL_PORT-START_OF_NORMAL_PORT)
#define EEMCS_DEV_NAME_LEN  30

typedef enum _CCCI_CDEV_STATE{
    CDEV_CLOSE = 0,
    CDEV_OPEN  = 1    
}CCCI_CDEV_STATE;

typedef struct
{
    struct sk_buff      *remaining_rx_skb; /* the pointer for streaming read */
    atomic_t            remaining_rx_cnt;
    KAL_UINT32          remaining_len;
}stream_buff_t;

typedef struct 
{
    KAL_CHAR            cdev_name[32];
    KAL_UINT8           eemcs_port_id;
    ccci_ch_set         ccci_ch;
    atomic_t            cdev_state;  /*0: close 1:open*/
    /* rx informaiont */
    /* 1. [eemcs_cdev_rx_callback] enqueue rx_skb_list 
     * 2. [eemcs_cdev_rx_callback] rx_pkt_cnt ++
     * 3. [eemcs_cdev_rx_callback] wake_up rx_waitq
     * 4. [emcs_cdev_read]         (wait_event_interruptible rx_waitq)
     * 5. [emcs_cdev_read]         rx_pkt_cnt --
     * 6. [emcs_cdev_read]         dequeue rx_skb_list
     */
    wait_queue_head_t   rx_waitq;
    struct sk_buff_head rx_skb_list;
    atomic_t            rx_pkt_cnt;
    atomic_t            rx_pkt_drop_cnt; /* happen when user memory is not enough */
    stream_buff_t       buff;
    /* tx informaiont */    
    wait_queue_head_t   tx_waitq;
    atomic_t            tx_pkt_cnt;
}eemcs_cdev_node_t;

typedef struct 
{
    struct class        *dev_class;        /* class_create/class_destroy/device_create/device_destroy */
    struct cdev         *eemcs_chrdev;     /* cdev_alloc/cdev_del/cdev_init/cdev_add */
    eemcs_cdev_node_t   cdev_node[EEMCS_CDEV_MAX_NUM];
    KAL_UINT32          expt_cb_id;        /* exception callback function ID */
}eemcs_cdev_inst_t;



/* EMCS opeartion Error. */
//  I for Internal use, O for device status, should cover by error handling..
#define EMCS_ERR_NONE           0
#define EMCS_ERR_TIMEOUT        20 /*[O] wait Interrupt or device read fail*/
#define EMCS_ERR_BT_STATUS      21 /*[I] invalid MD Status.*/
#define EMCS_ERR_CMDCRC         22 /*[O] invalid command*/
#define EMCS_ERR_LOAD_BIN       23 /*[O] MD BIN file open fail*/
#define EMCS_ERR_MSG_OVERFLOW   24 /*[O] receive message to long*/
#define EMCS_ERR_PKT_OVERFLOW   25 /*[I] send package big the tx limitaion*/
#define EMCS_ERR_INVALID_PARA   26 /*[I] emcs driver parameter check fail*/
#define EMCS_ERR_GET_OWNER      27 /*[O] get device ownership fail*/     
#define EMCS_ERR_NOMEM          28
#define EMCS_ERR_NOINIT         29
#define EMCS_ERR_INVAL_PARA     30
#define EMCS_ERR_TX_FAIL        31
#define EMCS_ERR_RX_FAIL        32
#define EMCS_ERROR_BUSY         33
#define EMCS_ERROR_NODEV        34
#define EMCS_ERR_RESET_MD       35

#if defined(_EEMCS_CDEV_LB_UT_) || defined(_EEMCS_FS_UT)
KAL_UINT32 cdevut_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data) ;
KAL_UINT32 cdevut_unregister_callback(CCCI_CHANNEL_T chn);
inline KAL_INT32 cdevut_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb);
inline KAL_UINT32 cdevut_UL_write_room_alloc(CCCI_CHANNEL_T chn);
inline KAL_UINT32 cdevut_UL_write_room_release(CCCI_CHANNEL_T chn);
KAL_UINT32 cdevut_UL_write_wait(CCCI_CHANNEL_T chn);
typedef void (*EEMCS_CCCI_EXCEPTION_IND_CALLBACK)(KAL_UINT32 msg_id);
KAL_UINT32 cdevut_register_expt_callback(EEMCS_CCCI_EXCEPTION_IND_CALLBACK func_ptr);
KAL_UINT32 cdevut_unregister_expt_callback(KAL_UINT32 cb_id);
void cdevut_turn_off_dlq_by_port(KAL_UINT32 port_idx);
void cdevut_turn_on_dlq_by_port(KAL_UINT32 port_idx);

#define ccci_cdev_register(ch_num,cb,para)           cdevut_register_callback(ch_num,cb,para) 
#define ccci_cdev_unregister(ch_num)			     cdevut_unregister_callback(ch_num) 
#define ccci_cdev_write_desc_to_q(ch_num,desc_p)     cdevut_UL_write_skb_to_swq(ch_num, desc_p)
#define ccci_cdev_write_space_alloc(ch_num)          cdevut_UL_write_room_alloc(ch_num)
#define ccci_cdev_write_space_release(ch_num)        cdevut_UL_write_room_release(ch_num)
#define ccci_cdev_write_wait(ch_num)                 cdevut_UL_write_wait(ch_num)
#define ccci_cdev_register_expt_callback(cb)         cdevut_register_expt_callback(cb)
#define ccci_cdev_unregister_expt_callback(cb)       cdevut_unregister_expt_callback(cb)
#define ccci_cdev_turn_off_dl_q(port_idx)            cdevut_turn_off_dlq_by_port(port_idx)
#define ccci_cdev_turn_on_dl_q(port_idx)             cdevut_turn_on_dlq_by_port(port_idx)

#else
#define ccci_cdev_register(ch_num,cb,para)           eemcs_ccci_register_callback(ch_num,cb,para) 
#define ccci_cdev_unregister(ch_num)			     eemcs_ccci_unregister_callback(ch_num) 
#define ccci_cdev_write_desc_to_q(ch_num,desc_p)     eemcs_ccci_UL_write_skb_to_swq(ch_num, desc_p)
#define ccci_cdev_write_space_alloc(ch_num)          eemcs_ccci_UL_write_room_alloc(ch_num)
#define ccci_cdev_write_space_release(ch_num)        eemcs_ccci_UL_write_room_release(ch_num)
#define ccci_cdev_write_wait(ch_num)                 eemcs_ccci_UL_write_wait(ch_num)   
#define ccci_cdev_register_expt_callback(cb)         eemcs_register_expt_callback(cb)
#define ccci_cdev_unregister_expt_callback(cb)       eemcs_unregister_expt_callback(cb)
#define ccci_cdev_turn_off_dl_q(port_idx)            eemcs_ccci_turn_off_dlq_by_port(port_idx)
#define ccci_cdev_turn_on_dl_q(port_idx)             eemcs_ccci_turn_on_dlq_by_port(port_idx)
#endif

#ifdef CCCI_SDIO_HEAD
#define CCCI_CDEV_HEADER_ROOM                   (sizeof(SDIO_H)+sizeof(CCCI_BUFF_T))
#else
#define CCCI_CDEV_HEADER_ROOM                   (sizeof(CCCI_BUFF_T))
#endif

//#define ccci_cdev_mem_alloc(sz)                 dev_alloc_skb(sz)
#define ccci_cdev_mem_alloc(sz, flag)            __dev_alloc_skb(sz, flag)

KAL_INT32 eemcs_char_mod_init(void);
void eemcs_char_exit(void);

void eemcs_fs_ut_callback(struct sk_buff *, KAL_UINT32);
bool eemcs_cdev_rst_port_closed(void);
void eemcs_cdev_reset(void);

#endif //_EEMCS_CHAR_H

/*********************************************************************
[SOP to add a char device node] 20130525 Ian
[EEMCS] mediatek/kernel/source/kernel/driver/eemcs
1. add channel in eemcs_ccci.h CCCI_CHANNEL_T
2. add port in eemcs_ccci.h CCCI_PORT
3. update ccci_port_info in eemcs_ccci.c
4. update ccci_cdev_name in eemcs_char.c
5. update exception port information ccci_expt_port_info in eemcs_expt.c
[eemcs_test] mediatek/source/external/eemcs_test
1. add channel in CCCI_CHANNEL_T
2. add port in CCCI_PORT
3. CCCI_CDEV_NUM++
4. update ccci_cdev_name, ccci_cdev_name_select, ccci_expose_cccih
5. modify port_to_tx_cccich function 
*********************************************************************/
