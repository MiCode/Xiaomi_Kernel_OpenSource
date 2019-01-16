/*****************************************************************************
 *
 * Filename:
 * ---------
 *   eemcs_rpc.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   MT6582 CCCI RPC
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

#include <mach/mt_sec_export.h>
#include "eemcs_kal.h"
#include "eemcs_ccci.h"
#include "eemcs_debug.h"
#include "eemcs_char.h"
#include "eemcs_file_ops.h"
#include "eemcs_state.h"
#include "eemcs_rpc.h"
#include "eemcs_boot.h"

//static RPC_BUF			*rpc_buf_vir;
//static unsigned int     rpc_buf_phy;
//static unsigned int     rpc_buf_len;
static spinlock_t       rpc_fifo_lock = __SPIN_LOCK_UNLOCKED(rpc_fifo_lock);
//static spinlock_t       rpc_fifo_lock = SPIN_LOCK_UNLOCKED;
static struct kfifo     rpc_fifo;
//static DECLARE_MUTEX(sej_sem);
struct work_struct      rpc_work;

#if 0
/* ccci_rpc_ch channel config*/
CCCI_RPC_T ccci_rpc_ch = 
{ 
    CH_RPC_TX,  
    CH_RPC_RX,  

    ccci_write_skb,
    ccci_init_gpdior,
    ccci_deinit,
	ccci_polling_io,
    kal_query_systemInit,
    IPC_RPC_CCCI_WRITE_ONLY_UT //ut flag code
};
#endif

//static bool rpc_write(int buf_idx,  unsigned int pkt_num)
static bool rpc_write(RPC_PKT* pkt_src, pIPC_RPC_StreamBuffer_T rpc_data_buffer)
{
	int ret = KAL_FAIL;
	//CCCI_BUFF_T     stream = { {0}, };
	//RPC_BUF	*rpc_buf_tmp = NULL;
	unsigned char	*pdata = NULL;
	unsigned int	skb_len = 0;
	unsigned int 	data_len = 0;
	unsigned int 	i = 0;
	unsigned int	AlignLength = 0;
	unsigned int	retry = 1000;
	struct sk_buff *new_skb;
	CCCI_BUFF_T * ccci_header;
	KAL_UINT32 alloc_time = 0, curr_time = 0;

	DBGLOG(RPCD, DBG, "[RPC] enter rpc_write");

	//check RX queue is available
	if(retry>0){
		while (ccci_ch_write_space_alloc(CH_RPC_TX)==0){
			retry --;
		}
		if(retry == 0){
			DBGLOG(RPCD, ERR, "[RPC] ccci_ch_write_space_alloc return 0");
			retry = 1000;
	        //goto _Exit;
		}	
	}
	
	DBGLOG(RPCD, DBG, "[RPC] rpc_buf: opid=0x%08X, num_para=0x%08X", 
		rpc_data_buffer->rpc_opid, rpc_data_buffer->num_para);

	//calculate data size with data 4bytes alignment
	data_len = 0;
	data_len +=  2*sizeof(unsigned int);  //opid, argv 
	for(i = 0; i < rpc_data_buffer->num_para; i++)
	{
		if( data_len > IPC_RPC_MAX_BUF_SIZE)
		{
			DBGLOG(RPCD, ERR, "[RPC] Stream buffer full!");
			goto _Exit;
		}
		data_len += sizeof(unsigned int);				
		// 4  byte aligned
		AlignLength = ((pkt_src[i].len + 3) >> 2) << 2;
		data_len += AlignLength;
	}

	skb_len = 0;
	skb_len += (CCCI_RPC_HEADER_ROOM + data_len);

#if 0	
	while(NULL == (new_skb = dev_alloc_skb(skb_len)))
	{        
		DBGLOG(RPCD,ERR,"alloc tx memory fail");      
	}
#endif
	new_skb = ccci_rpc_mem_alloc(skb_len, GFP_ATOMIC);
	if (NULL == new_skb) {
		DBGLOG(RPCD, INF, "[TX]rpc alloc skb with wait");
		alloc_time = jiffies;
		new_skb = ccci_rpc_mem_alloc(skb_len, GFP_KERNEL);
		if (NULL == new_skb) {
			ret = -ENOMEM;
			DBGLOG(RPCD, ERR, "[TX]rpc alloc skb with wait fail");
			goto _Exit; 
		}
		curr_time = jiffies;
		if ((curr_time - alloc_time) >= 1) {
			DBGLOG(RPCD, ERR, "[TX]rpc alloc skb with wait delay: time=%dms", 10*(curr_time - alloc_time));
		}
	}
	
    /* reserve SDIO_H and CCCI header room */
	#ifdef CCCI_SDIO_HEAD
	skb_reserve(new_skb, sizeof(SDIO_H));
	#endif
	
	ccci_header = (CCCI_BUFF_T *)skb_put(new_skb, sizeof(CCCI_BUFF_T)) ;
	pdata = (kal_uint8 *)skb_put(new_skb, data_len);
	
	#ifdef CCCI_SDIO_HEAD
	memset(new_skb->data, 0, skb_len - sizeof(SDIO_H));
	#else
	memset(new_skb->data, 0, skb_len);
	#endif
	
	(*(kal_uint32 *)pdata) = rpc_data_buffer->rpc_opid;
	pdata += sizeof(unsigned int);
	(*(kal_uint32 *)pdata) = rpc_data_buffer->num_para;
	pdata += sizeof(unsigned int); //point to LV[0]

	for(i = 0; i < rpc_data_buffer->num_para; i++)
	{
		*((unsigned int*)pdata) = pkt_src[i].len;
		pdata += sizeof(unsigned int);
		// 4  byte aligned
		AlignLength = ((pkt_src[i].len + 3) >> 2) << 2;		
		if(pdata != pkt_src[i].buf)
			memcpy(pdata, pkt_src[i].buf, pkt_src[i].len);
		else
  		    DBGLOG(RPCD, DBG, "[RPC] same addr, no copy");
		pdata += AlignLength;
	}

#if 0 
data_len = 0;
data_len += (sizeof(CCCI_BUFF_T) + 2*sizeof(unsigned int)); 



for(i = 0; i < rpc_data_buffer->num_para; i++)
{
	if( data_len > IPC_RPC_MAX_BUF_SIZE)
	{
		DBGLOG(RPCD, ERR, "[RPC] Stream buffer full!");
		goto _Exit;
	}
	
	*((unsigned int*)pdata) = pkt_src[i].len;
	pdata += sizeof(unsigned int);
	data_len += sizeof(unsigned int);
		
	// 4  byte aligned
	AlignLength = ((pkt_src[i].len + 3) >> 2) << 2;
	data_len += AlignLength;
		
	if(pdata != pkt_src[i].buf)
		memcpy(pdata, pkt_src[i].buf, pkt_src[i].len);
	else
		DBGLOG(RPCD, DBG, "[RPC] same addr, no copy");
				
	pdata += AlignLength;
}


#endif

	ccci_header->data[1]	= sizeof(CCCI_BUFF_T) + data_len;
	ccci_header->reserved	= rpc_data_buffer->ccci_header.reserved;
	ccci_header->channel	= CH_RPC_TX;

	DBGLOG(RPCD, DBG, "ccci header: data[0]=0x%08X, data[1]=0x%08X, ch=%02d, reserved=0x%08X", 
		ccci_header->data[0],ccci_header->data[1] ,ccci_header->channel,ccci_header->reserved);	 

	DBGLOG(RPCD, DBG, "skb data address = 0x%p", new_skb->data);	
	DBGLOG(RPCD, DBG, "skb data length  = 0x%08X", new_skb->len);
	DBGLOG(RPCD, DBG, "skb tail address = 0x%p", new_skb->tail);
	DBGLOG(RPCD, DBG, "skb end address  = 0x%p", new_skb->end);

	ret = ccci_rpc_ch_write_desc_to_q(ccci_header->channel, new_skb);
	
	DBGLOG(RPCD, DBG, "ready to leave rpc_write");

	if(ret != 	KAL_SUCCESS)
	{
		DBGLOG(RPCD, ERR, "fail send msg <%d>!!!", ret);;
		dev_kfree_skb(new_skb);
		return ret;
	}
			
_Exit:
	return ret;
}

