#include <linux/sched.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>
#include <asm/dma-mapping.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <mach/mtk_eemcs_helper.h>
#include "eemcs_kal.h"
#include "eemcs_debug.h"
#include "eemcs_char.h"
#include "eemcs_state.h"
#include "eemcs_boot.h"
#include "eemcs_ccci.h"
#include "eemcs_md.h"
#include "eemcs_statistics.h"
#include "eemcs_sysmsg.h"

static eemcs_cdev_inst_t eemcs_cdev_inst;
static KAL_UINT8 ccci_cdev_name[EEMCS_CDEV_MAX_NUM][32]=
{
    "eemcs_sys",        /* START_OF_NORMAL_PORT */
    "eemcs_aud",
    "eemcs_md_log_ctrl", /*eemcs_meta*/
    "eemcs_mux",
    "eemcs_fs",
    "eemcs_pmic",
    "eemcs_uem",
    "eemcs_rpc",
    "eemcs_ipc",
    "eemcs_ipc_uart",
    "eemcs_md_log",
    "eemcs_imsv",    /* ims video */
    "eemcs_imsc",    /* ims control */
    "eemcs_imsa",    /* ims audio */
    "eemcs_imsdc",   /* ims data control */
    "eemcs_muxrp",   /* mux report channel, support ioctl only no i/o*/
    "eemcs_ioctl",   /* ioctl channel, support ioctl only no i/o*/
    "eemcs_ril",     /* rild channel, support ioctl only no i/o*/
    "eemcs_it",      /* END_OF_NORMAL_PORT-1 */
};

static unsigned int       catch_last_log = 0;
static spinlock_t         md_logger_lock;
static eemcs_cdev_node_t* md_log_node = NULL; 
extern struct wake_lock   eemcs_wake_lock;


#define PORT2IDX(port) ((port)-START_OF_NORMAL_PORT)
#define IDX2PORT(idx) ((idx)+START_OF_NORMAL_PORT)

static KAL_INT32 eemcs_cdev_rx_callback(struct sk_buff *skb, KAL_UINT32 private_data)
{
    CCCI_BUFF_T *p_cccih = NULL;
    KAL_UINT32  port_id;

    DEBUG_LOG_FUNCTION_ENTRY;

    if (skb){
        p_cccih = (CCCI_BUFF_T *)skb->data;
        DBGLOG(CHAR, DBG, "cdev_rx_callback: CCCI_H(0x%08X, 0x%08X, %02d, 0x%08X",\
        	p_cccih->data[0],p_cccih->data[1],p_cccih->channel, p_cccih->reserved );
    }
    port_id = ccci_ch_to_port(p_cccih->channel);
    
    //if(CDEV_OPEN == atomic_read(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].cdev_state)) {
        skb_queue_tail(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].rx_skb_list, skb); /* spin_lock_ireqsave inside, refering skbuff.c */
        atomic_inc(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].rx_pkt_cnt);     /* increase rx_pkt_cnt */
        eemcs_update_statistics_number(0, port_id, RX, QUEUE, \
			atomic_read(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].rx_pkt_cnt));
        wake_up(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].rx_waitq);          /* wake up rx_waitq */

    #if 0
    }else{
        if(port_id != CCCI_PORT_MD_LOG) /* If port_id == CCCI_PORT_MD_LOG, skip drop info (request by ST team)*/
        {
            DBGLOG(CHAR, ERR, "!!! PKT DROP when cdev(%d) close", port_id);
        }
        dev_kfree_skb(skb);	
        eemcs_ccci_release_rx_skb(port_id, 1, skb);
        eemcs_update_statistics(0, port_id, RX, DROP);
    }
    #endif
	
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS ;
}

static int eemcs_cdev_open(struct inode *inode,  struct file *file)
{
    int id = iminor(inode);
    int ret = 0;
	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CHAR,INF,"cdev_open: open dev(%s, %d)",\
        eemcs_cdev_inst.cdev_node[PORT2IDX(id)].cdev_name, id);

    //4 <1> check multiple open
    if(CDEV_OPEN == atomic_read(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].cdev_state)){
        DBGLOG(CHAR,ERR,"cdev_open: %s(%d) multi-open fail!", \
			eemcs_cdev_inst.cdev_node[PORT2IDX(id)].cdev_name, id);
        return -EIO;
    }

	if(eemcs_cdev_inst.cdev_node[PORT2IDX(id)].ccci_ch.rx != CH_DUMMY){ /* CH_DUMMY should not have Rx Data */
		#if 0
		struct sk_buff *rx_skb;
	    //4 <2>  clear the rx_skb_list
		skb_queue_purge(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].rx_skb_list);
		while ((rx_skb = skb_dequeue(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].rx_skb_list)) != NULL) {
			dev_kfree_skb(rx_skb);	
        	eemcs_ccci_release_rx_skb(id, 1, rx_skb);
		}
		atomic_set(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].rx_pkt_cnt, 0);
		#endif
		
		//4 <3>  register ccci channel 
		ret = ccci_cdev_register(eemcs_cdev_inst.cdev_node[PORT2IDX(id)].ccci_ch.rx, eemcs_cdev_rx_callback, 0);
		if(ret != KAL_SUCCESS){
			DBGLOG(CHAR,ERR,"PORT%d register cdev fail!!", id);
			return -EIO;
		}
    }

    file->private_data = &eemcs_cdev_inst.cdev_node[PORT2IDX(id)];
    nonseekable_open(inode, file);
    atomic_set(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].cdev_state, CDEV_OPEN);

    if ( CCCI_PORT_META == id ) {
        md_log_node = file->private_data;
        DBGLOG(CHAR,INF, "init md log node");
        catch_last_log = 0;
    }

    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}

bool eemcs_cdev_rst_port_closed(void){
    /* reset must wait for fs/muxd/rild to be terminated */
    if(CDEV_OPEN == atomic_read(&eemcs_cdev_inst.cdev_node[PORT2IDX(CCCI_PORT_FS)].cdev_state)){
        DBGLOG(CHAR,DEF,"FS port close fail!");
        return false;
    }
    if(CDEV_OPEN == atomic_read(&eemcs_cdev_inst.cdev_node[PORT2IDX(CCCI_PORT_MUX)].cdev_state)){
         DBGLOG(CHAR,DEF,"MUX port close fail!");
         return false;
    }
//	    if(CDEV_OPEN == atomic_read(&eemcs_cdev_inst.cdev_node[PORT2IDX(CCCI_PORT_MD_LOG)].cdev_state)){
//	        DBGLOG(CHAR,DEF,"MDLOG port close fail!");
//	        return false;
//	    }
    return true;
}

static int eemcs_cdev_release(struct inode *inode, struct file *file)
{    
    int id = iminor(inode);
    struct sk_buff *rx_skb;
    struct sk_buff_head *list;
    unsigned long flags;
	
    DEBUG_LOG_FUNCTION_ENTRY;    	
    DBGLOG(CHAR,INF,"cdev_release: close dev(%s, %d)",\
        eemcs_cdev_inst.cdev_node[PORT2IDX(id)].cdev_name, id);

    atomic_set(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].cdev_state, CDEV_CLOSE);
    /*
    while ((rx_skb = skb_dequeue(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].rx_skb_list)) != NULL) {
        dev_kfree_skb(rx_skb);	
        eemcs_ccci_release_rx_skb(id, 1, rx_skb);
    }
    */
    list = &eemcs_cdev_inst.cdev_node[PORT2IDX(id)].rx_skb_list;
	
    spin_lock_irqsave(&list->lock, flags);
    while ((rx_skb = skb_peek(list)) != NULL)
    {
        __skb_unlink(rx_skb, list);
        dev_kfree_skb(rx_skb);
        eemcs_ccci_release_rx_skb(id, 1, rx_skb);
    }
    spin_unlock_irqrestore(&list->lock, flags);

    atomic_set(&eemcs_cdev_inst.cdev_node[PORT2IDX(id)].rx_pkt_cnt, 0);

    DBGLOG(CHAR,INF,"cdev_release: free pending skb done.");


    if( CCCI_PORT_META == id ) {
        spin_lock_irqsave(&md_logger_lock, flags);
        md_log_node = NULL;
        spin_unlock_irqrestore(&md_logger_lock, flags);
    }

    /* unregister ccci channel */ // Might casue raise condition while user close cdev
    //ccci_cdev_unregister(eemcs_cdev_inst.cdev_node[PORT2IDX(id)].ccci_ch.rx);

    if(true == eemcs_on_reset())
    {
        if(true == eemcs_cdev_rst_port_closed()){
            eemcs_boot_user_exit_notify();
        }
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return 0;
}

