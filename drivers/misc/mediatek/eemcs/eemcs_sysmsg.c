/*****************************************************************************
 *
 * Filename:
 * ---------
 *   eemcs_sysmsg.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT6582 CCCI SYSTEM MESSAGE
 *
 * Author:
 * -------
 *   
 *
 ****************************************************************************/

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#include "eemcs_kal.h"
#include "eemcs_ccci.h"
#include "eemcs_debug.h"
#include "eemcs_char.h"
#include "eemcs_file_ops.h"
#include "eemcs_sysmsg.h"
#include "mach/mtk_eemcs_helper.h"
#include "eemcs_state.h"

static spinlock_t       sysmsg_fifo_lock = __SPIN_LOCK_UNLOCKED(sysmsg_fifo_lock);
static struct kfifo     sysmsg_fifo;
struct work_struct      sysmsg_work;


//#define __EEMCS_SYSMSG_IT__
#ifdef __EEMCS_SYSMSG_IT__
#define TEST_MSG_ID_MD2AP (0x113)
#define TEST_MSG_ID_AP2MD (0x107)
#endif

//------------For MD get Battery info from AP------------//
void send_battery_info(int md_id)
{
	int	ret = 0;
	unsigned int para = 0;
	unsigned int resv = 0;
	unsigned int msg_id = EXT_MD_GET_BATTERY_INFO;

	resv = eemcs_get_bat_info(para);
	ret = eemcs_notify_md_by_sys_msg(md_id, msg_id, resv);
    DBGLOG(SMSG, INF, "send bat vol(%d) to md: %d", resv, ret);     
    
	return;
}
int eemcs_md_get_battery_info(int md_id, int data)
{
	DBGLOG(SMSG,DBG,"request to send battery voltage to md");
	send_battery_info(md_id);		      
	return 0;
}
//--------------end-------------------------//

//------------For MD send sim type msg to AP------------//

int enable_get_sim_type(int md_id, unsigned int enable)
{
	int ret = 0;
	unsigned int msg_id = EXT_MD_SIM_TYPE;
	unsigned int   resv = enable;
	
	ret = eemcs_notify_md_by_sys_msg(MD_SYS5, msg_id, resv);

	DBGLOG(SMSG,INF,"enable_get_sim_type(enable=%d): %d", resv, ret);

	return ret;
}

int sim_type = 0xEEEEEEEE;	//sim_type(MCC/MNC) send by MD wouldn't be 0xEEEEEEEE
int set_sim_type(int md_id, int data)
{
	int ret = 0;
	sim_type = data;
	
	DBGLOG(SMSG,INF,"set_sim_type(type=0x%x): %d", sim_type, ret);

	return ret;
}

int get_sim_type(int md_id, int *p_sim_type)
{
	*p_sim_type = sim_type;
	if (sim_type == 0xEEEEEEEE)
	{
		DBGLOG(SMSG,ERR,"get_sim_type fail: md not send sim type yet, type=0x%x", sim_type);
		return -1;
	} else
		DBGLOG(SMSG,INF,"get_sim_type: type=0x%x", sim_type);
	
	return 0;
}

//--------------end-------------------------//

static void eemcs_sysmsg_work(struct work_struct *work)
{
    //unsigned char *p_rdata;
    struct sk_buff *skb;
    CCCI_BUFF_T *p_cccih = NULL;
	int	md_id = MD_SYS5;

	DBGLOG(SMSG, DBG, "====> %s", FUNC_NAME);  

    while(kfifo_out(&sysmsg_fifo, &skb, sizeof(unsigned int)))
    {
        p_cccih = (CCCI_BUFF_T *)skb->data;
        DBGLOG(SMSG, DBG, "eemcs_sysmsg_work, msg: %08X, %08X, %08X, %08X\n", \
			p_cccih->data[0], p_cccih->data[1], p_cccih->channel, p_cccih->reserved);
        eemcs_exec_ccci_sys_call_back(md_id, p_cccih->data[1], p_cccih->reserved);
        dev_kfree_skb(skb);
    }
    DBGLOG(SMSG, DBG, "<==== %s", FUNC_NAME);  
}

static int send_eemcs_system_ch_msg(int md_id, unsigned int msg_id, unsigned int data)
{
    CCCI_BUFF_T * ccci_header;
    struct sk_buff *new_skb;
    unsigned int skb_len = CCCI_SYSMSG_HEADER_ROOM;
    int ret = KAL_FAIL;

    DBGLOG(SMSG, DBG, "send_eemcs_system_ch_msg"); 
	
    if (check_device_state() != EEMCS_BOOTING_DONE) {//modem not ready
        DBGLOG(CHAR, ERR, "send sys msg(id=%d) fail when modem not ready", msg_id);
        return -ENODEV;
    }

    //when alloc skb fail, not use __GFP_WAIT because this API may be used in interrupt context
    while(NULL == (new_skb = dev_alloc_skb(skb_len)))
    {        
        DBGLOG(SMSG, ERR, "dev_alloc_skb fail");      
    }

    /* reserve SDIO_H and CCCI header room */
    #ifdef CCCI_SDIO_HEAD
    skb_reserve(new_skb, sizeof(SDIO_H));
    #endif
    ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ;

    
    ccci_header->data[0] = 0xFFFFFFFF;
    ccci_header->data[1] = msg_id;
    ccci_header->channel = CH_SYS_TX;
    ccci_header->reserved = data;

    
    DBGLOG(SMSG, DBG, "send_sys_msg: 0x%08X, 0x%08X, 0x%02d, 0x%08X", 
            ccci_header->data[0],ccci_header->data[1] ,ccci_header->channel, ccci_header->reserved);  

    ret = ccci_sysmsg_ch_write_desc_to_q(ccci_header->channel, new_skb);
    if(ret != KAL_SUCCESS)
    {
		DBGLOG(SMSG, ERR, "send_sys_msg(id=%d, data=0x%x) fail: %d", msg_id, ccci_header->reserved, ret);
		dev_kfree_skb(new_skb);
	} else {
	    DBGLOG(SMSG, DBG, "send_sys_msg(id=%d, data=0x%x) OK: %d", msg_id, ccci_header->reserved, ret);
	}   
	return ret;
}

