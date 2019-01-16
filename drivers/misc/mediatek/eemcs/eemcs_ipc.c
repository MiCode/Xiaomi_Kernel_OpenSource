/*****************************************************************************
 *
 * Filename:
 * ---------
 *   eemcs_ipc.c
 *
 * 
 * Author:
 * -------
 *   Anping Wang (mtk05304)
 *
 ****************************************************************************/
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

#include "eemcs_kal.h"
#include "eemcs_debug.h"
#include "eemcs_ipc.h"
#include "eemcs_state.h"
#include "eemcs_statistics.h"

#ifdef ENABLE_CONN_COEX_MSG
#include "conn_md_exp.h"
#endif

static eemcs_ipc_inst_t eemcs_ipc_inst;

#define local_AP_id_2_unify_id(id) local_xx_id_2_unify_id(id,1)
#define local_MD_id_2_unify_id(id) local_xx_id_2_unify_id(id,0)
#define unify_AP_id_2_local_id(id)   unify_xx_id_2_local_id(id,1)
#define unify_MD_id_2_local_id(id)   unify_xx_id_2_local_id(id,0)


static IPC_MSGSVC_TASKMAP_T ipc_msgsvc_maptbl[] =
{
	#define __IPC_ID_TABLE
#include "eemcs_ipc_task_ID.h"
	#undef __IPC_ID_TABLE
    
};

//put after ipc_msgsvc_maptbl for #include "eemcs_ipc_task_ID.h"

static IPC_MSGSVC_TASKMAP_T *local_xx_id_2_unify_id(uint32 local_id,int AP)
{
	int i;
	for (i=0;i<sizeof(ipc_msgsvc_maptbl)/sizeof(ipc_msgsvc_maptbl[0]);i++)
	{
		if (ipc_msgsvc_maptbl[i].task_id==local_id &&
			 (AP?(ipc_msgsvc_maptbl[i].extq_id&AP_UNIFY_ID_FLAG):!(ipc_msgsvc_maptbl[i].extq_id&AP_UNIFY_ID_FLAG)))
			return  ipc_msgsvc_maptbl+i;
	}	
	return NULL;

}

static IPC_MSGSVC_TASKMAP_T *unify_xx_id_2_local_id(uint32 unify_id,int AP)
{
	int i;
	if (!(AP?(unify_id&AP_UNIFY_ID_FLAG):!(unify_id&AP_UNIFY_ID_FLAG)))
		return NULL;
	
	for (i=0;i<sizeof(ipc_msgsvc_maptbl)/sizeof(ipc_msgsvc_maptbl[0]);i++)
	{
		if(ipc_msgsvc_maptbl[i].extq_id==unify_id)
			return 	ipc_msgsvc_maptbl+i;
	}
	return NULL;
}

/****************************************/
/*For IDC test*/
//	typedef int (*CONN_MD_MSG_RX_CB)(ipc_ilm_t *ilm);
//	typedef struct{
//	    CONN_MD_MSG_RX_CB rx_cb;
//	}CONN_MD_BRIDGE_OPS, *P_CONN_MD_BRIDGE_OPS;
//	extern int mtk_conn_md_bridge_reg(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops);
//	extern int mtk_conn_md_bridge_unreg(uint32 u_id);
//	
//	extern int mtk_conn_md_bridge_send_msg(ipc_ilm_t *ilm);
//	
//	int __weak mtk_conn_md_bridge_reg(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops){printk(KERN_ERR "MTK_CONN Weak FUNCTION~~~\n"); return 0;}
//	int __weak mtk_conn_md_bridge_unreg(uint32 u_id){printk(KERN_ERR "MTK_CONN Weak FUNCTION~~~\n"); return 0;}
//	int __weak mtk_conn_md_bridge_send_msg(ipc_ilm_t *ilm){printk(KERN_ERR "MTK_CONN Weak FUNCTION~~~\n"); return 0;}