static bool get_pkt_info(unsigned int* pktnum, RPC_PKT* pkt_info, struct sk_buff *skb)
{
	unsigned int pkt_num;
	unsigned int buff_size = 0;
	unsigned int i = 0;
    kal_uint8 * ptr;
	pIPC_RPC_StreamBuffer_T rpc_data_buffer;
	DBGLOG(RPCD, DBG, "[RPC]  enter get_pkt_info");

	rpc_data_buffer = (pIPC_RPC_StreamBuffer_T)skb->data;
	pkt_num = rpc_data_buffer->num_para;
	DBGLOG(RPCD, DBG, "[RPC]  number of LV pair = 0x%08X", pkt_num);
	if(pkt_num > IPC_RPC_MAX_ARG_NUM)
	{
		DBGLOG(RPCD, ERR, "[RPC] pkt_num > IPC_RPC_MAX_ARG_NUM");
		return false;
	}

#ifdef _EEMCS_RPC_UT	

	DBGLOG(RPCD, DBG, "[RPC] rpc_buf: ccci header: data[0]=0x%08X, data[1]=0x%08X, channel=0x%08X, reserved=0x%08X", 
		rpc_data_buffer->ccci_header.data[0],rpc_data_buffer->ccci_header.data[1] ,rpc_data_buffer->ccci_header.channel,rpc_data_buffer->ccci_header.reserved);
	DBGLOG(RPCD, DBG, "[RPC] rpc_buf: opid=0x%08X, num_para=0x%08X", 
		rpc_data_buffer->rpc_opid, rpc_data_buffer->num_para);

	ptr = (kal_uint8*)(rpc_data_buffer) + sizeof(CCCI_BUFF_T) +2*sizeof(kal_uint32);
	//- point to LV[0]	
	//ptr = (kal_uint8*)(rpc_data_buffer) + sizeof(CCCI_BUFF_T) +2*sizeof(kal_uint32);
	DBGLOG(RPCD, DBG, "[RPC] LV[0]	len= 0x%08X", *((kal_uint32 *)ptr));
	ptr += sizeof(kal_uint32);
	DBGLOG(RPCD, DBG, "[RPC] LV[0]	value= 0x%08X", *((kal_uint32 *)ptr));
	//- point to LV[1]
	ptr += sizeof(kal_uint32);
	DBGLOG(RPCD, DBG, "[RPC] LV[1]	len= 0x%08X", *((kal_uint32 *)ptr));
	ptr += sizeof(kal_uint32);
	DBGLOG(RPCD, DBG, "[RPC] LV[1]	value= 0x%08X", *((kal_uint32 *)ptr));	
#endif

	ptr = (kal_uint8*)(rpc_data_buffer) + sizeof(CCCI_BUFF_T) +2*sizeof(kal_uint32);
	
	for(i = 0; i < pkt_num; i++)
	{
		pkt_info[i].len = *((unsigned int*)(ptr + buff_size));
		buff_size += sizeof(kal_uint32);
		pkt_info[i].buf = (ptr + buff_size);
		// 4 byte alignment
		buff_size += ((pkt_info[i].len+3)>>2)<<2;

		//DBGLOG(RPCD, DBG, "[RPC] LV[%d]  len= 0x%08X, value= 0x%08X", i, pkt_info[i].len, (unsigned int)pkt_info[i].buf);
		DBGLOG(RPCD, DBG, "[RPC] LV[%d]  len= 0x%08X, value= 0x%08X", i, pkt_info[i].len, *((unsigned int *)pkt_info[i].buf));
    	DBGLOG(RPCD, DBG, "[RPC] current buffer size = 0x%08X", buff_size);
	}

	if(buff_size > IPC_RPC_MAX_BUF_SIZE)
	{
		DBGLOG(RPCD, ERR, "over flow, pdata = %p, idx = 0x%08X, max = %p", ptr, buff_size, ptr + IPC_RPC_MAX_BUF_SIZE);	
		return false;
	}
	*pktnum = pkt_num;

	DBGLOG(RPCD, DBG, "[RPC]  leave get_pkt_info");

	return true;

#if 0 
	unsigned int pkt_num = *((unsigned int*)pdata);
	unsigned int idx = 0;
	unsigned int i = 0;

	DBGLOG(RPCD, DBG, "[RPC]  enter get_pkt_info");

	DBGLOG(RPCD, DBG, "[RPC]  number of LV pair = 0x%08X", pkt_num);
	if(pkt_num > IPC_RPC_MAX_ARG_NUM)
	{
		DBGLOG(RPCD, ERR, "[RPC] pkt_num > IPC_RPC_MAX_ARG_NUM");
		return false;
		}
	
	//idx = sizeof(unsigned int);
	idx = sizeof(kal_uint32);
	for(i = 0; i < pkt_num; i++)
	{
		pkt_info[i].len = *((unsigned int*)(pdata + idx));
		//idx += sizeof(unsigned int);
		idx += sizeof(kal_uint32);
		pkt_info[i].buf = (pdata + idx);
		DBGLOG(RPCD, DBG, "[RPC] LV[%d]  len= 0x%08X, value= 0x%08X", i, pkt_info[i].len, (unsigned int)pkt_info[i].buf);
		// 4 byte alignment
		idx += ((pkt_info[i].len+3)>>2)<<2;
	}
	
	if(idx > IPC_RPC_MAX_BUF_SIZE)
	{
		DBGLOG(RPCD, ERR, "over flow, pdata = %p, idx = 0x%08X, max = %p", pdata, idx, pdata + IPC_RPC_MAX_BUF_SIZE);	
		return false;
	}
	*pktnum = pkt_num;

	DBGLOG(RPCD, DBG, "[RPC]  leave get_pkt_info");

	return true;
#endif	
}


