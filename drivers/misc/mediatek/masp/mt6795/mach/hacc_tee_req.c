#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/xlog.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include "mobicore_driver_api.h"
#include "tlcApisec.h"
#include "sec_error.h"

#define MC_UUID_SDRV_DEFINE { 0x05, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
static const struct mc_uuid_t MC_UUID_HACC = {MC_UUID_SDRV_DEFINE};
uint32_t deviceId = MC_DEVICE_ID_DEFAULT;
struct mc_session_handle drSessionHandle;
static dapc_tciMessage_t *pTci  = NULL;
//static int open_count = 0;

/* DO NOT invoke this function unless you get HACC lock */
int open_sdriver_connection(void)
{
    enum mc_result mcRet = 0;
    int retryCnt = 0;

    do
    {
        /* Initialize session handle data */
        mcRet = mc_open_device(deviceId);
        if (MC_DRV_OK != mcRet)
        {
            retryCnt++;
            printk("NWD HACC: mc_open_device failed: %d, retry count (%d)\n", mcRet, retryCnt);
            continue;
        }
        printk("NWD HACC: mc_open_device success: %d\n", mcRet);

        /* Allocating WSM for DCI */
        mcRet = mc_malloc_wsm(deviceId, 0, sizeof(dapc_tciMessage_t), (uint8_t **) &pTci, 0);
        if (MC_DRV_OK != mcRet)
        {            
            printk("NWD HACC: mc_malloc_wsm failed: %d\n", mcRet);
            break;
        }

        /* Open session to the trustlet */ 
        memset(&drSessionHandle, 0, sizeof(drSessionHandle));
        drSessionHandle.device_id = deviceId;
        mcRet = mc_open_session(&drSessionHandle, &MC_UUID_HACC, (uint8_t *) pTci,(uint32_t) sizeof(dapc_tciMessage_t));

        if (MC_DRV_OK != mcRet)
        {            
            printk("NWD HACC: mc_open_session failed: %d\n", mcRet);
            break;
        }

        printk("NWD HACC: mc_open_session success: %d\n", mcRet);
        break;
    }while(retryCnt < 30);

    if (MC_DRV_OK != mcRet)
    {
        return -1;
    }

    return 0;
}

/* DO NOT invoke this function unless you get HACC lock */
int close_sdriver_connection(void)
{
    enum mc_result mcRet = 0;

    do
    {
        /* Close session*/        
        mcRet = mc_close_session(&drSessionHandle);
        if (MC_DRV_OK != mcRet)        
        {            
            printk("NWD HACC: mc_close_session failed: %d\n", mcRet);
            break;  
        }
        printk("NWD HACC: mc_close_session success: %d\n", mcRet);
        memset(&drSessionHandle, 0, sizeof(drSessionHandle));

        /* Free WSM for DCI */     
        mcRet = mc_free_wsm(deviceId, (uint8_t *)pTci);        
        if (MC_DRV_OK != mcRet)        
        {            
            printk("NWD HACC: mc_free_wsm failed: %d\n", mcRet);           
            break;
        }
        pTci = NULL;
        
        /* Close MobiCore device */        
        mcRet = mc_close_device(deviceId);        
        if (MC_DRV_OK != mcRet)        
        {            
            printk("NWD HACC: mc_close_device failed: %d\n", mcRet);
            break;
        }     
        printk("NWD HACC: mc_close_device success: %d\n", mcRet);   
    }while(false);

    if (MC_DRV_OK != mcRet)
    {
        return -1;
    }

    return 0;
}