void eemcs_cdev_reset(void)
{
	int i = 0;
	struct sk_buff *rx_skb;
	
	for(i = 0; i < EEMCS_CDEV_MAX_NUM; i++) {		
		// clear the rx_skb_list
		while ((rx_skb = skb_dequeue(&eemcs_cdev_inst.cdev_node[i].rx_skb_list)) != NULL) {
			dev_kfree_skb(rx_skb);	
			eemcs_ccci_release_rx_skb(IDX2PORT(i), 1, rx_skb);
		}		
		atomic_set(&eemcs_cdev_inst.cdev_node[i].rx_pkt_cnt, 0);
		atomic_set(&eemcs_cdev_inst.cdev_node[i].tx_pkt_cnt, 0);
		atomic_set(&eemcs_cdev_inst.cdev_node[i].rx_pkt_drop_cnt, 0);		
	}	
	return;
}

int eemcs_cdev_msg(int port_id, unsigned int message, unsigned int reserved){
    struct sk_buff *new_skb;
    CCCI_BUFF_T    *pccci_h;
    ccci_port_cfg  *port_cfg;

    DEBUG_LOG_FUNCTION_ENTRY;
    new_skb = ccci_cdev_mem_alloc(sizeof(CCCI_BUFF_T), GFP_ATOMIC);
    if(new_skb == NULL){
        DBGLOG(CHAR, INF, "[TX]PORT%d alloc skb fail when send msg and wait", port_id);
        new_skb = ccci_cdev_mem_alloc(sizeof(CCCI_BUFF_T), GFP_KERNEL);
        if (NULL == new_skb) {
            DBGLOG(CHAR, ERR, "[TX]PORT%d retry to alloc skb fail and exit", port_id);
            DEBUG_LOG_FUNCTION_LEAVE;
            return -ENOMEM;
        }
    }        
    pccci_h = (CCCI_BUFF_T *)new_skb->data;
    memset(pccci_h, 0, sizeof(CCCI_BUFF_T));
    port_cfg = ccci_get_port_info(port_id);
    pccci_h->data[0]  = CCCI_MAGIC_NUM;
    pccci_h->data[1]  = message;
    pccci_h->channel  = port_cfg->ch.rx;
    pccci_h->reserved = reserved;

    DBGLOG(CHAR, DBG, "%s(%d) send cdev_msg: 0x%08X, 0x%08X, %02d, 0x%08X", ccci_cdev_name[PORT2IDX(port_id)],\
            port_id, pccci_h->data[0], pccci_h->data[1], pccci_h->channel, pccci_h->reserved);

    if(port_id == CCCI_PORT_CTRL){
        return eemcs_boot_rx_callback(new_skb, 0);
    }else{
        return eemcs_cdev_rx_callback(new_skb, 0);
    }
}
    
static void eemcs_cdev_write_force_md_rst(void)
{
	struct sk_buff *new_skb;
	CCCI_BUFF_T *ccci_header;
	int ret;
	
	new_skb = ccci_cdev_mem_alloc(CCCI_CDEV_HEADER_ROOM, GFP_ATOMIC);
	while(NULL == new_skb)
	{
		new_skb = ccci_cdev_mem_alloc(CCCI_CDEV_HEADER_ROOM, GFP_ATOMIC);
	}
	/* reserve SDIO_H header room */
	#ifdef CCCI_SDIO_HEAD
	skb_reserve(new_skb, sizeof(SDIO_H)); 
	#endif
	
	ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ; 
	ccci_header->data[0]= CCCI_MAGIC_NUM; /* message box magic */
	ccci_header->data[1]= 0;              /* message ID */
	ccci_header->channel = CCCI_FORCE_RESET_MODEM_CHANNEL;      /* reset channel number */
	ret = ccci_cdev_write_desc_to_q(CCCI_FORCE_RESET_MODEM_CHANNEL, new_skb);
}

void eemcs_md_logger_notify(void)
{
    unsigned long flags;
    spin_lock_irqsave(&md_logger_lock, flags);
    if (md_log_node ) {
        wake_up_interruptible(&md_log_node->rx_waitq);
        catch_last_log = 1;
        wake_lock_timeout(&eemcs_wake_lock, HZ); // Using 0.5s wake lock
    }
    spin_unlock_irqrestore(&md_logger_lock, flags);
    DBGLOG(CHAR,INF, "notify md logger");
}

extern void apply_pre_md_run_setting(void);
extern void apply_post_md_run_setting(void);