//in this function, all input parameters are retreived to be execute and 
//generate return code and output parameters 
//static void ccci_rpc_work(struct work_struct *work  __always_unused)
void eemcs_rpc_work_helper(int *p_pkt_num, RPC_PKT pkt[], pIPC_RPC_StreamBuffer_T p_rpc_buf);
static void eemcs_rpc_work(struct work_struct *work)
{
	int pkt_num = 0;	// index of LV pair	
	int ret_val = 0;    // return code : stored in the first v entry
	unsigned int buf_idx = 0;  // index of md rpc gpd
	RPC_PKT pkt[IPC_RPC_MAX_ARG_NUM] = { {0}, }; //rpc packet : stored  LV pair  (input and output)
	pIPC_RPC_StreamBuffer_T rpc_data_buffer;
	struct sk_buff *skb;
	int i=0;
    kal_uint32 it_return_data = it_ret_val; 

#ifdef _EEMCS_RPC_UT	
	kal_uint8* ptr;
	CCCI_BUFF_T * p_cccih;
    kal_uint32 ut_return_data = ut_ret_val; 
#endif

	DBGLOG(RPCD, DBG, "[RPC] enter ccci_rpc_work");
	
//	    skb = dev_alloc_skb( IPC_RPC_MAX_BUF_SIZE);
//		memset(skb->data, 0, IPC_RPC_MAX_BUF_SIZE);
	
	while(kfifo_out(&rpc_fifo, &skb, sizeof(unsigned int)))
	{
	
    	rpc_data_buffer = (pIPC_RPC_StreamBuffer_T)skb->data;

        DBGLOG(RPCD, DBG, "[RPC] rpc_buf: ccci header: data[0]=0x%08X, data[1]=0x%08X, channel=0x%08X, reserved=0x%08X", 
			rpc_data_buffer->ccci_header.data[0],rpc_data_buffer->ccci_header.data[1] ,rpc_data_buffer->ccci_header.channel,rpc_data_buffer->ccci_header.reserved);
        DBGLOG(RPCD, DBG, "[RPC] rpc_buf: opid=0x%08X, num_para=0x%08X", 
			rpc_data_buffer->rpc_opid, rpc_data_buffer->num_para);

#ifdef _EEMCS_RPC_UT
		p_cccih = (CCCI_BUFF_T *)skb->data;
        DBGLOG(RPCD, DBG, "[RPC] ccci header: data[0]=0x%08X, data[1]=0x%08X, channel=0x%08X, reserved=0x%08X", 
			p_cccih->data[0],p_cccih->data[1] ,p_cccih->channel,p_cccih->reserved);
		//- point to LV[0]	
		ptr = (kal_uint8*)(rpc_data_buffer) + sizeof(CCCI_BUFF_T) +2*sizeof(kal_uint32);
		DBGLOG(RPCD, DBG, "[RPC] LV[0]	len= 0x%08X", *((kal_uint32 *)ptr));
		ptr += sizeof(kal_uint32);
		DBGLOG(RPCD, DBG, "[RPC] LV[0]	value= 0x%08X", *((kal_uint32 *)ptr));
		//- point to LV[1]
		ptr += sizeof(kal_uint32);
		DBGLOG(RPCD, DBG, "[RPC] LV[1]	len= 0x%08X", *((kal_uint32 *)ptr));
		ptr += sizeof(kal_uint32);
		DBGLOG(RPCD, DBG, "[RPC] LV[1]	value= 0x%08X", *((kal_uint32 *)ptr));
#endif

		if( rpc_data_buffer->ccci_header.reserved < 0 || rpc_data_buffer->ccci_header.reserved > IPC_RPC_REQ_BUFFER_NUM)
		{
			DBGLOG(RPCD, ERR, "[RPC] invalid idx %d\n", rpc_data_buffer->ccci_header.reserved);
			ret_val = FS_PARAM_ERROR;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &ret_val;
			goto _Next;
		}	
		buf_idx = rpc_data_buffer->ccci_header.reserved;
		DBGLOG(RPCD, DBG, "[RPC] rpc_buf: index = 0x%08X\n", rpc_data_buffer->ccci_header.reserved);

		pkt_num = 0;
		memset(pkt, 0x00, sizeof(RPC_PKT)*IPC_RPC_MAX_ARG_NUM);

        //get LV pair into pkt, if it can't not find parameter in pkt return false        
		//if(!get_pkt_info(&pkt_num, pkt, (skb->data+5*(sizeof(unsigned int))))) //(skb->data+5*(sizeof(unsigned int))) point to # parameters
		if(!get_pkt_info(&pkt_num, pkt, skb )) //(skb->data+5*(sizeof(unsigned int))) point to # parameters
		{
			DBGLOG(RPCD, ERR, "[RPC] Fail to get packet info");
			ret_val = FS_PARAM_ERROR;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &ret_val;
			goto _Next;
		}
		//current pkt stored input parameters from md 
		//in below switch, according to op id, parameters in pkt will be used to generate output parameters and be stored in pkt
		DBGLOG(RPCD, DBG, "[RPC] RPC Operation ID (0x%08X)",rpc_data_buffer->rpc_opid);
		switch(rpc_data_buffer->rpc_opid)  
		{     			  

#ifdef _EEMCS_RPC_UT
		case IPC_RPC_UT_OP:
			{   
				DBGLOG(RPCD, DBG, "[RPCUT] enter UT operation in ccci_rpc_work");
				//exam input parameters in pkt
                for(i=0; i<pkt_num ; i++){
					KAL_ASSERT(pkt[i].len == sizeof(unsigned int));
					KAL_ASSERT(*((unsigned int *)pkt[i].buf) == 0xbeef);									
				}

                //=======================================================================
                //RPC user can insert function call in specific op case to use the input parameters in pkt and refill the output parameters in pkt
                //=======================================================================
				
				//prepare output parameters
				ret_val = RPC_UT_SUCCESS;
				pkt_num = 0;
				DBGLOG(RPCD, DBG, "[RPCUT] prepare output parameters");
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &ret_val;
				DBGLOG(RPCD, DBG, "[RPCUT] LV[%d]  len= 0x%08X, value= 0x%08X", 0, pkt[0].len, *((unsigned int *)pkt[0].buf));
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &ut_return_data;
				DBGLOG(RPCD, DBG, "[RPCUT] LV[%d]  len= 0x%08X, value= 0x%08X", 1, pkt[1].len, *((unsigned int *)pkt[1].buf));
				break;
			}
#endif

		case IPC_RPC_IT_OP:
			{
				DBGLOG(RPCD, DBG, "[RPCIT] enter IT operation in ccci_rpc_work");
				//exam input parameters in pkt
                for(i=0; i<pkt_num ; i++){
					KAL_ASSERT(pkt[i].len == sizeof(unsigned int));
					KAL_ASSERT(*((unsigned int *)pkt[i].buf) == 0xbeef);									
				}

                //=======================================================================
                //RPC user can insert function call in specific op case to use the input parameters in pkt and refill the output parameters in pkt
                //=======================================================================
				
				//prepare output parameters
				ret_val = RPC_IT_SUCCESS;
				pkt_num = 0;
				DBGLOG(RPCD, DBG, "[RPCIT] prepare output parameters");
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &ret_val;
				DBGLOG(RPCD, DBG, "[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X", 0, pkt[0].len, *((unsigned int *)pkt[0].buf));
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &it_return_data;
				DBGLOG(RPCD, DBG, "[RPCIT] LV[%d]  len= 0x%08X, value= 0x%08X", 1, pkt[1].len, *((unsigned int *)pkt[1].buf));
				break;				
			}

		default:
			{
                DBGLOG(RPCD, DBG, "[RPC] PKT_num=%d", pkt_num);
                eemcs_rpc_work_helper(&pkt_num, pkt, rpc_data_buffer);  // !!!!! Make more meaningful
				break;
			}
		}
_Next:      		
		rpc_data_buffer->ccci_header.channel = CH_RPC_TX;
		rpc_data_buffer->rpc_opid = IPC_RPC_API_RESP_ID | rpc_data_buffer->rpc_opid;
		rpc_data_buffer->num_para = pkt_num;
		if(rpc_write(pkt, rpc_data_buffer) != KAL_SUCCESS)
		{
			DBGLOG(RPCD, ERR, "[RPC] fail to write packet!!");
            dev_kfree_skb(skb);
			return ;
		}	
        dev_kfree_skb(skb);
	} 
	DBGLOG(RPCD, DBG, "[RPC] leave ccci_rpc_work");

}