/***************************************/
static KAL_INT32 eemcs_ipc_rx_callback(struct sk_buff *skb, KAL_UINT32 private_data)
{
    CCCI_BUFF_T *p_cccih = NULL;
    KAL_UINT32  node_id;
    IPC_MSGSVC_TASKMAP_T *id_map;
    unsigned int i = 0;
    char * addr;

    DEBUG_LOG_FUNCTION_ENTRY;

    if (skb){
        p_cccih = (CCCI_BUFF_T *)skb->data;
        DBGLOG(IPCD,DBG,"[RX]CCCI_H(0x%08X, 0x%08X, %02d, 0x%08X)", \
            p_cccih->data[0],p_cccih->data[1],p_cccih->channel, p_cccih->reserved);
    }
    
#ifndef _EEMCS_IPCD_LB_UT_
    /* Check IPC task id and extq_id */
    if ((id_map=unify_AP_id_2_local_id(p_cccih->reserved))==NULL)
    {
       DBGLOG(IPCD,ERR,"Wrong AP Unify id (%#x)@RX.!!! PACKET DROP !!!\n",p_cccih->reserved);
       dev_kfree_skb(skb);
       return KAL_SUCCESS ;
    }
    node_id = id_map->task_id;
#else
    node_id = 0;
#endif
    if(IPCD_KERNEL == atomic_read(&eemcs_ipc_inst.ipc_node[node_id].dev_state)){
        ipc_ilm_t* p_ilm = NULL;
        skb_pull(skb, sizeof(CCCI_BUFF_T));
        p_ilm = (ipc_ilm_t*)(skb->data);
        p_ilm->dest_mod_id = p_cccih->reserved;
        p_ilm->local_para_ptr = (local_para_struct *)(p_ilm+1);

        if (p_ilm->local_para_ptr != NULL) {
            DBGLOG(IPCD, INF, "[RX][KERN]src=%d dest=0x%x sap=0x%x msg=0x%x local_ptr=%p msg_len=%d", \
				p_ilm->src_mod_id, p_ilm->dest_mod_id, p_ilm->sap_id, p_ilm->msg_id,\
				p_ilm->local_para_ptr, p_ilm->local_para_ptr->msg_len); 
            for (i=0; i<32; i++) {
		addr = (char *)p_ilm;
		DBGLOG(IPCD, DBG, "%p=%x", (addr+i), *(addr+i));
            }
        }
        #ifdef ENABLE_CONN_COEX_MSG		
        mtk_conn_md_bridge_send_msg((ipc_ilm_t*)(skb->data));
        #endif
        dev_kfree_skb(skb);
    }
    else if(IPCD_OPEN == atomic_read(&eemcs_ipc_inst.ipc_node[node_id].dev_state)){
        skb_queue_tail(&eemcs_ipc_inst.ipc_node[node_id].rx_skb_list, skb); /* spin_lock_ireqsave inside, refering skbuff.c */
        atomic_inc(&eemcs_ipc_inst.ipc_node[node_id].rx_pkt_cnt);     /* increase rx_pkt_cnt */
        kill_fasync(&eemcs_ipc_inst.ipc_node[node_id].fasync, SIGIO, POLL_IN);
        wake_up_poll(&eemcs_ipc_inst.ipc_node[node_id].rx_waitq,POLLIN); /* wake up rx_waitq */
    }else{
        DBGLOG(IPCD, ERR, "PKT DROP while ipc dev(%d) closed", node_id);
        dev_kfree_skb(skb);
        eemcs_update_statistics(0, eemcs_ipc_inst.eemcs_port_id, RX, DROP);
    }
    
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS ;
}

static int eemcs_ipc_open(struct inode *inode,  struct file *file)
{
    int id = iminor(inode) - EEMCS_IPC_MINOR_BASE;
    int ret = 0;
    DEBUG_LOG_FUNCTION_ENTRY;
    if (id >= EEMCS_IPCD_MAX_NUM || id < 0){
        DBGLOG(IPCD, ERR, "Wrong minor num(%d)", id);
        return -EINVAL;
    }
    
    DBGLOG(IPCD, INF, "ipc_open: deivce(%s) iminor(%d)", \
			eemcs_ipc_inst.ipc_node[id].dev_name, id);

    //4 <1> check multiple open
    if(IPCD_CLOSE != atomic_read(&eemcs_ipc_inst.ipc_node[id].dev_state)){
        DBGLOG(IPCD, ERR, "PORT%d multi-open fail!", id);
        return -EIO;
    }
    //4 <2>  clear the rx_skb_list
    skb_queue_purge(&eemcs_ipc_inst.ipc_node[id].rx_skb_list);
    atomic_set(&eemcs_ipc_inst.ipc_node[id].rx_pkt_cnt, 0);
    
    file->private_data = &eemcs_ipc_inst.ipc_node[id];
    nonseekable_open(inode, file);
    atomic_set(&eemcs_ipc_inst.ipc_node[id].dev_state, IPCD_OPEN);
    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}

#ifdef ENABLE_CONN_COEX_MSG
static int eemcs_ipc_kern_open(int id)
{
    int ret = 0;
    DEBUG_LOG_FUNCTION_ENTRY;
    if (id >= EEMCS_IPCD_MAX_NUM || id < 0){
        DBGLOG(IPCD, ERR, "Wrong minor num(%d)", id);
        return -EINVAL;
    }
    DBGLOG(IPCD, INF, "ipc_kern_open: deivce(%s) iminor(%d)", \
        eemcs_ipc_inst.ipc_node[id].dev_name, id);

    //4 <1> check multiple open
    if(IPCD_CLOSE != atomic_read(&eemcs_ipc_inst.ipc_node[id].dev_state)){
        DBGLOG(IPCD, ERR, "PORT%d multi-open fail!", id);
        return -EIO;
    }
    //4 <2>  clear the rx_skb_list
    skb_queue_purge(&eemcs_ipc_inst.ipc_node[id].rx_skb_list);
    atomic_set(&eemcs_ipc_inst.ipc_node[id].rx_pkt_cnt, 0);
    
    atomic_set(&eemcs_ipc_inst.ipc_node[id].dev_state, IPCD_KERNEL);
    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}
#endif

static int eemcs_ipc_release(struct inode *inode, struct file *file)
{    
    int id = iminor(inode) - EEMCS_IPC_MINOR_BASE;
    DEBUG_LOG_FUNCTION_ENTRY;    
    DBGLOG(IPCD, INF, "ipc_release: deivce(%s) iminor(%d) ", \
        eemcs_ipc_inst.ipc_node[id].dev_name, id);

    atomic_set(&eemcs_ipc_inst.ipc_node[id].dev_state, IPCD_CLOSE);
    skb_queue_purge(&eemcs_ipc_inst.ipc_node[id].rx_skb_list);
    atomic_set(&eemcs_ipc_inst.ipc_node[id].rx_pkt_cnt, 0);

    DEBUG_LOG_FUNCTION_LEAVE;
    return 0;
}