static long eemcs_cdev_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    KAL_UINT8 port_id = curr_node->eemcs_port_id; /* port_id */
    int ret = 0;
    unsigned int sim_type;
    unsigned int enable_sim_type;

    DEBUG_LOG_FUNCTION_ENTRY;
    if(port_id > END_OF_CCCI_CDEV)
    {
        DBGLOG(CHAR, ERR, "ccci ioctl fail: unknown Port id=%d", port_id);
        ret = -ENOTTY;
        goto _exit;                    
    }

    switch(cmd)
    {
        case CCCI_IOC_GET_MD_STATE:
        {
            KAL_UINT32  eemcs_state, md_state;

            eemcs_state = check_device_state();
            if(eemcs_state == EEMCS_BOOTING_DONE ){
				md_state = MD_STATE_READY;
            }else if(eemcs_state == EEMCS_EXCEPTION){
				md_state = MD_STATE_EXPT;
            }else if(eemcs_state <= EEMCS_INIT){
				md_state = MD_STATE_INVALID;
            }else{
				 md_state = MD_STATE_INIT;
            }
            ret = put_user((unsigned int)md_state, (unsigned int __user *)arg);
            DBGLOG(CHAR, DBG, "CCCI_IOC_GET_MD_STATE(md_s=%d, eemcs_s=%d) by %s(%d)", md_state, eemcs_state, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
        }
        break;

        case CCCI_IOC_SET_EXCEPTION_DATA:
        {
            DBGLOG(CHAR, ERR, "CCCI_IOC_SET_EXCEPTION_DATA by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id);
#if 0
            extern EX_LOG_T md_ex_log;
            void __user *argp = (void __user *)arg;
            if(copy_from_user(&md_ex_log,argp,MD_EX_LOG_SIZE))
            {
                 DBGLOG(PORT,ERR,"copy_from_user failed.");
                 return -EFAULT;
            }
            md_exception(&md_ex_log);
#endif		
        }
        break;

        case CCCI_IOC_SET_HEADER:
        {
            KAL_UINT32 ori_port_flag = 0;
            KAL_UINT32 new_port_flag = 0;
            //port->control_flag |=PORT_EXPORT_CCIF_BUFFER;
            ori_port_flag = ccci_get_port_cflag(port_id);
            ccci_set_port_type(port_id, (ori_port_flag|EXPORT_CCCI_H));
            new_port_flag = ccci_get_port_cflag(port_id);
            DBGLOG(CHAR, DBG, "CCCI_IOC_SET_HEADER(%d, %d) by %s(%d)", ori_port_flag, new_port_flag,\
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
        }
        break;

        case CCCI_IOC_CLR_HEADER:
        {
            //port->control_flag &=(~PORT_EXPORT_CCIF_BUFFER);
            KAL_UINT32 ori_port_flag = 0;
            KAL_UINT32 new_port_flag = 0;

            ori_port_flag = ccci_get_port_cflag(port_id);
            ccci_set_port_type(port_id, (ori_port_flag&(~EXPORT_CCCI_H)));
            new_port_flag = ccci_get_port_cflag(port_id);
            DBGLOG(CHAR, DBG, "CCCI_IOC_CLR_HEADER(%d, %d) by %s(%d)", ori_port_flag, new_port_flag, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
            
        }
        break;

        /* This ioctl will be issued from RILD */
        case CCCI_IOC_ENTER_DEEP_FLIGHT:
        case CCCI_IOC_SEND_STOP_MD_REQUEST: 
        {
            DBGLOG(CHAR, INF, "IOTCL CCCI_IOC_ENTER_DEEP_FLIGHT by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id); 
            change_device_state(EEMCS_GATE);
            eemcs_power_off_md(0, 0);
            apply_post_md_run_setting();
            /* mtlte_sys_sdio_remove */
            eemcs_cdev_msg(CCCI_PORT_CTRL, CCCI_MD_MSG_ENTER_FLIGHT_MODE, 0);
           
        }
        break;

        case CCCI_IOC_LEAVE_DEEP_FLIGHT:
        case CCCI_IOC_SEND_START_MD_REQUEST:
        {
            DBGLOG(CHAR, INF, "CCCI_IOC_LEAVE_DEEP_FLIGHT by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id); 
            eemcs_cdev_msg(CCCI_PORT_CTRL, CCCI_MD_MSG_LEAVE_FLIGHT_MODE, 0);
            apply_pre_md_run_setting();
        }
        break;

        case CCCI_IOC_FORCE_MD_ASSERT:
        {
            DBGLOG(CHAR, INF, "CCCI_IOC_FORCE_MD_ASSERT by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id); 
            /* force md assert channel is 20090215 */
            eemcs_cdev_write_force_md_rst();
            //CCCI_INIT_MAILBOX(&buff, 0);
            //ret = ccci_write_force(CCCI_FORCE_RESET_MODEM_CHANNEL, &buff);
        }
        break;

        case CCCI_IOC_MD_RESET:
        {
            DBGLOG(CHAR, INF, "CCCI_IOC_MD_RESET by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id); 
            eemcs_md_reset();
        }
        break;

        case CCCI_IOC_CHECK_STATE:
        {
            KAL_UINT32  state;
            state = check_device_state();
            DBGLOG(CHAR, INF, "CCCI_IOC_CHECK_STATE(%d) by %s(%d)", state, ccci_cdev_name[PORT2IDX(port_id)], port_id); 
            ret = put_user((unsigned int)state, (unsigned int __user *)arg);
        }
        break;
         
#ifdef IT_TESTING_PURPOSE   
        case CCCI_IOC_PURGE_SKBQ:
        {
            struct sk_buff *skb;
            while ((skb = skb_dequeue(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].rx_skb_list)) != NULL) {
				dev_kfree_skb(skb);	
				eemcs_ccci_release_rx_skb(port_id, 1, skb);
            }
            atomic_set(&eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].rx_pkt_cnt, 0);
            DBGLOG(CHAR, INF, "CCCI_IOC_PURGE_SKBQ by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id);         
        }
        break;
#endif

        case CCCI_IOC_GET_EXT_MD_POST_FIX:
        {
            eemcs_boot_get_ext_md_post_fix((char*) arg);
            DBGLOG(CHAR, INF, "CCCI_IOC_GET_MD_POSTFIX(%s) by %s(%d)", (char*)arg, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
        }
        break;

        case CCCI_IOC_SET_BOOT_STATE:
        {
            KAL_UINT32  state = 0;
            get_user(state, (unsigned int __user *)arg);
            state = eemcs_boot_reset_test(state);
            DBGLOG(CHAR, INF, "CCCI_IOC_SET_BOOT_STATE(%d) by %s(%d)", state, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
        }
        break;

        case CCCI_IOC_GET_BOOT_STATE:
        {
            KAL_UINT32  state = 0;
            
            state = eemcs_boot_get_state();
            ret = put_user((unsigned int)state, (unsigned int __user *)arg);
            DBGLOG(CHAR, INF, "CCCI_IOC_GET_BOOT_STATE(%d) by %s(%d)", state, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
        }
        break;
       
        case CCCI_IOC_GET_MD_IMG_EXIST:
        {
            unsigned int *md_img_exist_list = eemcs_get_md_img_exist_list();
            DBGLOG(CHAR, INF,"CCCI_IOC_GET_MD_IMG_EXIST by %s(%d)", ccci_cdev_name[PORT2IDX(port_id)], port_id);
            if (copy_to_user((void __user *)arg, md_img_exist_list,(unsigned int)eemcs_get_md_img_exist_list_size())) {
				DBGLOG(CHAR, ERR, "CCCI_IOC_GET_MD_IMG_EXIST: copy_to_user fail");
				ret= -EFAULT;
            }
        }
        break;
            
        case CCCI_IOC_GET_MD_TYPE:
        {
            int md_type = get_ext_modem_support(eemcs_get_md_id());
            DBGLOG(CHAR, INF, "CCCI_IOC_GET_MD_TYPE(%d) by %s(%d)", md_type, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
            ret = put_user((unsigned int)md_type, (unsigned int __user *)arg);
        }
        break;
            
        case CCCI_IOC_RELOAD_MD_TYPE:
        {
            int md_type = 0;
    		if(copy_from_user(&md_type, (void __user *)arg, sizeof(unsigned int))) {
				DBGLOG(CHAR, ERR, "CCCI_IOC_RELOAD_MD_TYPE: copy_from_user fail!");
				ret = -EFAULT;
                break;
			} 
            
            if (md_type > modem_invalid && md_type < modem_max_type){
                DBGLOG(CHAR, INF, "CCCI_IOC_RELOAD_MD_TYPE(%d) by %s(%d)", md_type, \
					ccci_cdev_name[PORT2IDX(port_id)], port_id);
			    ret = set_ext_modem_support(eemcs_get_md_id(), md_type);
            }
            else{
                DBGLOG(CHAR, ERR, "CCCI_IOC_RELOAD_MD_TYPE fail: invalid md type(%d)", md_type);
                ret = -EFAULT;
            }
            eemcs_set_reload_image(true);
	    }
        break;

        case CCCI_IOC_STORE_MD_TYPE:
        {
            unsigned int md_type_saving = 0;
			//DBGLOG(CHAR, INF, "IOC_STORE_MD_TYPE ioctl by %s!",  current->comm);
			if(copy_from_user(&md_type_saving, (void __user *)arg, sizeof(unsigned int))) {
				DBGLOG(CHAR, ERR, "CCCI_IOC_STORE_MD_TYPE: copy_from_user fail!");
				ret = -EFAULT;
                break;
			}
			
			DBGLOG(CHAR, DBG, "CCCI_IOC_STORE_MD_TYPE(%d) by %s(%s,%d)", md_type_saving, current->comm,\
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
			if (md_type_saving > modem_invalid && md_type_saving < modem_max_type){
				if (md_type_saving != get_ext_modem_support(eemcs_get_md_id())){
					DBGLOG(CHAR, INF, "CCCI_IOC_STORE_MD_TYPE(%d->%d)", md_type_saving, get_ext_modem_support(eemcs_get_md_id()));
				}
				//Notify md_init daemon to store md type in nvram
				eemcs_cdev_msg(CCCI_PORT_CTRL, CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, md_type_saving);
			}
			else {
				DBGLOG(CHAR, ERR, "CCCI_IOC_STORE_MD_TYPE fail: invalid md type(%d)", md_type_saving);
                ret = -EFAULT;
			}
			
	    }
		break;
           
        case CCCI_IOC_GET_MD_EX_TYPE:
        {
            int md_expt_type = get_md_expt_type();
            DBGLOG(CHAR, INF, "CCCI_IOC_GET_MD_EX_TYPE(%d) by %s(%d)", md_expt_type, \
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
			ret = put_user((unsigned int)md_expt_type, (unsigned int __user *)arg);
        }
        break;
		
        case CCCI_IOC_DL_TRAFFIC_CONTROL:
        {
            unsigned int traffic_control = 0;
            
            if(copy_from_user(&traffic_control, (void __user *)arg, sizeof(unsigned int))) {
                DBGLOG(CHAR, ERR, "CCCI_IOC_DL_TRAFFIC_CONTROL: copy_from_user fail!");
                ret = -EFAULT;
                break;
            }
			
            DBGLOG(CHAR, INF, "CCCI_IOC_DL_TRAFFIC_CONTROL(%d) by %s(%d)", traffic_control,\
				ccci_cdev_name[PORT2IDX(port_id)], port_id);
            if(traffic_control == 1)
            {
                ccci_cdev_turn_on_dl_q(port_id);
            }
            else if(traffic_control == 0)
            {
                ccci_cdev_turn_off_dl_q(port_id);
            }
            else 
            {
                DBGLOG(CHAR, ERR, "CCCI_IOC_DL_TRAFFIC_CONTROL fail: Unknown value(0x%x)", traffic_control);
                ret = -EFAULT;
            }
	    }
        break;    

		case CCCI_IOC_GET_SIM_TYPE: 		//for regional phone boot animation
		{
			get_sim_type(eemcs_get_md_id(), &sim_type);
			ret = put_user((unsigned int)sim_type, (unsigned int __user *)arg);
		}
		break;
		
		case CCCI_IOC_ENABLE_GET_SIM_TYPE:	//for regional phone boot animation
		{
			if(copy_from_user(&enable_sim_type, (void __user *)arg, sizeof(unsigned int))) {
				DBGLOG(CHAR, ERR, "CCCI_IOC_ENABLE_GET_SIM_TYPE: copy_from_user fail!\n");
				ret = -EFAULT;
			} else {
				enable_get_sim_type(eemcs_get_md_id(), enable_sim_type);
			}
		}
		break;          

		case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
		{
			char md_protol[] = "DHL";
			KAL_UINT32 data_size = sizeof(md_protol) / sizeof(char);

			DBGLOG(CHAR, ERR, "Call CCCI_IOC_GET_MD_PROTOCOL_TYPE!\n");

            if (copy_to_user((void __user *)arg, md_protol, data_size)) 
			{
                DBGLOG(CHAR, ERR, "copy_to_user MD_PROTOCOL failed !!");
                return -EFAULT;
			}
			
			break;
		}

        default:
            DBGLOG(CHAR, ERR, "Unknown ioctl(0x%x) by %s(%d)", cmd, ccci_cdev_name[PORT2IDX(port_id)], port_id);
            ret = -EFAULT;
        	break;
    }

_exit:
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

static ssize_t eemcs_cdev_write(struct file *fp, const char __user *buf, size_t in_sz, loff_t *ppos)
{
    ssize_t ret   = -EINVAL;
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    KAL_UINT8 port_id = curr_node->eemcs_port_id; /* port_id */
    KAL_UINT32 p_type, control_flag;    
    struct sk_buff *new_skb;
    CCCI_BUFF_T *ccci_header;
    size_t count = in_sz;
    size_t skb_alloc_size;
    KAL_UINT32 alloc_time = 0, curr_time = 0;
	
    DEBUG_LOG_FUNCTION_ENTRY;        
    DBGLOG(CHAR, DBG, "eemcs_cdev_write: %s(%d), len=%d",curr_node->cdev_name,port_id,count);

    p_type = ccci_get_port_type(port_id);
	if(curr_node->ccci_ch.tx == CH_DUMMY){
		/* if ccci channel is assigned to CH_DUMMY means tx packets should be dropped ex. muxreport port */
        DBGLOG(CHAR, ERR, "PORT%d is assigned to CH_DUMMY ccci channel !!PKT DROP!!", port_id);
        ret = -EINVAL;
        goto _exit;                    
	}

    if(p_type != EX_T_USER) 
    {
        DBGLOG(CHAR, ERR, "PORT%d refuse p_type(%d) access user port", port_id, p_type);
        ret = -EINVAL;
        goto _exit;                    
    }

    control_flag = ccci_get_port_cflag(port_id);	
    if (check_device_state() == EEMCS_EXCEPTION) {//modem exception		
        if ((control_flag & TX_PRVLG2) == 0) {
            DBGLOG(CHAR, ERR, "[TX]PORT%d write fail when modem exception", port_id);
            return -ETXTBSY;
        }
    } else if (check_device_state() != EEMCS_BOOTING_DONE) {//modem not ready
        if ((control_flag & TX_PRVLG1) == 0) {
            DBGLOG(CHAR, ERR, "[TX]PORT%d write fail when modem not ready", port_id);
            return -ENODEV;
        }
    }
	
    if((control_flag & EXPORT_CCCI_H) && (count < sizeof(CCCI_BUFF_T)))
    {
        DBGLOG(CHAR, WAR, "[TX]PORT%d invalid wirte len(%d)", port_id, count);
        ret = -EINVAL;
        goto _exit;            
    }
	
    if(port_id == CCCI_PORT_FS) {
        skb_alloc_size = count - sizeof(CCCI_BUFF_T);
    } else {
        if(control_flag & EXPORT_CCCI_H){		
	        if(count > (MAX_TX_BYTE+sizeof(CCCI_BUFF_T))){
	            DBGLOG(CHAR, ERR, "[TX]PORT%d wirte_len(%d) > MTU(%d)!", port_id, count, MAX_TX_BYTE);
	            count = MAX_TX_BYTE+sizeof(CCCI_BUFF_T);
	        }
	        skb_alloc_size = count - sizeof(CCCI_BUFF_T);
        }else{
	        if(count > MAX_TX_BYTE){
	            DBGLOG(CHAR, WAR, "[TX]PORT%d wirte_len(%d) > MTU(%d)!", port_id, count, MAX_TX_BYTE);
	            count = MAX_TX_BYTE;
	        }
	        skb_alloc_size = count;
        }
    }	
__blocking_IO:
    if (ccci_cdev_write_space_alloc(curr_node->ccci_ch.tx)==0){
        if (fp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            DBGLOG(CHAR, WAR, "[TX]PORT%d ccci_cdev_write_space_alloc return 0)", port_id);
            goto _exit;
        }else{ // Blocking IO
            DBGLOG(CHAR, DBG, "[TX]PORT%d Enter Blocking I/O wait", port_id);
            ret = ccci_cdev_write_wait(curr_node->ccci_ch.tx);
        	if(ret == -ERESTARTSYS) {
				DBGLOG(CHAR, WAR, "[TX]PORT%d Interrupted,return ERESTARTSYS", port_id);
				ret = -EINTR;
				goto _exit;
			}
            goto __blocking_IO;
        }
    }	
    
    new_skb = ccci_cdev_mem_alloc(skb_alloc_size + CCCI_CDEV_HEADER_ROOM, GFP_ATOMIC);
    if(NULL == new_skb)
    {
    	DBGLOG(CHAR, INF, "[TX]PORT%d alloc skb with wait", port_id);
    	alloc_time = jiffies;
    	new_skb = ccci_cdev_mem_alloc(skb_alloc_size + CCCI_CDEV_HEADER_ROOM, GFP_KERNEL);
    	if (NULL == new_skb) {
            ret = -ENOMEM;
            DBGLOG(CHAR, ERR, "[TX]PORT%d alloc skb with wait fail", port_id);
            goto _exit; 
    	}
		
    	curr_time = jiffies;
    	if ((curr_time - alloc_time) >= 1) {
            DBGLOG(CHAR, ERR, "[TX]PORT%d alloc skb with wait delay: time=%dms", port_id,
			10*(curr_time - alloc_time));
    	}
    }
    
    /* reserve SDIO_H header room */
    #ifdef CCCI_SDIO_HEAD
    skb_reserve(new_skb, sizeof(SDIO_H));
    #endif
	
    if(control_flag & EXPORT_CCCI_H){
        ccci_header = (CCCI_BUFF_T *)new_skb->data;
    }else{
        ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ;
    }

    if(copy_from_user(skb_put(new_skb, count), buf, count))
    {
        DBGLOG(CHAR, ERR, "[TX]PORT%d copy_from_user(len=%d, %p->%p) fail", port_id, \
			count, buf, new_skb->data);
        dev_kfree_skb(new_skb);
        ret=-EFAULT;
        goto _exit;
    }

    if(control_flag & EXPORT_CCCI_H)
    {
        /* user bring down the ccci header */
        if(count == sizeof(CCCI_BUFF_T)){
            DBGLOG(CHAR, TRA, "[TX]PORT%d, CCCI_MSG(0x%08X, 0x%08X, %02d, 0x%08X)", 
                    port_id, 
                    ccci_header->data[0], ccci_header->data[1],
                    ccci_header->channel, ccci_header->reserved);

            ccci_header->data[0]= CCCI_MAGIC_NUM;
        }else{
            ccci_header->data[1]= count; 
        }

        if(ccci_header->channel != curr_node->ccci_ch.tx){
            DBGLOG(CHAR, WAR, "[TX]PORT%d CCCI ch mis-match (%d) vs (%d)!! will correct by char_dev",\
				port_id, ccci_header->channel, curr_node->ccci_ch.tx);
        }
    }
    else
    {
        /* user bring down the payload only */
        ccci_header->data[1]    = count + sizeof(CCCI_BUFF_T);
        ccci_header->reserved   = 0;
    }
    ccci_header->channel = curr_node->ccci_ch.tx;

    DBGLOG(CHAR, DBG, "[TX]PORT%d, CCCI_MSG(0x%08X, 0x%08X, %02d, 0x%08X)", 
                    port_id, 
                    ccci_header->data[0], ccci_header->data[1],
                    ccci_header->channel, ccci_header->reserved);
	
    if ((ccci_header->channel == CH_FS_TX) && (ccci_header->reserved > 0x4)) {
        char *pdata = (char *)new_skb->data;
        DBGLOG(CHAR, INF, "[FS]eemcs_cdev_write\n\
		[00..07] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[08..15] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[16..23] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[24..31] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",\
		(int)*(pdata+0),(int)*(pdata+1),(int)*(pdata+2),(int)*(pdata+3),(int)*(pdata+4),(int)*(pdata+5),(int)*(pdata+6),(int)*(pdata+7),\
		(int)*(pdata+8),(int)*(pdata+9),(int)*(pdata+10),(int)*(pdata+11),(int)*(pdata+12),(int)*(pdata+13),(int)*(pdata+14),(int)*(pdata+15),\
		(int)*(pdata+16),(int)*(pdata+17),(int)*(pdata+18),(int)*(pdata+19),(int)*(pdata+20),(int)*(pdata+21),(int)*(pdata+22),(int)*(pdata+23),\
		(int)*(pdata+24),(int)*(pdata+25),(int)*(pdata+26),(int)*(pdata+27),(int)*(pdata+28),(int)*(pdata+29),(int)*(pdata+30),(int)*(pdata+31));
    }

    {
        char *ptr = (char *)new_skb->data;
        ptr+=sizeof(CCCI_BUFF_T);
        /* dump 32 byte of the !!!CCCI DATA!!! part */

		CDEV_LOG(port_id, CHAR, INF, "[DUMP]PORT%d eemcs_cdev_write\n\
		[00..07] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[08..15] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[16..23] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[24..31] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",\
		port_id,\
		(int)*(ptr+0),(int)*(ptr+1),(int)*(ptr+2),(int)*(ptr+3),(int)*(ptr+4),(int)*(ptr+5),(int)*(ptr+6),(int)*(ptr+7),\
		(int)*(ptr+8),(int)*(ptr+9),(int)*(ptr+10),(int)*(ptr+11),(int)*(ptr+12),(int)*(ptr+13),(int)*(ptr+14),(int)*(ptr+15),\
		(int)*(ptr+16),(int)*(ptr+17),(int)*(ptr+18),(int)*(ptr+19),(int)*(ptr+20),(int)*(ptr+21),(int)*(ptr+22),(int)*(ptr+23),\
		(int)*(ptr+24),(int)*(ptr+25),(int)*(ptr+26),(int)*(ptr+27),(int)*(ptr+28),(int)*(ptr+29),(int)*(ptr+30),(int)*(ptr+31));

    }
  
    ret = ccci_cdev_write_desc_to_q(curr_node->ccci_ch.tx, new_skb);

	if (KAL_SUCCESS != ret) {
		DBGLOG(CHAR, ERR, "[TX]PORT%d PKT DROP of ch%d!", port_id, curr_node->ccci_ch.tx);
		dev_kfree_skb(new_skb);
        ret = -EAGAIN;
	} else {
        atomic_inc(&curr_node->tx_pkt_cnt);
        //wake_up(&curr_node->tx_waitq); /* wake up tx_waitq for notify poll_wait of state change */
	}

#if 0    
    20130102 note that 
    ret = que_wakeup_transfer(port->txq_id);
    if(ret)
    {
        DBGLOG(PORT,ERR,"PORT(%d) fail wake when write(%d)", port->id, ret);
        goto _exit;
    }   
    ret = wait_event_interruptible(port->write_waitq, port->tx_pkt_id == port->tx_pkt_id_done);
    if(ret == -ERESTARTSYS)
    {
      // TODO: error handling .....
      DBGLOG(PORT,ERR,"PORT(%d) fail wait write done event successfully", port->id);
    }
#endif
_exit:
    DEBUG_LOG_FUNCTION_LEAVE;
    if(!ret){
        return count;
    }

    ccci_cdev_write_space_release(curr_node->ccci_ch.tx);
    return ret;
}


static ssize_t eemcs_cdev_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
    unsigned int flag;
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    KAL_UINT8 port_id = curr_node->eemcs_port_id; /* port_id */
    KAL_UINT32 p_type, rx_pkt_cnt, read_len, rx_pkt_cnt_int;
    struct sk_buff *rx_skb;
    unsigned char *payload=NULL;
    CCCI_BUFF_T *ccci_header;
    int ret = 0;

    DEBUG_LOG_FUNCTION_ENTRY;
    
    flag=fp->f_flags;
    //verbose DBGLOG(CHAR,DBG,"read deivce iminor (0x%x),length(0x%x)",port_id,count);

    p_type = ccci_get_port_type(port_id);
    if(p_type != EX_T_USER) 
    {
        DBGLOG(CHAR, ERR, "[RX]PORT%d refuse access user port: type=%d", port_id, p_type);
        goto _exit;                    
    }

    rx_pkt_cnt_int = atomic_read(&curr_node->buff.remaining_rx_cnt);
    KAL_ASSERT(rx_pkt_cnt_int >= 0);
    if(rx_pkt_cnt_int == 1)
    {
        DBGLOG(CHAR, DBG, "[RX]Streaming reading!! PORT%d len=%d\n",port_id,count);
        rx_skb = curr_node->buff.remaining_rx_skb;
        /* rx_skb shall not be null */
        KAL_ASSERT(NULL != rx_skb);
        read_len = curr_node->buff.remaining_len;
        KAL_ASSERT(read_len >= 0);
    }
    else    
    {
        rx_pkt_cnt = atomic_read(&curr_node->rx_pkt_cnt);
        KAL_ASSERT(rx_pkt_cnt >= 0);
        
        if(rx_pkt_cnt == 0) {
            if (flag&O_NONBLOCK)
            {	
                ret=-EAGAIN;
                //verbose DBGLOG(CHAR,DBG,"[CHAR] PORT(%d) eemcs_cdev_read return O_NONBLOCK for NON-BLOCKING",port_id);
                goto _exit;
            }
            ret = wait_event_interruptible(curr_node->rx_waitq, atomic_read(&curr_node->rx_pkt_cnt) > 0);
            if(ret)
            {
                ret = -EINTR;
                DBGLOG(CHAR, ERR, "[RX]PORT%d Interrupted by syscall.signal=%lld", port_id, \
					*(long long *)current->pending.signal.sig);	
                goto _exit;
            }
        }
       
        /* Cached memory from last read fail*/
        DBGLOG(CHAR, TRA, "[RX]dequeue from rx_skb_list, rx_pkt_cnt(%d)", rx_pkt_cnt); 
        rx_skb = skb_dequeue(&curr_node->rx_skb_list);
		
        /* There should be rx_skb in the list */
        if (rx_skb == NULL) {
            DBGLOG(CHAR, ERR, "[RX]dequeue from rx_skb_list, rx_pkt_cnt(%d, qlen=%d)", \
				atomic_read(&curr_node->rx_pkt_cnt), curr_node->rx_skb_list.qlen); 
            KAL_ASSERT(NULL != rx_skb);
        }
        atomic_dec(&curr_node->rx_pkt_cnt);
        rx_pkt_cnt = atomic_read(&curr_node->rx_pkt_cnt);
        KAL_ASSERT(rx_pkt_cnt >= 0);

        ccci_header = (CCCI_BUFF_T *)rx_skb->data;

        DBGLOG(CHAR, DBG, "[RX]PORT%d CCCI_H(0x%08X, 0x%08X, %02d, 0x%08X)",\
                port_id, ccci_header->data[0],ccci_header->data[1],
                ccci_header->channel, ccci_header->reserved);
        
        /*If not match please debug EEMCS CCCI demux skb part*/
        if(ccci_header->channel != curr_node->ccci_ch.rx) {
            DBGLOG(CHAR,ERR,"Assert(ccci_header->channel == curr_node->ccci_ch.rx)");
            DBGLOG(CHAR,ERR,"ccci_header->channel:%d, curr_node->ccci_ch.rx:%d, curr_node->eemcs_port_id:%d", 
				ccci_header->channel, curr_node->ccci_ch.rx, curr_node->eemcs_port_id);
            KAL_ASSERT(ccci_header->channel == curr_node->ccci_ch.rx);
        }
        //KAL_ASSERT(ccci_header->channel == curr_node->ccci_ch.rx);
        
        if(!(ccci_get_port_cflag(port_id) & EXPORT_CCCI_H))
        {
            read_len = ccci_header->data[1] - sizeof(CCCI_BUFF_T);
            /* remove CCCI_HEADER */
            skb_pull(rx_skb, sizeof(CCCI_BUFF_T));
        }else{
            if(ccci_header->data[0] == CCCI_MAGIC_NUM){
                read_len = sizeof(CCCI_BUFF_T); 
            }else{
                read_len = ccci_header->data[1];
            }
        }
    }
    
    DBGLOG(CHAR, TRA, "[RX]PORT%d read_len=%d",port_id, read_len);

/* 20130816 ian add aud dump */
    {
	    char *ptr = (char *)rx_skb->data;
	    /* dump 32 byte of the !!!CCCI DATA!!! part */
		CDEV_LOG(port_id, CHAR, ERR,"[DUMP]PORT%d eemcs_cdev_read\n\
		[00..07] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[08..15] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[16..23] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n\
		[24..31] 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",\
		port_id,\
		(int)*(ptr+0),(int)*(ptr+1),(int)*(ptr+2),(int)*(ptr+3),(int)*(ptr+4),(int)*(ptr+5),(int)*(ptr+6),(int)*(ptr+7),\
		(int)*(ptr+8),(int)*(ptr+9),(int)*(ptr+10),(int)*(ptr+11),(int)*(ptr+12),(int)*(ptr+13),(int)*(ptr+14),(int)*(ptr+15),\
		(int)*(ptr+16),(int)*(ptr+17),(int)*(ptr+18),(int)*(ptr+19),(int)*(ptr+20),(int)*(ptr+21),(int)*(ptr+22),(int)*(ptr+23),\
		(int)*(ptr+24),(int)*(ptr+25),(int)*(ptr+26),(int)*(ptr+27),(int)*(ptr+28),(int)*(ptr+29),(int)*(ptr+30),(int)*(ptr+31));

    }

    payload=(unsigned char*)rx_skb->data;
    if(count < read_len)
    {
        /* Means 1st streaming reading*/
        if(rx_pkt_cnt_int == 0)
        {
            atomic_inc(&curr_node->buff.remaining_rx_cnt);
            curr_node->buff.remaining_rx_skb = rx_skb;
        }
        
        DBGLOG(CHAR, TRA, "[RX]PORT%d !!! USER BUFF(%d) less than DATA SIZE(%d) !!!", port_id, count, read_len);
        DBGLOG(CHAR, TRA, "[RX]copy_to_user: %p -> %p, len=%d", payload, buf, count);
        ret = copy_to_user(buf, payload, count);
        if(ret == 0)
        {   
            curr_node->buff.remaining_len = read_len - count;
            skb_pull(rx_skb, count);  //move data pointer
            //update actually read length
            read_len = count;
        }
        else
        {
            // If error occurs, discad the skb buffer
            DBGLOG(CHAR, ERR, "[RX]PORT%d copy_to_user(%p->%p, len=%d) fail: %d", port_id, \
            	payload, buf, count, ret);
            atomic_dec(&curr_node->rx_pkt_drop_cnt);
            eemcs_update_statistics(0, port_id, RX, DROP);
            dev_kfree_skb(rx_skb);
            eemcs_ccci_release_rx_skb(port_id, 1, rx_skb);
            if(rx_pkt_cnt_int == 1)
            {
                curr_node->buff.remaining_len = 0;
                curr_node->buff.remaining_rx_skb = NULL;
                atomic_dec(&curr_node->buff.remaining_rx_cnt);
            } 
        }
    }
    else
    {

        DBGLOG(CHAR, TRA, "[RX]copy_to_user: %p->%p, len=%d", payload, buf, read_len);

        ret = copy_to_user(buf, payload, read_len);
        if(ret!=0)
        {		
            DBGLOG(CHAR, ERR, "[RX]PORT%d copy_to_user(%p->%p, len=%d) fail: %d", port_id, \
            	payload, buf, read_len, ret);
        }       

        dev_kfree_skb(rx_skb);
        eemcs_ccci_release_rx_skb(port_id, 1, rx_skb);

        if(rx_pkt_cnt_int == 1)
        {
            curr_node->buff.remaining_len = 0;
            curr_node->buff.remaining_rx_skb = NULL;
            atomic_dec(&curr_node->buff.remaining_rx_cnt);
        }
    }
    

    if(ret == 0){
        DEBUG_LOG_FUNCTION_LEAVE;
        return read_len;
    }
_exit:

    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}

unsigned int eemcs_cdev_poll(struct file *fp,poll_table *wait)
{
    eemcs_cdev_node_t *curr_node = (eemcs_cdev_node_t *)fp->private_data;
    unsigned int mask=0;
    
    DEBUG_LOG_FUNCTION_ENTRY;    
    DBGLOG(CHAR, DEF, "eemcs_cdev_poll emcs poll enter");

    //poll_wait(fp,&curr_node->tx_waitq, wait);  /* non-blocking, wake up to indicate the state change */
    poll_wait(fp,&curr_node->rx_waitq, wait);  /* non-blocking, wake up to indicate the state change */

    if (ccci_cdev_write_space_alloc(curr_node->ccci_ch.tx)!=0)
    {
        DBGLOG(CHAR, DEF, "eemcs_cdev_poll TX avaliable");
        mask|= POLLOUT|POLLWRNORM;
    }

    if(0 != atomic_read(&curr_node->rx_pkt_cnt))
    {
        DBGLOG(CHAR, DEF, "eemcs_cdev_poll RX avaliable");
        mask|= POLLIN|POLLRDNORM;
    }

    if ((curr_node->eemcs_port_id == CCCI_PORT_META)&& catch_last_log) {
        DBGLOG(CHAR,INF, "notiy reset by poll error");
        catch_last_log = 0;
        mask |= POLLERR;
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return mask;    
}


static struct file_operations eemcs_char_ops=
{
	.owner          =   THIS_MODULE,
	.open           =   eemcs_cdev_open,
	.read           =   eemcs_cdev_read,
	.write          =   eemcs_cdev_write,
	.release        =   eemcs_cdev_release,
	.unlocked_ioctl =   eemcs_cdev_ioctl,
	.poll           =   eemcs_cdev_poll,  
    //.fasync         =   emcs_fasync,
	//.mmap           =   emcs_mmap,
};

// DBGLOG(CHAR,DBG, "[CHAR] OHLA GUENOSDIAS");

static void* create_cdev_class(struct module *owner, const char *name)
{
    int err = 0;
	
    struct class *dev_class = class_create(owner, name);
    if(IS_ERR(dev_class))
    {
        err = PTR_ERR(dev_class);
        DBGLOG(CHAR, ERR, "create class %s fail: %d", name, err);
        return NULL;
    }
    DBGLOG(CHAR, DBG, "create class %s ok",name);
	return dev_class;
}

static int register_cdev_node(void *dev_class, const char *name, int major_id, int minor_start_id, int index)
{
    int ret=0;
    dev_t dev;
    struct device *devices;

	if(index>0){
		dev = MKDEV(major_id, minor_start_id) + index;
		devices = device_create( (struct class *)dev_class, NULL, dev, NULL, "%s%d", name, index );
	}else{
		dev = MKDEV(major_id, minor_start_id);
		devices = device_create( (struct class *)dev_class, NULL, dev, NULL, "%s", name );
	}

	if(IS_ERR(devices))
    {
		ret = PTR_ERR(devices);
		DBGLOG(CHAR, ERR, "create cdev %s fail: %d", name, ret);
    }

	return ret;
}

static void release_cdev_class(void *dev_class)
{
	if(NULL != dev_class){
		class_destroy(dev_class);
    }
}

extern void eemcs_sysfs_init(struct class *dev_class);

/*
 * @brief Store unhandled packets in list of ports
 * @param
 *     None
 * @return
 *     None
 */
void eemcs_char_exception_log_pkts(void)
{
    KAL_UINT32 i = 0, j = 0, pkt_cnt = 0;
    struct sk_buff *skb = NULL;

    for (i = 0; i < EEMCS_CDEV_MAX_NUM; i++) {
//        if (!is_valid_exception_rx_port(IDX2PORT(i))) {
         if ((IDX2PORT(i) != CCCI_PORT_MD_LOG) && (IDX2PORT(i) != CCCI_PORT_META)) {
            pkt_cnt = atomic_read(&eemcs_cdev_inst.cdev_node[i].rx_pkt_cnt);
            if (pkt_cnt != 0) {
				DBGLOG(CHAR, INF, "%d packets in Rx list of port%d", pkt_cnt, IDX2PORT(i));
                for (j = 0; j < pkt_cnt; j++) {
                    skb = skb_dequeue(&eemcs_cdev_inst.cdev_node[i].rx_skb_list);
                    if (skb != NULL) {
                        eemcs_ccci_release_rx_skb(IDX2PORT(i), 1, skb);
                        atomic_dec(&eemcs_cdev_inst.cdev_node[i].rx_pkt_cnt);
                        eemcs_expt_log_port(skb, IDX2PORT(i));
                    } else {
                        DBGLOG(CHAR, INF, "dequeue NULL skb from port%d list", IDX2PORT(i));
                    }
                }
            }
        }
    }
}

/*
 * @brief Exception callback function which is registerd to CCCI layer
 * @param
 *     msg_id [in] Exception ID
 * @return
 *     None
 */
void eemcs_char_exception_callback(KAL_UINT32 msg_id)
{
    DBGLOG(CHAR, INF, "Char exception Callback 0x%X", msg_id);
    switch (msg_id) {
        case EEMCS_EX_INIT:
            eemcs_char_exception_log_pkts();
            break;
        case EEMCS_EX_DHL_DL_RDY:
            break;
        case EEMCS_EX_INIT_DONE:
            break;
        default:
            DBGLOG(CHAR, ERR, "Unknown char exception callback 0x%X", msg_id);
    }
}

KAL_INT32 eemcs_char_mod_init(void){
    KAL_INT32 ret   = KAL_FAIL;
    KAL_INT32 i     = 0;
    ccci_port_cfg *curr_port_info = NULL;
    
    DEBUG_LOG_FUNCTION_ENTRY;
    eemcs_cdev_inst.dev_class    = NULL;
    eemcs_cdev_inst.eemcs_chrdev = NULL;
	
    //4 <1> create dev class
    eemcs_cdev_inst.dev_class = create_cdev_class(THIS_MODULE, EEMCS_DEV_NAME);
    if(!eemcs_cdev_inst.dev_class)
    {
    	ret = KAL_FAIL;
        goto register_chrdev_fail;
    }

    //4 <2> register characer device region 
    ret=register_chrdev_region(MKDEV(EEMCS_DEV_MAJOR, START_OF_NORMAL_PORT), EEMCS_CDEV_MAX_NUM, EEMCS_DEV_NAME);
    if (ret)
    {
        DBGLOG(CHAR, ERR, "register_chrdev_region fail: %d", ret);
        goto register_chrdev_fail;
    }

    //4 <3> allocate character device
    eemcs_cdev_inst.eemcs_chrdev = cdev_alloc();
	if (eemcs_cdev_inst.eemcs_chrdev == NULL)
    {
    	ret = KAL_FAIL;
        DBGLOG(CHAR, ERR, "cdev_alloc fail");
        goto cdev_alloc_fail;
    }

	cdev_init(eemcs_cdev_inst.eemcs_chrdev, &eemcs_char_ops);
	eemcs_cdev_inst.eemcs_chrdev->owner = THIS_MODULE;
    
	ret=cdev_add(eemcs_cdev_inst.eemcs_chrdev, MKDEV(EEMCS_DEV_MAJOR, START_OF_NORMAL_PORT), EEMCS_CDEV_MAX_NUM);
	if (ret)
	{
		DBGLOG(CHAR, ERR, "cdev_add fail: %d", ret);
		goto cdev_add_fail;
	}
    
    //4 <4> register device nodes
    for(i = 0; i < EEMCS_CDEV_MAX_NUM; i++)
    {
        register_cdev_node(eemcs_cdev_inst.dev_class, ccci_cdev_name[i], EEMCS_DEV_MAJOR, IDX2PORT(i), 0);
    }
    eemcs_sysfs_init(eemcs_cdev_inst.dev_class);


    //4 <5> setup ccci_information ccci_cdev_register
    for(i = 0; i < EEMCS_CDEV_MAX_NUM; i++)
    {
        memset(eemcs_cdev_inst.cdev_node[i].cdev_name,0,sizeof(eemcs_cdev_inst.cdev_node[i].cdev_name)); 
        strncpy(eemcs_cdev_inst.cdev_node[i].cdev_name, ccci_cdev_name[i], sizeof(ccci_cdev_name[i]));

        eemcs_cdev_inst.cdev_node[i].eemcs_port_id = IDX2PORT(i);
        curr_port_info = ccci_get_port_info(IDX2PORT(i));
        eemcs_cdev_inst.cdev_node[i].ccci_ch.rx = curr_port_info->ch.rx;
        eemcs_cdev_inst.cdev_node[i].ccci_ch.tx = curr_port_info->ch.tx;
        atomic_set(&eemcs_cdev_inst.cdev_node[i].cdev_state, CDEV_CLOSE);


        skb_queue_head_init(&eemcs_cdev_inst.cdev_node[i].rx_skb_list);
        atomic_set(&eemcs_cdev_inst.cdev_node[i].rx_pkt_cnt, 0);
        atomic_set(&eemcs_cdev_inst.cdev_node[i].rx_pkt_drop_cnt, 0);
        init_waitqueue_head(&eemcs_cdev_inst.cdev_node[i].rx_waitq);
        init_waitqueue_head(&eemcs_cdev_inst.cdev_node[i].tx_waitq);
        atomic_set(&eemcs_cdev_inst.cdev_node[i].tx_pkt_cnt, 0);

        eemcs_cdev_inst.cdev_node[i].buff.remaining_len = 0;
        eemcs_cdev_inst.cdev_node[i].buff.remaining_rx_skb = NULL;
        atomic_set(&eemcs_cdev_inst.cdev_node[i].buff.remaining_rx_cnt,0);

        DBGLOG(CHAR, DBG, "char_dev(%s)(%d), rx_ch(%d), tx_ch(%d)",\
			eemcs_cdev_inst.cdev_node[i].cdev_name, eemcs_cdev_inst.cdev_node[i].eemcs_port_id,\
            eemcs_cdev_inst.cdev_node[i].ccci_ch.rx, eemcs_cdev_inst.cdev_node[i].ccci_ch.tx);
    }

    eemcs_cdev_inst.expt_cb_id = ccci_cdev_register_expt_callback(eemcs_char_exception_callback);
    if (eemcs_cdev_inst.expt_cb_id == KAL_FAIL){
        DBGLOG(CHAR, ERR, "register exp callback fail");
    }

    spin_lock_init(&md_logger_lock);
    
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
    
cdev_add_fail:
    cdev_del(eemcs_cdev_inst.eemcs_chrdev);
cdev_alloc_fail:
    unregister_chrdev_region(MKDEV(EEMCS_DEV_MAJOR, START_OF_NORMAL_PORT), EEMCS_CDEV_MAX_NUM);
register_chrdev_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;    
}

struct class *eemcs_char_get_class(void)
{
    return eemcs_cdev_inst.dev_class;
}

void eemcs_sysfs_exit(struct class *dev_class);

void eemcs_char_exit(void){
    KAL_INT32 i=0;

    DEBUG_LOG_FUNCTION_ENTRY;

    for(i=0 ; i<EEMCS_CDEV_MAX_NUM; i++)
    {
        if (eemcs_cdev_inst.expt_cb_id != -1){
            ccci_cdev_unregister_expt_callback(eemcs_cdev_inst.expt_cb_id);
        }

		device_destroy(eemcs_cdev_inst.dev_class,MKDEV(EEMCS_DEV_MAJOR, IDX2PORT(i)));
    }
    eemcs_sysfs_exit(eemcs_cdev_inst.dev_class);

    if(eemcs_cdev_inst.dev_class)
    {
        DBGLOG(CHAR,DBG,"[CHAR] dev_class unregister ");
        release_cdev_class(eemcs_cdev_inst.dev_class);
    }
    
    if(eemcs_cdev_inst.eemcs_chrdev)
    {
        DBGLOG(CHAR,DBG,"[CHAR] eemcs_chrdev unregister ");
	    cdev_del(eemcs_cdev_inst.eemcs_chrdev);
    }
	unregister_chrdev_region(MKDEV(EEMCS_DEV_MAJOR,START_OF_NORMAL_PORT), EEMCS_CDEV_MAX_NUM);  

    DEBUG_LOG_FUNCTION_LEAVE;
    return;
}


#if defined(_EEMCS_CDEV_LB_UT_)
void cdevut_turn_off_dlq_by_port(KAL_UINT32 port_idx) {	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CHAR,DBG, "[CHAR_UT]CCCI port (%d) turn off downlink queue", port_idx);
    DEBUG_LOG_FUNCTION_LEAVE;
    return;
}

void cdevut_turn_on_dlq_by_port(KAL_UINT32 port_idx) {	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CHAR,DBG, "[CHAR_UT]CCCI port (%d) turn on downlink queue", port_idx);
    DEBUG_LOG_FUNCTION_LEAVE;
    return;
}

KAL_UINT32 cdevut_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data) {	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CHAR,DBG, "[CHAR_UT]CCCI channel (%d) register callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}
KAL_UINT32 cdevut_unregister_callback(CCCI_CHANNEL_T chn) {
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CHAR,DBG, "[CHAR_UT]CCCI channel (%d) UNregister callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 cdevut_UL_write_room_alloc(CCCI_CHANNEL_T chn)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return 1;
}

KAL_UINT32 cdevut_UL_write_room_release(CCCI_CHANNEL_T chn)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return 0;
}

KAL_UINT32 cdevut_UL_write_wait(CCCI_CHANNEL_T chn)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return 1;
}

KAL_UINT32 cdevut_unregister_expt_callback(KAL_UINT32 cb_id)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 cdevut_register_expt_callback(EEMCS_CCCI_EXCEPTION_IND_CALLBACK func_ptr)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 cdevut_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb)
{
	CCCI_BUFF_T *pccci_h = (CCCI_BUFF_T *)skb->data;
    KAL_UINT8 port_id;
    KAL_UINT32 tx_ch, rx_ch;
    
	DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CHAR,DBG, "[CHAR_UT]CCCI channel (%d) ccci_write CCCI_H(0x%x)(0x%x)(0x%x)(0x%x)",\
        chn, pccci_h->data[0], pccci_h->data[1], pccci_h->channel, pccci_h->reserved);

#if defined(_EEMCS_CDEV_LB_UT_)
    {
        struct sk_buff *new_skb;
        new_skb = dev_alloc_skb(skb->len);
        if(new_skb == NULL){
            DBGLOG(NETD,ERR,"[NETD_UT] _ECCMNI_LB_UT_ dev_alloc_skb fail sz(%d).", skb->len);
            dev_kfree_skb(skb);
            DEBUG_LOG_FUNCTION_LEAVE;
    	    return KAL_SUCCESS;
        }        
        memcpy(skb_put(new_skb, skb->len), skb->data, skb->len);
        pccci_h = (CCCI_BUFF_T *)new_skb->data;
        port_id = ccci_ch_to_port(pccci_h->channel);
        tx_ch = pccci_h->channel;
        rx_ch =  eemcs_cdev_inst.cdev_node[PORT2IDX(port_id)].ccci_ch.rx;
        pccci_h->channel = rx_ch;
        
        DBGLOG(CHAR,DBG, "[CHAR_UT]=========PORT(%d) tx_ch(%d) LB to rx_ch(%d)",\
            port_id, tx_ch, rx_ch);

        eemcs_cdev_rx_callback(new_skb, 0);
    }
#if 0
    skb_share_check
    skb_get
#endif    
#endif
    dev_kfree_skb(skb);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}
#endif //_EEMCS_CDEV_LB_UT_


void eemcs_fs_ut_callback(struct sk_buff *new_skb, KAL_UINT32 private_data)
{
#ifdef _EEMCS_FS_UT
    eemcs_cdev_rx_callback(new_skb, 0);
#else // !_EEMCS_FS_UT
#endif // _EEMCS_FS_UT
}