KAL_INT32 eemcs_rpc_callback(struct sk_buff *skb, KAL_UINT32 private_data)
{
	CCCI_BUFF_T *p_cccih = NULL;
	DBGLOG(RPCD, DBG, "[RPC] enter ccci_rpc_callback");

	if (skb){
		p_cccih = (CCCI_BUFF_T *)skb->data;
		DBGLOG(RPCD, DBG, "RX callback,ccci header: %08X, %08X, %08X, %08X", p_cccih->data[0], p_cccih->data[1], p_cccih->channel, p_cccih->reserved);
	}
	if (p_cccih->channel==CH_RPC_TX){
		DBGLOG(RPCD, ERR, "[Wrong CH for recv.");
	} 
	spin_lock_bh(&rpc_fifo_lock);
	DBGLOG(RPCD, DBG, "ready to put skb into FIFO");
	//kfifo_in(&rpc_fifo, &buff->reserved, sizeof(unsigned int));
	kfifo_in(&rpc_fifo, &skb, sizeof(unsigned int));
	DBGLOG(RPCD, DBG, "after put skb into FIFO");
	spin_unlock_bh(&rpc_fifo_lock);
	DBGLOG(RPCD, DBG, "ready to schedule rpc_work");
   	schedule_work(&rpc_work);
	
	DBGLOG(RPCD, DBG, "leave ccci_rpc_callback");
	return KAL_SUCCESS;
}