// eemcs_sysmsg_rx_dispatch_cb: CCCI_SYSTEM_RX message dispatch call back function for MODEM
// @skb: pointer to a CCCI buffer
// @private_data: pointer to private data of CCCI_SYSTEM_RX
KAL_INT32 eemcs_sysmsg_rx_dispatch_cb(struct sk_buff *skb, KAL_UINT32 private_data)
{
    CCCI_BUFF_T *p_cccih = NULL;

	DBGLOG(SMSG, DBG, "====> %s", FUNC_NAME);  

    if (skb){
		p_cccih = (CCCI_BUFF_T *)skb->data;
		DBGLOG(SMSG, INF, "sysmsg RX callback, msg: %08X, %08X, %02d, %08X\n", p_cccih->data[0], p_cccih->data[1], p_cccih->channel, p_cccih->reserved);

        if (p_cccih->channel == CH_SYS_TX){
		    DBGLOG(SMSG, ERR, "Wrong CH for recv");
            return KAL_FAIL;
        }

        if (kfifo_is_full(&sysmsg_fifo))
        {
		    DBGLOG(SMSG, ERR, "kfifo full and packet drop, msg: %08X, %08X, %02d, %08X\n", \
				p_cccih->data[0], p_cccih->data[1], p_cccih->channel, p_cccih->reserved);
            dev_kfree_skb(skb);
            return KAL_FAIL;
        }
        
        spin_lock_bh(&sysmsg_fifo_lock);
    	//DBGLOG(SMSG, TRA, "ready to put skb into FIFO");
    	kfifo_in(&sysmsg_fifo, &skb, sizeof(unsigned int));
    	//DBGLOG(SMSG, TRA, "after put skb into FIFO");
    	spin_unlock_bh(&sysmsg_fifo_lock);
    	DBGLOG(SMSG, DBG, "schedule sysmsg_work");
       	schedule_work(&sysmsg_work);
        
    }
    else
    {
        DBGLOG(SMSG, ERR, "skb is NULL!");
        return KAL_FAIL;
    }

    DBGLOG(SMSG, DBG, "<==== %s", FUNC_NAME);  
    return KAL_SUCCESS;
}

#ifdef __EEMCS_SYSMSG_IT__
int eemcs_sysmsg_echo_test(int md_id, int data)
{
    eemcs_notify_md_by_sys_msg(md_id, TEST_MSG_ID_AP2MD, data);
    return 0;
}
#endif

int eemcs_sysmsg_mod_init(void)
{ 	
    int ret = 0;

    DBGLOG(SMSG, DBG, "====> %s", FUNC_NAME);  

    ret = kfifo_alloc(&sysmsg_fifo, sizeof(unsigned)*CCCI_SYSMSG_MAX_REQ_NUM, GFP_KERNEL);
    if (ret)
    {
        DBGLOG(SMSG, ERR, "kfifo_alloc fail: %d", ret);
        return ret;
    }

    //related channel registration.
    //RX
    KAL_ASSERT(ccci_ch_register((CCCI_CHANNEL_T)CH_SYS_RX, eemcs_sysmsg_rx_dispatch_cb, 0) == KAL_SUCCESS);
	//TX
    eemcs_register_sys_msg_notify_func(MD_SYS5, send_eemcs_system_ch_msg);  

    INIT_WORK(&sysmsg_work, eemcs_sysmsg_work);

#ifdef __EEMCS_SYSMSG_IT__
    eemcs_register_ccci_sys_call_back(MD_SYS5, TEST_MSG_ID_MD2AP, eemcs_sysmsg_echo_test);
#endif

    //DBGLOG(SMSG, TRA, "register sys msg callback: md_get_battery_info");
    eemcs_register_ccci_sys_call_back(MD_SYS5, EXT_MD_GET_BATTERY_INFO , eemcs_md_get_battery_info);  //EXT_MD_GET_BATTERY_INFO == MD_GET_BATTERY_INFO == 0x105

    eemcs_register_ccci_sys_call_back(MD_SYS5, EXT_MD_SIM_TYPE, set_sim_type);

    DBGLOG(SMSG, DBG, "<==== %s", FUNC_NAME);  
		
    return 0;
}

void eemcs_sysmsg_exit(void)
{
	DBGLOG(SMSG, INF, "====> %s", FUNC_NAME);  
	kfifo_free(&sysmsg_fifo);
}

