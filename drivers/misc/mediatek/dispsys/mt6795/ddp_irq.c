
#define LOG_TAG "IRQ"

#include "ddp_log.h"
#include "ddp_debug.h"

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/timer.h>

//#include <mach/mt_irq.h>
#include "ddp_reg.h"
#include "ddp_irq.h"
#include "ddp_aal.h"
#include <linux/aee.h>
#include "ddp_manager.h"
#include "cmdq_reg.h"

// IRQ log print kthread
static struct task_struct *disp_irq_log_task = NULL;
static wait_queue_head_t disp_irq_log_wq;
static int disp_irq_log_module = 0;

static int irq_init = 0;

static unsigned int cnt_rdma_underflow[3];
static unsigned int cnt_rdma_abnormal[3];
static unsigned int cnt_ovl_underflow[2];
static unsigned int cnt_wdma_underflow[2];

unsigned long long rdma_start_time[3]={0};
unsigned long long rdma_end_time[3]={0};
#define DISP_MAX_IRQ_CALLBACK   10

static DDP_IRQ_CALLBACK irq_module_callback_table[DISP_MODULE_NUM][DISP_MAX_IRQ_CALLBACK];
static DDP_IRQ_CALLBACK irq_callback_table[DISP_MAX_IRQ_CALLBACK];
extern int primary_display_esd_cust_get();

int disp_register_irq_callback(DDP_IRQ_CALLBACK cb)
{
    int i = 0;
    for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
        if (irq_callback_table[i] == cb)
            break;
    }
    if (i < DISP_MAX_IRQ_CALLBACK)
        return 0;

    for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {
        if (irq_callback_table[i] == NULL)
            break;
    }
    if (i == DISP_MAX_IRQ_CALLBACK) {
        DDPERR("not enough irq callback entries for module\n");
        return -1;
    }
    DDPMSG("register callback on %d", i);
    irq_callback_table[i] = cb;
    return 0;
}

int disp_unregister_irq_callback(DDP_IRQ_CALLBACK cb)
{
    int i;
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_callback_table[i] == cb)
        {
            irq_callback_table[i] = NULL;
            break;
        }
    }
    if (i == DISP_MAX_IRQ_CALLBACK)
    {
        DDPERR("Try to unregister callback function 0x%p which was not registered\n",cb);
        return -1;
    }
    return 0;
}

int disp_register_module_irq_callback(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
    int i;
    if (module >= DISP_MODULE_NUM)
    {
        DDPERR("Register IRQ with invalid module ID. module=%d\n", module);
        return -1;
    }
    if (cb == NULL)
    {
        DDPERR("Register IRQ with invalid cb.\n");
        return -1;
    }
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_module_callback_table[module][i] == cb)
            break;
    }
    if (i < DISP_MAX_IRQ_CALLBACK)
    {
        // Already registered.
        return 0;
    }
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_module_callback_table[module][i] == NULL)
            break;
    }
    if (i == DISP_MAX_IRQ_CALLBACK)
    {
        DDPERR("No enough callback entries for module %d.\n", module);
        return -1;
    }
    irq_module_callback_table[module][i] = cb;
    return 0;
}

int disp_unregister_module_irq_callback(DISP_MODULE_ENUM module, DDP_IRQ_CALLBACK cb)
{
    int i;
    for (i=0; i<DISP_MAX_IRQ_CALLBACK; i++)
    {
        if (irq_module_callback_table[module][i] == cb)
        {
            irq_module_callback_table[module][i] = NULL;
            break;
        }
    }
	if (i == DISP_MAX_IRQ_CALLBACK) {
		DDPERR
		    ("Try to unregister callback function with was not registered. module=%d cb=0x%08lx\n", module, (unsigned long)cb);
        return -1;
    }
    return 0;
}

void disp_invoke_irq_callbacks(DISP_MODULE_ENUM module, unsigned int param)
{
    int i;
	for (i = 0; i < DISP_MAX_IRQ_CALLBACK; i++) {

            
		if (irq_callback_table[i]) {
            //DDPERR("Invoke callback function. module=%d param=0x%X\n", module, param);
            irq_callback_table[i](module,param);
        }
        
		if (irq_module_callback_table[module][i]) {
            //DDPERR("Invoke module callback function. module=%d param=0x%X\n", module, param);
            irq_module_callback_table[module][i](module,param);
        }
    }
}

//Mark out for eliminate build warning message, because it is not used
extern int dispsys_irq[DISP_REG_NUM];
	// should sort irq_id_to_module_table by numberic sequence