int eemcs_rpc_mod_init(void)
{ 
    int ret;	
    DBGLOG(RPCD, DBG, "eemcs_rpc_mod_init");
	
    ret=kfifo_alloc(&rpc_fifo,sizeof(unsigned) * IPC_RPC_REQ_BUFFER_NUM, GFP_KERNEL);
    if (ret)
    {
        DBGLOG(RPCD, ERR, "kfifo_alloc fail: %d", ret);
        return ret;
    }
#if 0
	rpc_work_queue = create_workqueue(WORK_QUE_NAME);
	if(rpc_work_queue == NULL)
	{
        printk(KERN_ERR "Fail create rpc_work_queue!!\n");
        return -EFAULT;
	}
#endif
    INIT_WORK(&rpc_work, eemcs_rpc_work);

    // modem related channel registration.
    //KAL_ASSERT(ccci_rpc_setup((int*)&rpc_buf_vir, &rpc_buf_phy, &rpc_buf_len) == KAL_SUCCESS);
    //CCCI_RPC_MSG("init %p, 0x%08X, 0x%08X\n", rpc_buf_vir, rpc_buf_phy, rpc_buf_len);
  	//KAL_ASSERT(rpc_buf_vir != NULL);
    //KAL_ASSERT(rpc_buf_len != 0);
    KAL_ASSERT(ccci_ch_register((CCCI_CHANNEL_T)CH_RPC_RX, eemcs_rpc_callback, 0) == KAL_SUCCESS);
	
	DBGLOG(RPCD, DBG, "ccci_ch_register OK");

    return 0;
}

void eemcs_rpc_exit(void)
{
	DBGLOG(RPCD, DBG, "eemcs_rpc_exit");
	
#if 0
    if(cancel_work_sync(&rpc_work)< 0)
    {
		printk("ccci_rpc: Cancel rpc work fail!\n");
    }
	//destroy_workqueue(rpc_work_queue);
#endif

	kfifo_free(&rpc_fifo);
	DBGLOG(RPCD, DBG, "eemcs_rpc_exit OK");	
}
/*********************************************************************************/
/* ccci rpc helper function for RPC Section                                                                             */
/*                                                                                                                                   */
/*********************************************************************************/
unsigned int res_len = 0; //<<KE, need check this
#if 1
//-------------feature enable/disable configure----------------//
//#define FEATURE_GET_TD_EINT_NUM
//#define FEATURE_GET_MD_GPIO_NUM	
//#define FEATURE_GET_MD_ADC_NUM
//#define FEATURE_GET_MD_EINT_ATTR
//-------------------------------------------------------------//


#include <mach/mt_gpio_affix.h>
#include <mach/eint.h>
//#include <mach/mt_reg_base.h>
//#include <mach/battery_common.h>

#if defined (FEATURE_GET_MD_ADC_NUM)
extern int IMM_get_adc_channel_num(char *channel_name, int len);
#endif

#if defined (FEATURE_GET_DRAM_TYPE_CLK)
extern int get_dram_info(int *clk, int *type);
#endif

#if defined (FEATURE_GET_MD_EINT_ATTR)
extern int get_eint_attribute(char *name, unsigned int name_len, unsigned int type, char * result, unsigned int *len);
#endif

int get_td_eint_info(int md_id, char * eint_name, unsigned int len)
{
	#if defined (FEATURE_GET_TD_EINT_NUM)
	return get_td_eint_num(eint_name, len);
	
	#else
	return -1;
	#endif
}

int get_md_gpio_info(int md_id, char *gpio_name, unsigned int len)
{
	#if defined (FEATURE_GET_MD_GPIO_NUM)
	return mt_get_md_gpio(gpio_name, len);
	
	#else
	return -1;
	#endif
}

int get_md_adc_info(int md_id, char *adc_name, unsigned int len)
{
	#if defined (FEATURE_GET_MD_ADC_NUM)
	return IMM_get_adc_channel_num(adc_name, len);
	
	#else
	return -1;
	#endif
}
int get_eint_attr(char *name, unsigned int name_len, unsigned int type, char * result, unsigned int *len)
{
	#if defined (FEATURE_GET_MD_EINT_ATTR)
	return get_eint_attribute(name, name_len, type, result, len);

	#else
	return -1;
	#endif
}
	
int get_dram_type_clk(int *clk, int *type)
{
	#if defined (FEATURE_GET_DRAM_TYPE_CLK)
	return get_dram_info(clk, type);

    #else
	return -1;
	#endif
}
#else
//#include <mach/mtk_ccci_helper.h>
int get_td_eint_info(int md_id, char * eint_name, unsigned int len){return 0;}
int get_md_gpio_info(int md_id, char *gpio_name, unsigned int len){return 0;}
int get_md_adc_info(int md_id, char *adc_name, unsigned int len){return 0;}
int get_dram_type_clk(int *clk, int *type){return 0;}
int get_eint_attr(char *name, unsigned int name_len, unsigned int type, char * result, unsigned int *len){return 0;}
#endif