static long eemcs_ipc_ioctl( struct file *fp, unsigned int cmd, unsigned long arg)
{
    eemcs_ipc_node_t *curr_node = (eemcs_ipc_node_t *)fp->private_data;
    int ret = 0;

    DEBUG_LOG_FUNCTION_ENTRY;

    switch(cmd)
    {
        case CCCI_IPC_RESET_RECV:
            DBGLOG( IPCD, TRA, "CCCI_IPC_RESET_RECV: Clean device(%d)", curr_node->ipc_node_id );
            skb_queue_purge(&curr_node->rx_skb_list);
            atomic_set(&curr_node->rx_pkt_cnt, 0);
            ret = 0;
            break;
        case CCCI_IPC_RESET_SEND:
            DBGLOG(IPCD, TRA, "CCCI_IPC_RESET_SEND: Wakeup device(%d)", curr_node->ipc_node_id );
            wake_up(&curr_node->tx_waitq);
            ret = 0;
            break;
        case CCCI_IPC_WAIT_MD_READY:
            if (check_device_state() != EEMCS_BOOTING_DONE){
                DBGLOG(IPCD, TRA, "CCCI_IPC_WAIT_MD_READY: MD not ready(sta=%d)", check_device_state());
                wait_event_interruptible(eemcs_ipc_inst.state_waitq,(check_device_state() == EEMCS_BOOTING_DONE));
            }
            ret = 0;
			
            DBGLOG(IPCD, TRA, "CCCI_IPC_WAIT_MD_READY ok");
            break;
        case CCCI_IPC_KERN_WRITE_TEST:
        {
            int node_id;
            ipc_ilm_t ilm = {AP_MOD_WMT, MD_MOD_L4C, 0, 0, NULL, NULL};
            if(copy_from_user(&node_id, (void __user *)arg, sizeof(unsigned int))) {
                DBGLOG(IPCD, ERR, "CCCI_IPC_KERN_WRITE_TEST: copy_from_user fail!");
                ret = -EFAULT;
                break;
            } 
            DBGLOG(IPCD, TRA, "CCCI_IPC_KERN_WRITE_TEST: %d", node_id);

            ret = eemcs_ipc_kern_write(&ilm);
            if (ret > 0 ){
                ret = 0;
            }

            break;
        }
        default:
            DBGLOG(IPCD, ERR, "unsupport ioctrl(%d)\n", cmd);
            ret=-EINVAL;
            break;           
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

static ssize_t eemcs_ipc_write(struct file *fp, const char __user *buf, size_t in_sz, loff_t *ppos)
{
    ssize_t ret   = 0;
    eemcs_ipc_node_t *curr_node = (eemcs_ipc_node_t *)fp->private_data;
    KAL_UINT8 node_id = curr_node->ipc_node_id;/* node_id */
    KAL_UINT8 port_id = eemcs_ipc_inst.eemcs_port_id; /* port_id */
    KAL_UINT32 p_type, control_flag;    
    struct sk_buff *new_skb;
    CCCI_BUFF_T *ccci_header;
    ipc_ilm_t *ilm=NULL;
    IPC_MSGSVC_TASKMAP_T *id_map;
    size_t count = in_sz;
    size_t skb_alloc_size;
    KAL_UINT32 alloc_time = 0, curr_time = 0;
    
    DEBUG_LOG_FUNCTION_ENTRY;        
    DBGLOG(IPCD, DBG, "[TX]deivce=%s iminor=%d len=%d", curr_node->dev_name, node_id, count);

    p_type = ccci_get_port_type(port_id);
    if(p_type != EX_T_USER) 
    {
        DBGLOG(IPCD, ERR, "PORT%d refuse port(%d) access user port", port_id, p_type);
        ret=-EINVAL;
        goto _exit;                    
    }

    control_flag = ccci_get_port_cflag(port_id);	
    if (check_device_state() == EEMCS_EXCEPTION) {//modem exception		
        if ((control_flag & TX_PRVLG2) == 0) {
            DBGLOG(IPCD, TRA, "[TX]PORT%d write fail when modem exception", port_id);
            return -ETXTBSY;
        }
    } else if (check_device_state() != EEMCS_BOOTING_DONE) {//modem not ready
        if ((control_flag & TX_PRVLG1) == 0) {
            DBGLOG(IPCD, TRA, "[TX]PORT%d write fail when modem not ready", port_id);
            return -ENODEV;
        }
    }

    if((control_flag & EXPORT_CCCI_H) && (count < sizeof(CCCI_BUFF_T)))
    {
        DBGLOG(IPCD, ERR, "invalid wirte_len(%d) of PORT%d", count, port_id);
        ret=-EINVAL;
        goto _exit;            
    }

    if(control_flag & EXPORT_CCCI_H){
        if(count > (MAX_TX_BYTE+sizeof(CCCI_BUFF_T))){
            DBGLOG(IPCD, WAR, "PORT%d wirte_len(%d)>MTU(%d)!", port_id, count, MAX_TX_BYTE);
            count = MAX_TX_BYTE+sizeof(CCCI_BUFF_T);
        }
        skb_alloc_size = count - sizeof(CCCI_BUFF_T);
    }else{
        if(count > MAX_TX_BYTE){
            DBGLOG(IPCD, WAR, "PORT%d wirte_len(%d)>MTU(%d)!", port_id, count, MAX_TX_BYTE);
            count = MAX_TX_BYTE;
        }
        skb_alloc_size = count;
    }

    if (ccci_ch_write_space_alloc(eemcs_ipc_inst.ccci_ch.tx)==0){
        DBGLOG(IPCD, WAR, "PORT%d write return 0)", port_id);
        ret = -EAGAIN;
        goto _exit;
    }	
    
    new_skb = ccci_ipc_mem_alloc(skb_alloc_size + CCCI_IPC_HEADER_ROOM, GFP_ATOMIC);
    if(NULL == new_skb)
    {
    	DBGLOG(CHAR, INF, "[TX]PORT%d alloc skb fail with wait", port_id);
    	alloc_time = jiffies;
    	new_skb = ccci_ipc_mem_alloc(skb_alloc_size + CCCI_IPC_HEADER_ROOM, GFP_KERNEL);
    	if (NULL == new_skb) {
            ret = -ENOMEM;
            DBGLOG(IPCD, ERR, "[TX]PORT%d alloc skb fail with wait fail", port_id);
            goto _exit; 
    	}

    	curr_time = jiffies;
    	if ((curr_time - alloc_time) >= 1) {			
            DBGLOG(IPCD, ERR, "[TX]PORT%d alloc skb delay: time=%dms", port_id, \
			10*(curr_time - alloc_time));
    	}
    }
    
    /* reserve SDIO_H header room */
    #ifdef CCCI_SDIO_HEAD
    skb_reserve(new_skb, sizeof(SDIO_H));
    #endif

    ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ;

    if(copy_from_user(skb_put(new_skb, count), buf, count))
    {
        DBGLOG(IPCD, ERR, "[TX]PORT%d copy_from_user(len=%d, %p->%p) fail", \
		port_id, count, buf, new_skb->data);
        dev_kfree_skb(new_skb);
        ret = -EFAULT;
        goto _exit;
    }

    ilm = (ipc_ilm_t*)((char*)ccci_header + sizeof(CCCI_BUFF_T));
    /* Check IPC extq_id */
	if ((id_map=local_MD_id_2_unify_id(ilm->dest_mod_id))==NULL)
	{
		DBGLOG(IPCD,ERR,"Invalid dest_mod_id=%d",ilm->dest_mod_id);
		dev_kfree_skb(new_skb);
		ret=-EINVAL;
		goto _exit;
	}
    
    /* user bring down the payload only */
    ccci_header->data[1]    = count + sizeof(CCCI_BUFF_T);
    ccci_header->reserved   = id_map->extq_id;
    ccci_header->channel    = eemcs_ipc_inst.ccci_ch.tx;

    DBGLOG(IPCD, DBG, "[TX]PORT%d CCCI_MSG(0x%08X, 0x%08X, %02d, 0x%08X)", 
                    port_id, 
                    ccci_header->data[0], ccci_header->data[1],
                    ccci_header->channel, ccci_header->reserved);

  
    ret = ccci_ch_write_desc_to_q(ccci_header->channel, new_skb);

    if (KAL_SUCCESS != ret) {
        DBGLOG(IPCD, ERR, "PORT%d PKT DROP of ch%d!", port_id, ccci_header->channel);
        dev_kfree_skb(new_skb);
        ret = -EAGAIN;
    } else {
        atomic_inc(&curr_node->tx_pkt_cnt);
        wake_up(&curr_node->tx_waitq); /* wake up tx_waitq for notify poll_wait of state change */
    }

_exit:
    DEBUG_LOG_FUNCTION_LEAVE;
    if(!ret){
        return count;
    }
    return ret;
}

ssize_t eemcs_ipc_kern_write(ipc_ilm_t *in_ilm){
    ssize_t ret   = 0;
    eemcs_ipc_node_t *curr_node = NULL;
    KAL_UINT8 node_id = 0;
    KAL_UINT8 port_id = eemcs_ipc_inst.eemcs_port_id; /* port_id */
    KAL_UINT32 p_type, control_flag;    
    struct sk_buff *new_skb;
    CCCI_BUFF_T *ccci_header;
    ipc_ilm_t *ilm=NULL;
    IPC_MSGSVC_TASKMAP_T *id_map;
    size_t count = sizeof(ipc_ilm_t);
    size_t skb_alloc_size;
    char * addr;
    int i = 0;
    DEBUG_LOG_FUNCTION_ENTRY;
    
    // src module id check
    node_id =(KAL_UINT8) (in_ilm->src_mod_id & (~AP_UNIFY_ID_FLAG)); // source id is ap side txq_id
    if (node_id >= EEMCS_IPCD_MAX_NUM){
        DBGLOG(IPCD, ERR, "invalid src_mod_id=0x%x", in_ilm->src_mod_id);
        ret = -EINVAL;
        goto _exit;
    }
    curr_node = (eemcs_ipc_node_t *)&eemcs_ipc_inst.ipc_node[node_id];
    node_id = curr_node->ipc_node_id;/* node_id */
    if (atomic_read(&curr_node->dev_state) != IPCD_KERNEL){
        DBGLOG(IPCD, ERR, "invalid dev_state(not IPCD_KERNEL), src_mod_id=0x%x", in_ilm->src_mod_id);
        ret = -EINVAL;
        goto _exit;
    }

    p_type = ccci_get_port_type(port_id);
    if(p_type != EX_T_USER) 
    {
        DBGLOG(IPCD, ERR, "PORT%d refuse port(%d) access user port", port_id, p_type);
        ret=-EINVAL;
        goto _exit;                    
    }
	
    control_flag = ccci_get_port_cflag(port_id);
    if (check_device_state() == EEMCS_EXCEPTION) {//modem exception		
        if ((control_flag & TX_PRVLG2) == 0) {
            DBGLOG(IPCD, TRA, "[TX]PORT%d kernel write fail when modem exception", port_id);
            return -ETXTBSY;
        }
    } else if (check_device_state() != EEMCS_BOOTING_DONE) {//modem not ready
        if ((control_flag & TX_PRVLG1) == 0) {
            DBGLOG(IPCD, TRA, "[TX]PORT%d kernel write fail when modem not ready", port_id);
            return -ENODEV;
        }
    }

    DBGLOG(IPCD, INF, "[TX][KERN]iminor=%d src=0x%x dest=0x%x sap=0x%x msg_id=0x%x local_ptr=%#X peer_ptr=%#X",\
                node_id, \
                (unsigned int)in_ilm->src_mod_id, (unsigned int)in_ilm->dest_mod_id, \
                (unsigned int)in_ilm->sap_id, (unsigned int)in_ilm->msg_id, \
                (unsigned int)in_ilm->local_para_ptr, (unsigned int)in_ilm->peer_buff_ptr);

    if(in_ilm->local_para_ptr != NULL){
        count = sizeof(ipc_ilm_t) + in_ilm->local_para_ptr->msg_len;		
        DBGLOG(IPCD, DBG, "[TX][KERN]ilm_len=%d local_len=%d msg_len=%d", \
		sizeof(ipc_ilm_t), sizeof(local_para_struct), count);
    }
	
    if((control_flag & EXPORT_CCCI_H) && (count < sizeof(CCCI_BUFF_T)))
    {
        DBGLOG(IPCD, ERR, "invalid wirte_len(%d) of PORT%d", count, port_id);
        ret=-EINVAL;
        goto _exit;            
    }

    if(control_flag & EXPORT_CCCI_H){
        if(count > (MAX_TX_BYTE+sizeof(CCCI_BUFF_T))){
            DBGLOG(IPCD, WAR, "PORT%d wirte_len(%d)>MTU(%d)", port_id, count, MAX_TX_BYTE);
            count = MAX_TX_BYTE+sizeof(CCCI_BUFF_T);
        }
        skb_alloc_size = count - sizeof(CCCI_BUFF_T);
    }else{
        if(count > MAX_TX_BYTE){
            DBGLOG(IPCD, WAR, "PORT%d wirte_len(%d)>MTU(%d)", port_id, count, MAX_TX_BYTE);
            count = MAX_TX_BYTE;
        }
        skb_alloc_size = count;
    }

    if (ccci_ch_write_space_alloc(eemcs_ipc_inst.ccci_ch.tx)==0){
        DBGLOG(IPCD, WAR, "PORT%d write return 0)", port_id);
        ret = -EAGAIN;
        goto _exit;
    }	
    
    new_skb = ccci_ipc_mem_alloc(skb_alloc_size + CCCI_IPC_HEADER_ROOM, GFP_ATOMIC);
    if(NULL == new_skb)
    {
        DBGLOG(IPCD, ERR, "[TX]PORT%d alloc skb fail", port_id);
        ret = -ENOMEM;
        goto _exit;            
    }
    
    /* reserve SDIO_H header room */
    #ifdef CCCI_SDIO_HEAD
    skb_reserve(new_skb, sizeof(SDIO_H));
    #endif

    ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ;

    memcpy(skb_put(new_skb, sizeof(ipc_ilm_t)), in_ilm, sizeof(ipc_ilm_t));

    count = in_ilm->local_para_ptr->msg_len;
    memcpy(skb_put(new_skb, count), in_ilm->local_para_ptr, count);

    ilm = (ipc_ilm_t*)((char*)ccci_header + sizeof(CCCI_BUFF_T));
    for (i=0; i<count+sizeof(ipc_ilm_t); i++) {
        addr = (char *)ilm;
        DBGLOG(IPCD, DBG, "%p=%x", (addr+i), *(addr+i));
    }
    /* Check IPC extq_id */
    if ((id_map=local_MD_id_2_unify_id(ilm->dest_mod_id))==NULL)
    {
        DBGLOG(IPCD,ERR,"Invalid dest_mod_id=%d",ilm->dest_mod_id);
        dev_kfree_skb(new_skb);
        ret=-EINVAL;
        goto _exit;
    }
    
    /* user bring down the payload only */
    ccci_header->data[1]    = count + sizeof(CCCI_BUFF_T);
    ccci_header->reserved   = id_map->extq_id;
    ccci_header->channel    = eemcs_ipc_inst.ccci_ch.tx;

    DBGLOG(IPCD, DBG, "[TX][KERN]PORT%d CCCI_MSG(0x%08X, 0x%08X, %02d, 0x%08X)", 
                    port_id, 
                    ccci_header->data[0], ccci_header->data[1],
                    ccci_header->channel, ccci_header->reserved);

  
    ret = ccci_ch_write_desc_to_q(ccci_header->channel, new_skb);

    if (KAL_SUCCESS != ret) {
        DBGLOG(IPCD, ERR, "PKT DROP of ch%d!",ccci_header->channel);
        dev_kfree_skb(new_skb);
        ret = -EAGAIN;
    } else {
        atomic_inc(&curr_node->tx_pkt_cnt);
        wake_up(&curr_node->tx_waitq); /* wake up tx_waitq for notify poll_wait of state change */
    }

_exit:
    DEBUG_LOG_FUNCTION_LEAVE;
    if(!ret){
        return count;
    }
    return ret;    
}


static ssize_t eemcs_ipc_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
    unsigned int flag;
    eemcs_ipc_node_t *curr_node = (eemcs_ipc_node_t *)fp->private_data;
    KAL_UINT8 node_id = curr_node->ipc_node_id;/* node_id */
    KAL_UINT8 port_id = eemcs_ipc_inst.eemcs_port_id; /* port_id */
    KAL_UINT32 rx_pkt_cnt, read_len;
    struct sk_buff *rx_skb;
    unsigned char *payload=NULL;
    CCCI_BUFF_T *ccci_header;
    int ret = 0;

    DEBUG_LOG_FUNCTION_ENTRY;
    
    flag=fp->f_flags;
    DBGLOG(IPCD, DBG, "[RX]deivce iminor=%d, len=%d", node_id ,count);

    if(!eemcs_device_ready())
    {
        DBGLOG(IPCD, ERR, "MD device not ready!");
        ret= -EIO;
        return ret;
    }

    /* Check receive pkt count */
    rx_pkt_cnt = atomic_read(&curr_node->rx_pkt_cnt);
    KAL_ASSERT(rx_pkt_cnt >= 0);
    
    if(rx_pkt_cnt == 0){
        if (flag&O_NONBLOCK)
        {	
            ret=-EAGAIN;
            DBGLOG(IPCD, TRA, "ipc_read: PORT%d for NONBLOCK",port_id);
            goto _exit;
        }
        ret = wait_event_interruptible(curr_node->rx_waitq, atomic_read(&curr_node->rx_pkt_cnt) > 0);
        if(ret)
        {
            ret = -EINTR;
            DBGLOG(IPCD, ERR, "[RX]PORT%d read interrupt by syscall.signal(%lld)", port_id, \
		*(long long *)current->pending.signal.sig);	
            goto _exit;
        }
    }

    /*
     * Cached memory from last read fail
     */
    DBGLOG(IPCD, DBG, "[RX]dequeue from rx_skb_list, rx_pkt_cnt(%d)",rx_pkt_cnt);
    rx_skb = skb_dequeue(&curr_node->rx_skb_list);
    /* There should be rx_skb in the list */
    KAL_ASSERT(NULL != rx_skb);
    atomic_dec(&curr_node->rx_pkt_cnt);
    rx_pkt_cnt = atomic_read(&curr_node->rx_pkt_cnt);
    KAL_ASSERT(rx_pkt_cnt >= 0);

    ccci_header = (CCCI_BUFF_T *)rx_skb->data;

    DBGLOG(IPCD, DBG, "[RX]PORT%d CCCI_MSG(0x%08X, 0x%08X, %02d, 0x%08X)",\
            port_id, ccci_header->data[0],ccci_header->data[1],
            ccci_header->channel, ccci_header->reserved);
    
    /*If not match please debug EEMCS CCCI demux skb part*/
    KAL_ASSERT(ccci_header->channel == eemcs_ipc_inst.ccci_ch.rx);
    
    read_len = ccci_header->data[1] - sizeof(CCCI_BUFF_T);
    /* remove CCCI_HEADER */
    skb_pull(rx_skb, sizeof(CCCI_BUFF_T));

    payload=(unsigned char*)rx_skb->data;
    if(count < read_len)
    {
        DBGLOG(IPCD, ERR, "PKT DROP of PORT%d! want_read=%d, read_len=%d", 
                port_id, count, read_len);
        atomic_inc(&curr_node->rx_pkt_drop_cnt);
        eemcs_update_statistics(0, eemcs_ipc_inst.eemcs_port_id, RX, DROP);
        dev_kfree_skb(rx_skb);
        ret = -E2BIG;
        goto _exit;
    }

    ret = copy_to_user(buf, payload, read_len);
    if(ret!=0)
    {
        DBGLOG(IPCD, ERR, "[RX]PORT%d copy_to_user(len=%d, %p->%p) fail: %d", \
		port_id, read_len, payload, buf, ret);
        ret = -EFAULT;
        goto _exit;
    }       
    DBGLOG(IPCD, DBG, "[RX]copy_to_user(len=%d, %d): %p->%p", read_len, ret, payload, buf);

    dev_kfree_skb(rx_skb);

    if(ret == 0){
        DEBUG_LOG_FUNCTION_LEAVE;
        return read_len;
    }
_exit:    

    DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}


static int eemcs_ipc_fasync(int fd, struct file *file, int on)
{
	eemcs_ipc_node_t *curr_node = (eemcs_ipc_node_t *)file->private_data;
	return fasync_helper(fd,file,on,&curr_node->fasync);
}

unsigned int eemcs_ipc_poll(struct file *fp, poll_table *wait)
{
    eemcs_ipc_node_t *curr_node = (eemcs_ipc_node_t *)fp->private_data;
    unsigned int mask=0;
    
    DEBUG_LOG_FUNCTION_ENTRY;    
    DBGLOG(IPCD,DEF,"eemcs_ipc_poll: enter");

	poll_wait(fp,&curr_node->tx_waitq, wait);  /* non-blocking, wake up to indicate the state change */
	poll_wait(fp,&curr_node->rx_waitq, wait);  /* non-blocking, wake up to indicate the state change */

    if (ccci_ch_write_space_alloc(eemcs_ipc_inst.ccci_ch.tx)!=0)
    {
        DBGLOG(IPCD,DEF,"eemcs_ipc_poll: TX avaliable");
        mask|= POLLOUT|POLLWRNORM;
    }

	if(0 != atomic_read(&curr_node->rx_pkt_cnt))
    {
        DBGLOG(IPCD,DEF,"eemcs_ipc_poll: RX avaliable");
        mask|= POLLIN|POLLRDNORM;
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return mask;    
}


void eemcs_ipc_state_callback_func(EEMCS_STATE state){
    switch(state){
        case EEMCS_EXCEPTION:
        case EEMCS_GATE: //MD reset
            if (eemcs_ipc_inst.md_is_ready){
                int i;
                eemcs_ipc_inst.md_is_ready = 0;
                for(i=0; i<EEMCS_IPCD_MAX_NUM;i++){
                    DBGLOG( IPCD, TRA, "ipc_state_callback: Clean device(%s) when ipc_sta=%d", \
						eemcs_ipc_inst.ipc_node[i].dev_name, state);
                    skb_queue_purge(&eemcs_ipc_inst.ipc_node[i].rx_skb_list);
                    atomic_set(&eemcs_ipc_inst.ipc_node[i].rx_pkt_cnt, 0);
                }               
            }
            break;
        case EEMCS_BOOTING_DONE:
            DBGLOG( IPCD, TRA, "ipc_state_callback: MD booting DONE");
            wake_up_interruptible(&eemcs_ipc_inst.state_waitq);
            eemcs_ipc_inst.md_is_ready = 1;
            break; 
        default:
            break;
    }
}

EEMCS_STATE_CALLBACK_T eemcs_ipc_state_callback ={
    .callback = eemcs_ipc_state_callback_func,
};

static struct file_operations eemcs_ipc_ops=
{
	.owner          =   THIS_MODULE,
	.open           =   eemcs_ipc_open,
	.read           =   eemcs_ipc_read,
	.write          =   eemcs_ipc_write,
	.release        =   eemcs_ipc_release,
	.unlocked_ioctl =   eemcs_ipc_ioctl,
	.poll           =   eemcs_ipc_poll,  
    .fasync         =   eemcs_ipc_fasync,
};

static void* create_ipc_class(struct module *owner, const char *name)
{
    int err = 0;
    struct class *dev_class = class_create(owner, name);
    if(IS_ERR(dev_class))
    {
        err = PTR_ERR(dev_class);
        DBGLOG(IPCD, ERR, "create class %s fail: %d", name, err);
        return NULL;
    }
    DBGLOG(IPCD, DBG," create class %s ok",name);
	return dev_class;
}

static void release_ipc_class(void *dev_class)
{
	if(NULL != dev_class){
		class_destroy(dev_class);
        DBGLOG(IPCD, DBG, "release_ipc_class ok");
    }
}

static int register_ipc_node(void *dev_class, const char *name, int major_id, int minor_start_id, int index)
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
		DBGLOG(IPCD, ERR, "create ipc dev %s fail: %d", name, ret);
    }

	return ret;
}