///TODO:  move each irq to module driver
irqreturn_t disp_irq_handler(int irq, void *dev_id)
{

    DISP_MODULE_ENUM module = DISP_MODULE_UNKNOWN;
	unsigned int reg_val = 0;                                                                             
    unsigned int index = 0;
    unsigned int mutexID = 0;
    //MMProfileLogEx(ddp_mmp_get_events()->DDP_IRQ, MMProfileFlagStart, irq, 0);
                                                                                                           
	if(irq==dispsys_irq[DISP_REG_DSI0] || irq==dispsys_irq[DISP_REG_DSI1])
    {
		index = (irq == dispsys_irq[DISP_REG_DSI0]) ? 0 : 1;                                               
		module = (irq == dispsys_irq[DISP_REG_DSI0]) ? DISP_MODULE_DSI0 : DISP_MODULE_DSI1; 
		reg_val = DISP_REG_GET(DDP_REG_BASE_DSI0+0xC + index * DISP_DSI_INDEX_OFFSET) & 0xff;   	
		if(primary_display_esd_cust_get()==1)
		{
			reg_val=reg_val&0xfffe;//rd_rdy don't clear and wait for ESD & Read LCM will clear the bit.
		}
		DISP_CPU_REG_SET(DDP_REG_BASE_DSI0+0xC + index * DISP_DSI_INDEX_OFFSET, ~reg_val);  
		DDPIRQ("IRQ: DSI%d 0x%x!\n", index, reg_val); 
			//MMProfileLogEx(ddp_mmp_get_events()->DSI_IRQ[index], MMProfileFlagPulse, reg_val, 0);
	}                                                                                                      
	else if(irq==dispsys_irq[DISP_REG_OVL0] || irq==dispsys_irq[DISP_REG_OVL1])                            
	{
		index = (irq == dispsys_irq[DISP_REG_OVL0]) ? 0 : 1;                                               
		module = (irq == dispsys_irq[DISP_REG_OVL0]) ? DISP_MODULE_OVL0 : DISP_MODULE_OVL1;                
		reg_val = DISP_REG_GET(DISP_REG_OVL_INTSTA + index * DISP_OVL_INDEX_OFFSET);                           
                if(reg_val&(1<<1))
			DDPIRQ("IRQ: OVL%d frame done!\n", index); 
		
		if (reg_val & (1 << 2)) {                                                                          
			DDPERR("IRQ: OVL%d frame underrun! cnt=%d\n", index,                                           
			cnt_ovl_underflow[index]++);                                                            
                    disp_irq_log_module |= 1<<module;
                }
		if (reg_val & (1 << 3)) {                                                                          
			DDPIRQ("IRQ: OVL%d sw reset done\n", index);                                                   
		}                                                                                                  
		if (reg_val & (1 << 4)) {                                                                          
			DDPIRQ("IRQ: OVL%d hw reset done\n", index);                                                   
		}                                                                                                  
		if (reg_val & (1 << 5)) {                                                                          
			DDPERR("IRQ: OVL%d-RDMA0 not complete untill EOF!\n", index);                                  
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 6)) {                                                                          
			DDPERR("IRQ: OVL%d-RDMA1 not complete untill EOF!\n", index);                                  
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 7)) {                                                                          
			DDPERR("IRQ: OVL%d-RDMA2 not complete untill EOF!\n", index);                                  
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 8)) {                                                                          
			DDPERR("IRQ: OVL%d-RDMA3 not complete untill EOF!\n", index);                                  
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 9)) {                                                                          
			DDPERR("IRQ: OVL%d-RDMA0 fifo underflow!\n", index);                                           
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 10)) {                                                                         
			DDPERR("IRQ: OVL%d-RDMA1 fifo underflow!\n", index);                                           
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 11)) {                                                                         
			DDPERR("IRQ: OVL%d-RDMA2 fifo underflow!\n", index);                                           
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		if (reg_val & (1 << 12)) {                                                                         
			DDPERR("IRQ: OVL%d-RDMA3 fifo underflow!\n", index);                                           
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		DISP_CPU_REG_SET(DISP_REG_OVL_INTSTA + index * DISP_OVL_INDEX_OFFSET, ~reg_val);                       
		MMProfileLogEx(ddp_mmp_get_events()->OVL_IRQ[index], MMProfileFlagPulse, reg_val, DISP_REG_GET(DISP_REG_OVL_INTSTA+index*DISP_OVL_INDEX_OFFSET)); 
	}
	else if(irq==dispsys_irq[DISP_REG_WDMA0] || irq==dispsys_irq[DISP_REG_WDMA1])                                                                                                           
	{                                                                                                      
		index = (irq==dispsys_irq[DISP_REG_WDMA0]) ? 0 : 1;                                                                                                                             
		module =(irq==dispsys_irq[DISP_REG_WDMA0]) ? DISP_MODULE_WDMA0 : DISP_MODULE_WDMA1;                                                                                             
		reg_val = DISP_REG_GET(DISP_REG_WDMA_INTSTA+index*DISP_WDMA_INDEX_OFFSET);                                                                                                      
		if (reg_val & (1 << 0)) {                                                                          
			DDPIRQ("IRQ: WDMA%d frame done!\n", index);                                                    
		}                                                                                                  
		if (reg_val & (1 << 1)) {                                                                          
			DDPERR("IRQ: WDMA%d underrun! cnt=%d\n", index,                                                
			cnt_wdma_underflow[index]++);                                                           
			disp_irq_log_module |= 1 << module;                                                            
		}                                                                                                  
		DISP_CPU_REG_SET(DISP_REG_WDMA_INTSTA + index * DISP_WDMA_INDEX_OFFSET, ~reg_val);                      
		MMProfileLogEx(ddp_mmp_get_events()->WDMA_IRQ[index], MMProfileFlagPulse,                          
									reg_val, DISP_REG_GET(DISP_REG_WDMA_CLIP_SIZE));
	}
	else if(irq==dispsys_irq[DISP_REG_RDMA0] 
			|| irq==dispsys_irq[DISP_REG_RDMA1]
			|| irq==dispsys_irq[DISP_REG_RDMA2] )                                                                                                           
	{                                                                                                                                                                                       
		if(dispsys_irq[DISP_REG_RDMA0]==irq)                                                                                                                                           
		{                                                                                                                                                                               
			index = 0;                                                                                     
			module = DISP_MODULE_RDMA0;                                                                    
		}                                                                                                                                                                               
		else if(dispsys_irq[DISP_REG_RDMA1]==irq)                                                                                                                                       
		{                                                                                                                                                                               
			index = 1;                                                                                     
			module = DISP_MODULE_RDMA1;                                                                    
		} else if (dispsys_irq[DISP_REG_RDMA2] == irq) {                                                         
			index = 2;                                                                                     
			module = DISP_MODULE_RDMA2;                                                                    
		}
		
		reg_val = DISP_REG_GET(DISP_REG_RDMA_INT_STATUS+index*DISP_RDMA_INDEX_OFFSET);                                                                                                  
		if (reg_val & (1 << 0)) {                                                                          
			DDPIRQ("IRQ: RDMA%d reg update done!\n", index);                                               
		}   
		/* deal with end first */
		if (reg_val & (1 << 2)) {                                                                          
			MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagEnd, reg_val, 0);                                                                        
			rdma_end_time[index] = sched_clock();                                                          
			DDPIRQ("IRQ: RDMA%d frame done!\n", index);                                                    
		} 		
		if (reg_val & (1 << 1)) {                                                                          
			MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagStart, reg_val, 0);                                                    
			MMProfileLogEx(ddp_mmp_get_events()->layer[0], MMProfileFlagPulse,                             
			DISP_REG_GET(DISP_REG_OVL_L0_ADDR),                                                 
			DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x1);                                          
			MMProfileLogEx(ddp_mmp_get_events()->layer[1], MMProfileFlagPulse,                             
			DISP_REG_GET(DISP_REG_OVL_L1_ADDR),                                                 
			DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x2);                                          
			MMProfileLogEx(ddp_mmp_get_events()->layer[2], MMProfileFlagPulse,                             
			DISP_REG_GET(DISP_REG_OVL_L2_ADDR),                                                 
			DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x4);                                          
			MMProfileLogEx(ddp_mmp_get_events()->layer[3], MMProfileFlagPulse,                             
			DISP_REG_GET(DISP_REG_OVL_L3_ADDR),                                                 
			DISP_REG_GET(DISP_REG_OVL_SRC_CON) & 0x8);                                          
			rdma_start_time[index] = sched_clock();                                                        
			DDPIRQ("IRQ: RDMA%d frame start!\n", index);                                                   
		}                                                                                                  
                                                                                                 
		if (reg_val & (1 << 3)) {                                                                          
			DDPERR("IRQ: RDMA%d abnormal! cnt=%d\n", index,                                                
			cnt_rdma_abnormal[index]++);                                                            
			disp_irq_log_module |= 1 << module;
			MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagPulse, reg_val, 0);                                                                        
		}                                                                                                  
		if (reg_val & (1 << 4)) {                            
			MMProfileLogEx(ddp_mmp_get_events()->rdma_underflow, MMProfileFlagPulse,cnt_rdma_underflow, 0);
			MMProfileLogEx(ddp_mmp_get_events()->SCREEN_UPDATE[index], MMProfileFlagPulse, reg_val, 0);                                                                        

			DDPERR("IRQ: RDMA%d underflow! cnt=%d dsi0_cur(%d,%d)\n", index, cnt_rdma_underflow[index]++,
					DISP_REG_GET(DDP_REG_BASE_DSI0+0x168), DISP_REG_GET(DDP_REG_BASE_DSI0+0x16C));
			disp_irq_log_module |= module;                                                                 
		}                                                                                                  
		if (reg_val & (1 << 5)) {                                                                       
			DDPIRQ("IRQ: RDMA%d target line!\n", index);                                                   
		}                                                                                                  
		/* clear intr */                                                                                   
		DISP_CPU_REG_SET(DISP_REG_RDMA_INT_STATUS + index * DISP_RDMA_INDEX_OFFSET, ~reg_val);                  
	} 

	else if(irq==dispsys_irq[DISP_REG_COLOR0] || irq==dispsys_irq[DISP_REG_COLOR1])                                                                                                                                               
	{                                                                                                                                                                                       
		index = (irq == dispsys_irq[DISP_REG_COLOR0]) ? 0 : 1;                                                   
		module = (irq == dispsys_irq[DISP_REG_COLOR0]) ? DISP_MODULE_COLOR0 : DISP_MODULE_COLOR1;                
		reg_val = 0;                                                                                       
	}                                                                                             
	else if(irq==dispsys_irq[DISP_REG_MM_MUTEX])                                                                                                                                               
	{                                                                                                                                                                                       
		module = DISP_MODULE_MUTEX;                                                                        
		reg_val = DISP_REG_GET(DISP_REG_CONFIG_MUTEX_INTSTA) & 0x7C1F;                                     
		for (mutexID = 0; mutexID < 5; mutexID++) {                                                        
			if (reg_val & (0x1 << mutexID)) {                                                              
				DDPIRQ("IRQ: mutex%d sof!\n", mutexID);                                                    
				MMProfileLogEx(ddp_mmp_get_events()->MUTEX_IRQ[mutexID],                                   
									MMProfileFlagPulse, reg_val, 0);    
			}
		}
		if (reg_val & (0x1 << (mutexID + DISP_MUTEX_TOTAL))) {                                         
			DDPIRQ("IRQ: mutex%d eof!\n", mutexID);                                                    
			MMProfileLogEx(ddp_mmp_get_events()->MUTEX_IRQ[mutexID],                                   
									MMProfileFlagPulse, reg_val, 1);
		}                                                                                              
		DISP_CPU_REG_SET(DISP_REG_CONFIG_MUTEX_INTSTA, ~reg_val);                                          
	}                                                                                                                                                                                       
	else if(irq==dispsys_irq[DISP_REG_AAL])                                                                                                                                                 
	{                                                                                                                                                                                       
		module = DISP_MODULE_AAL;                                                                          
		reg_val = DISP_REG_GET(DISP_AAL_INTSTA);                                                           
		disp_aal_on_end_of_frame();                                                                        
	}                                                                                                                                                                                       
	else                                                                                                                                                                                    
	{                                                                                                                                                                                       
		module = DISP_MODULE_UNKNOWN;                                                                      
		reg_val = 0;                                                                                       
		DDPERR("invalid irq=%d\n ", irq);                                                                  
	}    
    disp_invoke_irq_callbacks(module, reg_val);
	if (disp_irq_log_module != 0) {                                                                        
        wake_up_interruptible(&disp_irq_log_wq);
    }

	//MMProfileLogEx(ddp_mmp_get_events()->DDP_IRQ, MMProfileFlagEnd, irq, reg_val);
    return IRQ_HANDLED;
}


static int disp_irq_log_kthread_func(void *data)
{
    unsigned int i=0;
	while (1) {
        wait_event_interruptible(disp_irq_log_wq, disp_irq_log_module);
        DDPMSG("disp_irq_log_kthread_func dump intr register: disp_irq_log_module=%d \n", disp_irq_log_module);
		for (i = 0; i < DISP_MODULE_NUM; i++) {
			if ((disp_irq_log_module & (1 << i)) != 0) {
				ddp_dump_reg(i);
            }
        }
        disp_irq_log_module = 0;
    }
    return 0;
}


int disp_init_irq(void)
{
    static char * device_name="mtk_disp";

    if(irq_init)
        return 0;

    irq_init = 1;
	DDPMSG("disp_init_irq\n");
 
    //create irq log thread
    init_waitqueue_head(&disp_irq_log_wq);
    disp_irq_log_task = kthread_create(disp_irq_log_kthread_func, NULL, "ddp_irq_log_kthread");
	if (IS_ERR(disp_irq_log_task)) {
        DDPERR(" can not create disp_irq_log_task kthread\n");
    }
    //wake_up_process(disp_irq_log_task);
    return 0;
}