void eemcs_rpc_work_helper(int *p_pkt_num, RPC_PKT pkt[], pIPC_RPC_StreamBuffer_T p_rpc_buf)
{
	// tmp_data[] is used to make sure memory address is valid after this function return
	int pkt_num = *p_pkt_num;
    int md_id = 0;
    unsigned int *tmp_data = (unsigned int *)p_rpc_buf->buffer;

	DBGLOG(RPCD, TRA, "[RPCD] eemcs_rpc_work_helper++\n");
	

	switch(p_rpc_buf->rpc_opid)
	{

        case IPC_RPC_CPSVC_SECURE_ALGO_OP:
		{
			unsigned char Direction = 0;
			unsigned int  ContentAddr = 0;
			unsigned int  ContentLen = 0;
			sed_t CustomSeed = SED_INITIALIZER;
			unsigned char *ResText __always_unused= NULL;
			unsigned char *RawText __always_unused= NULL;
			unsigned int i __always_unused= 0;

			if(pkt_num < 4)
			{
				DBGLOG(RPCD, ERR, "[RPCD] invalid pkt_num %d for RPC_SECURE_ALGO_OP!\n", pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				break;
			}

			Direction = *(unsigned char*)pkt[0].buf;
			ContentAddr = (unsigned int)pkt[1].buf;				
			DBGLOG(RPCD, TRA, "[RPCD] RPC_SECURE_ALGO_OP: Content_Addr = 0x%08X, RPC_Base = 0x%08X, RPC_Len = 0x%08X\n", 
				ContentAddr, (unsigned int)p_rpc_buf, sizeof(RPC_BUF)+IPC_RPC_MAX_BUF_SIZE);
			if(ContentAddr < (unsigned int)p_rpc_buf || 
								ContentAddr > ((unsigned int)p_rpc_buf + sizeof(RPC_BUF)+IPC_RPC_MAX_BUF_SIZE))
			{
				DBGLOG(RPCD, ERR, "[RPCD] invalid ContentAdddr[0x%08X] for RPC_SECURE_ALGO_OP!\n", ContentAddr);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				break;
			}
			ContentLen = *(unsigned int*)pkt[2].buf;
			//	CustomSeed = *(sed_t*)pkt[3].buf;
			WARN_ON(sizeof(CustomSeed.sed)<pkt[3].len);
			memcpy(CustomSeed.sed,pkt[3].buf,pkt[3].len);

#ifdef ENCRYPT_DEBUG
			{
			unsigned char log_buf[128];

			if(Direction == TRUE)
				DBGLOG(RPCD, TRA, "[RPCD] HACC_S: EnCrypt_src:\n");
			else
				DBGLOG(RPCD, TRA, "[RPCD] HACC_S: DeCrypt_src:\n");
			for(i = 0; i < ContentLen; i++)
			{
				if(i % 16 == 0){
					if(i!=0){
						DBGLOG(RPCD, TRA, "[RPCD] %s\n", log_buf);
					}
					curr = 0;
					curr += snprintf(log_buf, sizeof(log_buf)-curr, "HACC_S: ");
				}
				//CCCI_MSG("0x%02X ", *(unsigned char*)(ContentAddr+i));
				curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "0x%02X ", *(unsigned char*)(ContentAddr+i));					
				//sleep(1);
			}
			DBGLOG(RPCD, TRA, "[RPCD] %s\n", log_buf);
				
			RawText = kmalloc(ContentLen, GFP_KERNEL);
			if(RawText == NULL)
				DBGLOG(RPCD, ERR, "[RPCD] Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
			else
				memcpy(RawText, (unsigned char*)ContentAddr, ContentLen);
			}
#endif

			ResText = kmalloc(ContentLen, GFP_KERNEL);
			if(ResText == NULL)
			{
				DBGLOG(RPCD, ERR, "[RPCD] Fail alloc Mem for RPC_SECURE_ALGO_OP!\n");
				tmp_data[0] = FS_PARAM_ERROR;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				break;
			}

#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
			if(!masp_secure_algo_init())
			{
				DBGLOG(RPCD, ERR, "[RPCD] masp_secure_algo_init fail!\n");
				ASSERT(0);
			}
			
			DBGLOG(RPCD, TRA, "[RPCD] RPC_SECURE_ALGO_OP: Dir=0x%08X, Addr=0x%08X, Len=0x%08X, Seed=0x%016llX\n", 
					Direction, ContentAddr, ContentLen, *(long long *)CustomSeed.sed);
			masp_secure_algo(Direction, ContentAddr, ContentLen, CustomSeed.sed, ResText);

			if(!masp_secure_algo_deinit())
				DBGLOG(RPCD, ERR, "[RPCD] masp_secure_algo_deinit fail!\n");
#endif

			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &tmp_data[0];
			pkt[pkt_num].len = ContentLen;	
			
#if (defined(ENABLE_MD_IMG_SECURITY_FEATURE) && defined(CONFIG_MTK_SEC_MODEM_NVRAM_ANTI_CLONE))
			memcpy(pkt[pkt_num++].buf, ResText, ContentLen);
			DBGLOG(RPCD, TRA, "[RPCD] RPC_Secure memory copy OK: %d!", ContentLen);
#else
			memcpy(pkt[pkt_num++].buf, (void *)ContentAddr, ContentLen);
			DBGLOG(RPCD, TRA, "[RPCD] RPC_NORMAL memory copy OK: %d!", ContentLen);
#endif
			
#ifdef ENCRYPT_DEBUG
			if(Direction == TRUE)
				DBGLOG(RPCD, TRA, "[RPCD] HACC_D: EnCrypt_dst:\n");
			else
				DBGLOG(RPCD, TRA, "[RPCD] HACC_D: DeCrypt_dst:\n");
			for(i = 0; i < ContentLen; i++)
			{
				if(i % 16 == 0){
					if(i!=0){
						CCCI_RPC_MSG(md_id, "%s\n", log_buf);
					}
					curr = 0;
					curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "HACC_D: ");
				}
				//CCCI_MSG("%02X ", *(ResText+i));
				curr += snprintf(&log_buf[curr], sizeof(log_buf)-curr, "0x%02X ", *(ResText+i));
				//sleep(1);
			}
			
			DBGLOG(RPCD, TRA, "[RPCD] %s\n", log_buf);

			if(RawText)
				kfree(RawText);
#endif

			kfree(ResText);
			break;
		}

#ifdef ENABLE_MD_SECURITY_RO_FEATURE
		case IPC_RPC_GET_SECRO_OP:
		{
			unsigned char *addr = NULL;
			unsigned int img_len = 0;
			unsigned int img_len_bak = 0;
			unsigned int blk_sz = 0;
			unsigned int tmp = 1;
			unsigned int cnt = 0;
			unsigned int req_len = 0;	
		
			if(pkt_num != 1) {
				DBGLOG(RPCD, ERR, "RPC_GET_SECRO_OP: invalid parameter: pkt_num=%d \n", pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				tmp_data[1] = img_len;
				pkt[pkt_num++].buf = (void*) &tmp_data[1];
				break;
			}
				
			req_len = *(unsigned int*)(pkt[0].buf);
			if(masp_secro_en()) {
				//if(md_id == MD_SYS1) {
				//	img_len = masp_secro_md_len(SECRO_MD1);
				//} else {
				//	img_len = masp_secro_md_len(SECRO_MD2);
				//}
				char md_post_fix[EXT_MD_POST_FIX_LEN] = {0};
				eemcs_boot_get_ext_md_post_fix(md_post_fix);
				img_len = masp_secro_md_len(md_post_fix);

				if((img_len > IPC_RPC_MAX_BUF_SIZE) || (req_len > IPC_RPC_MAX_BUF_SIZE)) {
					pkt_num = 0;
					tmp_data[0] = FS_MEM_OVERFLOW;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void*) &tmp_data[0];
					//set it as image length for modem ccci check when error happens
					pkt[pkt_num].len = img_len;
					///pkt[pkt_num].len = sizeof(unsigned int);
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void*) &tmp_data[1];
					DBGLOG(RPCD, ERR, "[RPCD] RPC_GET_SECRO_OP: md request length is larger than rpc memory: (%d, %d) \n", 
						req_len, img_len);
					break;
				}
				
				if(img_len > req_len) {
					pkt_num = 0;
					tmp_data[0] = FS_NO_MATCH;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void*) &tmp_data[0];
					//set it as image length for modem ccci check when error happens
					pkt[pkt_num].len = img_len;
					///pkt[pkt_num].len = sizeof(unsigned int);
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void*) &tmp_data[1];
					DBGLOG(RPCD, ERR, "[RPCD] RPC_GET_SECRO_OP: AP mis-match MD request length: (%d, %d) \n", 
						req_len, img_len);
					break;
				}

				/* TODO : please check it */
				/* save original modem secro length */
				DBGLOG(RPCD, TRA, "[RPCD] RPC_GET_SECRO_OP: save MD SECRO length: (%d) \n",img_len);
				img_len_bak = img_len;
	   
				blk_sz = masp_secro_blk_sz();
				for(cnt = 0; cnt < blk_sz; cnt++) {
					tmp = tmp*2;
					if(tmp >= blk_sz)
						break;
				}
				++cnt;
				img_len = ((img_len + (blk_sz-1)) >> cnt) << cnt;

				addr = p_rpc_buf->buffer + 4*sizeof(unsigned int);
				//if(md_id == MD_SYS1) {
				//	tmp_data[0] = masp_secro_md_get_data(SECRO_MD1, addr, 0, img_len);
				//} else {
				//	tmp_data[0] = masp_secro_md_get_data(SECRO_MD2, addr, 0, img_len);
				//}
				tmp_data[0] = masp_secro_md_get_data(md_post_fix, addr, 0, img_len);

				/* TODO : please check it */
				/* restore original modem secro length */
				img_len = img_len_bak;

				DBGLOG(RPCD, TRA, "[RPCD] RPC_GET_SECRO_OP: restore MD SECRO length: (%d) \n",img_len);             

				if(tmp_data[0] != 0) {
					DBGLOG(RPCD, ERR, "[RPCD] RPC_GET_SECRO_OP: get data fail:%d \n", tmp_data[0]);
					pkt_num = 0;
					pkt[pkt_num].len = sizeof(unsigned int);
					pkt[pkt_num++].buf = (void*) &tmp_data[0];
					pkt[pkt_num].len = sizeof(unsigned int);
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void*) &tmp_data[1];
				} else {
					DBGLOG(RPCD, TRA, "[RPCD] RPC_GET_SECRO_OP: get data OK: %d,%d \n", img_len, tmp_data[0]);
					pkt_num = 0;
					pkt[pkt_num].len = sizeof(unsigned int);
					//pkt[pkt_num++].buf = (void*) &img_len;
					tmp_data[1] = img_len;
					pkt[pkt_num++].buf = (void*)&tmp_data[1];
					pkt[pkt_num].len = img_len;
					pkt[pkt_num++].buf = (void*) addr;
					//tmp_data[2] = (unsigned int)addr;
					//pkt[pkt_num++].buf = (void*) &tmp_data[2];
				}
			}else {
				DBGLOG(RPCD, ERR, "[RPCD] RPC_GET_SECRO_OP: secro disable \n");
				tmp_data[0] = FS_NO_FEATURE;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				tmp_data[1] = img_len;
				pkt[pkt_num++].buf = (void*) &tmp_data[1];	
			}

			break;
		}