KAL_INT32 eemcs_ipc_mod_init(void){
    KAL_INT32 ret = KAL_FAIL;
    KAL_INT32 i = 0;
    ccci_port_cfg *curr_port_info = NULL;
    
    DEBUG_LOG_FUNCTION_ENTRY;
    eemcs_ipc_inst.dev_class    = NULL;
    eemcs_ipc_inst.eemcs_ipcdev = NULL;
    init_waitqueue_head(&eemcs_ipc_inst.state_waitq);
    eemcs_state_callback_register(&eemcs_ipc_state_callback);

    //4 <1> create dev class
    eemcs_ipc_inst.dev_class = create_ipc_class(THIS_MODULE, EEMCS_IPC_NAME);
    if(!eemcs_ipc_inst.dev_class)
    {
    	ret = KAL_FAIL;
        goto register_chrdev_fail;
    }

    //4 <2> register characer device region 
    ret = register_chrdev_region(MKDEV(EEMCS_IPC_MAJOR,EEMCS_IPC_MINOR_BASE), EEMCS_IPCD_MAX_NUM, EEMCS_IPC_NAME);
    if (ret)
    {
        DBGLOG(IPCD, ERR, "register_chrdev_region fail: %d", ret);
        goto register_chrdev_fail;
    }

    //4 <3> allocate character device
    eemcs_ipc_inst.eemcs_ipcdev = cdev_alloc();
	if (eemcs_ipc_inst.eemcs_ipcdev == NULL)
    {
    	ret = KAL_FAIL;
        DBGLOG(IPCD, ERR, "cdev_alloc fail");
        goto cdev_alloc_fail;
    }

	cdev_init(eemcs_ipc_inst.eemcs_ipcdev, &eemcs_ipc_ops);
	eemcs_ipc_inst.eemcs_ipcdev->owner = THIS_MODULE;
    
	ret=cdev_add(eemcs_ipc_inst.eemcs_ipcdev, MKDEV(EEMCS_IPC_MAJOR,EEMCS_IPC_MINOR_BASE), EEMCS_IPCD_MAX_NUM);
	if (ret)
	{
		DBGLOG(IPCD,ERR,"cdev_add fail: %d", ret);
		goto cdev_add_fail;
	}
   
    //4 <4> setup ccci_information ccci_ch_register
    eemcs_ipc_inst.eemcs_port_id = EEMCS_IPC_PORT;
    curr_port_info = ccci_get_port_info(EEMCS_IPC_PORT);
    eemcs_ipc_inst.ccci_ch.rx = curr_port_info->ch.rx;
    eemcs_ipc_inst.ccci_ch.tx = curr_port_info->ch.tx;

    //4 <5>  register ccci channel 
    ret = ccci_ch_register(eemcs_ipc_inst.ccci_ch.rx, eemcs_ipc_rx_callback, 0);
    if(ret != KAL_SUCCESS){
        DBGLOG(IPCD, ERR, "PORT%d register Ch%d fail!", eemcs_ipc_inst.eemcs_port_id, eemcs_ipc_inst.ccci_ch.rx);
        KAL_ASSERT(0);
    }
    

    for(i = 0; i < EEMCS_IPCD_MAX_NUM; i++)
    {
        //4 <6> register device nodes
        sprintf(eemcs_ipc_inst.ipc_node[i].dev_name,"%s_%d", EEMCS_IPC_NAME, i);
        register_ipc_node(eemcs_ipc_inst.dev_class, eemcs_ipc_inst.ipc_node[i].dev_name, EEMCS_IPC_MAJOR, EEMCS_IPC_MINOR_BASE+i, 0);
        
        eemcs_ipc_inst.ipc_node[i].ipc_node_id = i;
        
        atomic_set(&eemcs_ipc_inst.ipc_node[i].dev_state, IPCD_CLOSE);

        skb_queue_head_init(&eemcs_ipc_inst.ipc_node[i].rx_skb_list);
        atomic_set(&eemcs_ipc_inst.ipc_node[i].rx_pkt_cnt, 0);
        atomic_set(&eemcs_ipc_inst.ipc_node[i].rx_pkt_drop_cnt, 0);
        init_waitqueue_head(&eemcs_ipc_inst.ipc_node[i].rx_waitq);
        init_waitqueue_head(&eemcs_ipc_inst.ipc_node[i].tx_waitq);
        atomic_set(&eemcs_ipc_inst.ipc_node[i].tx_pkt_cnt, 0);
        
        DBGLOG(IPCD, DBG, "ipc_dev(%s) ipc_node_id(%d) rx_ch(%d) tx_ch(%d)",\
			eemcs_ipc_inst.ipc_node[i].dev_name, eemcs_ipc_inst.ipc_node[i].ipc_node_id,\
            eemcs_ipc_inst.ccci_ch.rx, eemcs_ipc_inst.ccci_ch.tx);

    }
	
    #ifdef ENABLE_CONN_COEX_MSG
    {
        CONN_MD_BRIDGE_OPS eemcs_ipc_conn_ops = {.rx_cb = eemcs_ipc_kern_write};
        mtk_conn_md_bridge_reg(MD_MOD_EL1, &eemcs_ipc_conn_ops);
        eemcs_ipc_kern_open(AP_IPC_WMT);        
    }
    #endif
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
    
cdev_add_fail:
    cdev_del(eemcs_ipc_inst.eemcs_ipcdev);
cdev_alloc_fail:
    unregister_chrdev_region(MKDEV(EEMCS_IPC_MAJOR,EEMCS_IPC_MINOR_BASE), EEMCS_IPCD_MAX_NUM);
register_chrdev_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;    
}