/* DO NOT invoke this function unless you get HACC lock */
int tee_secure_request(unsigned int user, unsigned char *data, unsigned int data_size, 
    unsigned int direction, unsigned char *seed, unsigned int seed_size)
{
    int ret = SEC_OK;    
    struct mc_bulk_map dataMapInfo;
    struct mc_bulk_map seedMapInfo;
    char *databuf = NULL;
    char *seedbuf = NULL;
    enum mc_result mcRet = 0;

    /* allocate data buffer to be sent to TEE */
    databuf = (char*)vmalloc(data_size);    
    if(databuf == NULL)
    {
        printk("NWD HACC: vmalloc fail for data buffer");
        ret = ERR_HACC_ALLOCATE_BUFFER_FAIL;
        goto _allocate_data_buf_err;
    }
    memcpy(databuf, data, data_size);

    if(seed_size != 0)
    {
        /* allocate seed buffer to be sent to TEE */
        seedbuf = (char*)vmalloc(seed_size); 
        if(seedbuf == NULL)
        {
            printk("NWD HACC: vmalloc fail for seed buffer");
            ret = ERR_HACC_ALLOCATE_BUFFER_FAIL;
            goto _allocate_seed_buf_err;
        }
        memcpy(seedbuf, seed, seed_size);
    }

    /* map TCI virtual address for data buffer */
    ret = mc_map(&drSessionHandle, databuf, data_size, &dataMapInfo);
    if (MC_DRV_OK != ret) {
        printk("NWD HACC: mcMap failed of data buffer: %d", ret);
        ret = ERR_HACC_MCMAP_BUFFER_FAIL;
        goto _mcmap_data_fail;
    }    
    pTci->data_addr = (uint32_t)dataMapInfo.secure_virt_addr;
    pTci->data_len = data_size;

    if(seed_size != 0)
    {
        /* map TCI virtual address for seed buffer */
        ret = mc_map(&drSessionHandle, seedbuf, seed_size, &seedMapInfo);
        if (MC_DRV_OK != ret) {
            printk("NWD HACC: mcMap failed of seed buffer: %d", ret);
            ret = ERR_HACC_MCMAP_BUFFER_FAIL;
            goto _mcmap_seed_fail;
        }
        pTci->seed_addr = (uint32_t)seedMapInfo.secure_virt_addr;  
        pTci->seed_len = seed_size;
    }
    else
    {
        pTci->seed_addr = 0;
        pTci->seed_len = 0;
    }
    
    /* set other TCI parameter */
    pTci->hacc_user = user;
    pTci->direction = direction;

    /* set TCI command */
    pTci->cmd.header.commandId = CMD_HACC_REQUEST; 
   
	/* notify the trustlet */        
	printk("NWD HACC: prepare notify\n");
	mcRet = mc_notify(&drSessionHandle);        
	if (MC_DRV_OK != mcRet)        
	{            
		printk("NWD HACC IRQ fail: mc_notify returned: %d\n", mcRet); 
        ret = ERR_HACC_NOTIFY_TO_TRUSTLET_FAIL;
		goto _notify_to_trustlet_fail;
	}
    
	/* wait for response from the trustlet */        
	if (MC_DRV_OK != (mcRet = mc_wait_notification(&drSessionHandle, /*MC_INFINITE_TIMEOUT*/20000)))        
	{            
		printk("NWD HACC IRQ fail: mc_wait_notification 20s timeout: %d\n", mcRet);
		printk("NWD HACC IRQ fail: mc_wait_notification 20s timeout: %d\n", mcRet);
		printk("NWD HACC IRQ fail: mc_wait_notification 20s timeout: %d\n", mcRet);
		ret = ERR_HACC_NOTIFY_FROM_TRUSTLET_FAIL;
		goto _notify_from_trustlet_fail;
	}    

    if(pTci->result != 0)
    {
        printk("NWD HACC Request Fail!!!!!!!!(ret:%d, err:%d)\n", pTci->result, pTci->rsp.header.returnCode);  
        printk("NWD HACC Request Fail!!!!!!!!(ret:%d, err:%d)\n", pTci->result, pTci->rsp.header.returnCode); 
        printk("NWD HACC Request Fail!!!!!!!!(ret:%d, err:%d)\n", pTci->result, pTci->rsp.header.returnCode); 
    }
    else
    {        
        printk("NWD HACC Request Success!!!!!!!!\n");  
        printk("NWD HACC Request Success!!!!!!!!\n"); 
        printk("NWD HACC Request Success!!!!!!!!\n");
        /* update result from secure buffer */
        memcpy(data, databuf, data_size); 
    }
	
_notify_from_trustlet_fail:
_notify_to_trustlet_fail:
    if(seed_size != 0)    
    {    
        mc_unmap(&drSessionHandle, seedbuf, &seedMapInfo);
    }
_mcmap_seed_fail:    
    mc_unmap(&drSessionHandle, databuf, &dataMapInfo);
_mcmap_data_fail:
    if(seed_size != 0)
    {
        vfree(seedbuf);
    }
_allocate_seed_buf_err:
    vfree(databuf);
_allocate_data_buf_err:
    
    return ret;
}