#endif
		//call eint API to get TDD EINT configuration for modem EINT initial
		case IPC_RPC_GET_TDD_EINT_NUM_OP:
		case IPC_RPC_GET_TDD_GPIO_NUM_OP:
		case IPC_RPC_GET_TDD_ADC_NUM_OP:
		{
			int get_num = 0;
			unsigned char * name = NULL;
			unsigned int length = 0;	

			if(pkt_num < 2)	{
				DBGLOG(RPCD, ERR, "[RPCD] invalid parameter for [0x%X]: pkt_num=%d!\n", 
	                                p_rpc_buf->rpc_opid, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err1;
			}

			if((length = pkt[0].len) < 1) {
				DBGLOG(RPCD, ERR, "[RPCD] invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n", 
					p_rpc_buf->rpc_opid, pkt_num, length);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err1;
			}

			name = kmalloc(length, GFP_KERNEL);
			if(name == NULL) {
				DBGLOG(RPCD, ERR, "[RPCD] Fail alloc Mem for [0x%X]!\n", p_rpc_buf->rpc_opid);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err1;
			} else {
				memcpy(name, (unsigned char*)(pkt[0].buf), length);

				if(p_rpc_buf->rpc_opid == IPC_RPC_GET_TDD_EINT_NUM_OP) {
					if((get_num = get_td_eint_info(md_id, name, length)) < 0) {
						get_num = FS_FUNC_FAIL;
					}
				}else if(p_rpc_buf->rpc_opid == IPC_RPC_GET_TDD_GPIO_NUM_OP) {
					if((get_num = get_md_gpio_info(md_id, name, length)) < 0)	{
						get_num = FS_FUNC_FAIL;
					}
				}
				else if(p_rpc_buf->rpc_opid == IPC_RPC_GET_TDD_ADC_NUM_OP) {
					if((get_num = get_md_adc_info(md_id, name, length)) < 0)	{
						get_num = FS_FUNC_FAIL;
					}
				}
		
				DBGLOG(RPCD, TRA, "[RPCD] [0x%08X]: name:%s, len=%d, get_num:%d\n",p_rpc_buf->rpc_opid,
					name, length, get_num);	
				pkt_num = 0;

				/* NOTE: tmp_data[1] not [0] */
				tmp_data[1] = (unsigned int)get_num;	// get_num may be invalid after exit this function
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*)(&tmp_data[1]);	//get_num);
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*)(&tmp_data[1]);	//get_num);
				kfree(name);
			}
			break;
	   
	        err1:
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				break;
	    }

		case IPC_RPC_GET_EMI_CLK_TYPE_OP:
		{
			int dram_type = 0;
			int dram_clk = 0;
		
			if(pkt_num != 0) {
				DBGLOG(RPCD, ERR, "[RPCD] invalid parameter for [0x%X]: pkt_num=%d!\n", 
	                                p_rpc_buf->rpc_opid, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err2;
			}

			if(get_dram_type_clk(&dram_clk, &dram_type)) {
                DBGLOG(RPCD, ERR, "[RPCD] [0x%08X]: get_dram_type_clk(%d,%d) call error \n",
					p_rpc_buf->rpc_opid, dram_clk, dram_type);	
				tmp_data[0] = FS_FUNC_FAIL;
				goto err2;
			}
			else {
				tmp_data[0] = 0;
				DBGLOG(RPCD, TRA, "[RPCD] [0x%08X]: dram_clk: %d, dram_type:%d \n",
					p_rpc_buf->rpc_opid, dram_clk, dram_type);	
			}
		
			tmp_data[1] = (unsigned int)dram_type;
			tmp_data[2] = (unsigned int)dram_clk;
			
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*)(&tmp_data[0]);	
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*)(&tmp_data[1]);	
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*)(&tmp_data[2]);	
			break;
			
			err2:
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				break;
	    }
			
		case IPC_RPC_GET_EINT_ATTR_OP:	
		{
			char * eint_name = NULL;
			unsigned int name_len = 0;
			unsigned int type = 0;
			char * res = NULL;
			//unsigned int res_len = 0;
			int ret = 0;
			
			if(pkt_num < 3)	{
				DBGLOG(RPCD, ERR, "[RPCD] invalid parameter for [0x%X]: pkt_num=%d!\n",
					p_rpc_buf->rpc_opid, pkt_num);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err3;
			}
			
			if((name_len = pkt[0].len) < 1) {
				DBGLOG(RPCD, ERR, "[RPCD] invalid parameter for [0x%X]: pkt_num=%d, name_len=%d!\n",
					p_rpc_buf->rpc_opid, pkt_num, name_len);
				tmp_data[0] = FS_PARAM_ERROR;
				goto err3;
			}
			
			eint_name = kmalloc(name_len, GFP_KERNEL);
			if(eint_name == NULL) {
				DBGLOG(RPCD, ERR, "[RPCD] Fail alloc Mem for [0x%X]!\n", p_rpc_buf->rpc_opid);
				tmp_data[0] = FS_ERROR_RESERVED;
				goto err3;
			}
			else {
				memcpy(eint_name, (unsigned char*)(pkt[0].buf), name_len);
			}
			
			type = *(unsigned int*)(pkt[2].buf);
			res = (unsigned char*)&(p_rpc_buf->num_para) + 4*sizeof(unsigned int);
			ret = get_eint_attr(eint_name, name_len, type, res, &res_len);
			if (ret == 0) {
				tmp_data[0] = ret;
				pkt_num = 0;
				pkt[pkt_num].len = sizeof(unsigned int);
				pkt[pkt_num++].buf = (void*) &tmp_data[0];
				pkt[pkt_num].len = res_len;
				pkt[pkt_num++].buf = (void*) res;
				DBGLOG(RPCD, TRA, "[RPCD] [0x%08X] OK: name:%s, len:%d, type:%d, res:%d, res_len:%d\n",
					p_rpc_buf->rpc_opid, eint_name, name_len, type, *res, res_len);
				kfree(eint_name);
			}
			else {
				tmp_data[0] = ret;
				DBGLOG(RPCD, ERR, "[RPCD] [0x%08X] fail: name:%s, len:%d, type:%d, ret:%d\n", p_rpc_buf->rpc_opid,
					eint_name, name_len, type, ret);
				kfree(eint_name);
				goto err3;
			}
			break;
			
		err3:
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &tmp_data[0];
			pkt[pkt_num].len = sizeof(unsigned int);
			pkt[pkt_num++].buf = (void*) &tmp_data[0];
			break;
		}

		default:
			DBGLOG(RPCD, ERR, "[RPCD] [error]Unknown Operation ID (0x%08X)\n", p_rpc_buf->rpc_opid);			
			tmp_data[0] = FS_NO_OP;
			pkt_num = 0;
			pkt[pkt_num].len = sizeof(int);
			pkt[pkt_num++].buf = (void*) &tmp_data[0];
			break;
	}
	*p_pkt_num = pkt_num;

	DBGLOG(RPCD, TRA, "[RPCD] eemcs_rpc_work_helper--\n");
}