void eemcs_ipc_exit(void){
    KAL_INT32 i=0;

    DEBUG_LOG_FUNCTION_ENTRY;
	
    #ifdef ENABLE_CONN_COEX_MSG
    mtk_conn_md_bridge_unreg(MD_MOD_EL1);
    #endif
    
    for(i=0 ; i<EEMCS_IPCD_MAX_NUM; i++)
    {
		device_destroy(eemcs_ipc_inst.dev_class, MKDEV(EEMCS_IPC_MAJOR,EEMCS_IPC_MINOR_BASE+i));
        skb_queue_purge(&eemcs_ipc_inst.ipc_node[i].rx_skb_list);
    }

    if(eemcs_ipc_inst.dev_class)
    {
        DBGLOG(IPCD,DBG,"dev_class unregister ");
        release_ipc_class(eemcs_ipc_inst.dev_class);
    }
    
    if(eemcs_ipc_inst.eemcs_ipcdev)
    {
        DBGLOG(IPCD, DBG, "eemcs_ipcdev delete");
	    cdev_del(eemcs_ipc_inst.eemcs_ipcdev);
    }
	unregister_chrdev_region(MKDEV(EEMCS_IPC_MAJOR,EEMCS_IPC_MINOR_BASE), EEMCS_IPCD_MAX_NUM);  
    eemcs_state_callback_unregister(&eemcs_ipc_state_callback);
    DEBUG_LOG_FUNCTION_LEAVE;
    return;
}



#ifdef _EEMCS_IPCD_LB_UT_
KAL_UINT32 ipcdut_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data) {	
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(IPCD,DBG, "[CHAR_UT]CCCI channel (%d) register callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}
KAL_UINT32 ipcdut_unregister_callback(CCCI_CHANNEL_T chn) {
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(IPCD,DBG, "[CHAR_UT]CCCI channel (%d) UNregister callback", chn);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

inline KAL_UINT32 ipcdut_UL_write_room_alloc(CCCI_CHANNEL_T chn)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return 1;
}

inline KAL_INT32 ipcdut_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb)
{
	CCCI_BUFF_T *pccci_h = (CCCI_BUFF_T *)skb->data;
    KAL_UINT8 port_id;
    KAL_UINT32 tx_ch, rx_ch;
    
	DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(IPCD,DBG, "[CHAR_UT]CCCI channel (%d) ccci_write CCCI_H(%#X)(%#X)(%#X)(%#X)",\
        chn, pccci_h->data[0], pccci_h->data[1], pccci_h->channel, pccci_h->reserved);

#ifdef _EEMCS_IPCD_LB_UT_
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
        rx_ch =  eemcs_ipc_inst.ccci_ch.rx;
        pccci_h->channel = rx_ch;
        
        DBGLOG(IPCD,DBG, "[CHAR_UT]=========PORT(%d) tx_ch(%d) LB to rx_ch(%d)",\
            port_id, tx_ch, rx_ch);

        eemcs_ipc_rx_callback(new_skb, 0);
    }
#endif
    dev_kfree_skb(skb);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}
#endif //_EEMCS_CDEV_LB_UT_

