#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/timer.h>
#include <asm/atomic.h>
#include <mach/mt_boot.h>

#include "eemcs_md.h"
#include "eemcs_ccci.h"
#include "eemcs_debug.h"

#define MDLOGGER_FILE_PATH "/data/mdlog/mdlog5_config"

/*
 * @brief Configure the modem runtime data structure.
 * @param
 *     buffer [in] The buffer containing modem runtime data.
 * @return Size of the modem runtime data structure is returned always.
 */
KAL_INT32 eemcs_md_runtime_cfg(void *buffer)
{
    struct file *filp = NULL;
    LOGGING_MODE mdlog_flag = MODE_IDLE;
    struct MODEM_RUNTIME_st *runtime = NULL;
    int ret = 0;

	KAL_ASSERT(buffer != NULL);
    runtime = (struct MODEM_RUNTIME_st *)buffer;
    memset(runtime, 0, sizeof(struct MODEM_RUNTIME_st));
	
    runtime->Prefix = 0x46494343;           //"CCIF"
    runtime->Postfix = 0x46494343;          //"CCIF"
    runtime->BootChannel = CH_CTRL_RX;
    runtime->DriverVersion = 0x20110118;

    filp = filp_open(MDLOGGER_FILE_PATH, O_RDONLY, 0777);
    if (!IS_ERR(filp)) {
        ret = kernel_read(filp, 0, (char*)&mdlog_flag, sizeof(int));	
        if (ret != sizeof(int)) 
            mdlog_flag = MODE_IDLE;
    } else {
        DBGLOG(BOOT, ERR, "open %s fail: %ld", MDLOGGER_FILE_PATH, PTR_ERR(filp));
        filp = NULL;
    }

    if (filp != NULL) {
        filp_close(filp, NULL);
    }

    if (is_meta_mode() || is_advanced_meta_mode())
        runtime->BootingStartID = ((char)mdlog_flag << 8 | META_BOOT_ID);
    else
        runtime->BootingStartID = ((char)mdlog_flag << 8 | NORMAL_BOOT_ID);

    DBGLOG(BOOT, INF, "send /data/mdlog/mdlog5_config =%d to modem!", mdlog_flag);

    return sizeof(struct MODEM_RUNTIME_st);
}

/*
 * @brief Generate a modem runtime data structure.
 * @param
 *     data [out] A pointer to receive the allocated memory address
 *                of modem runtime data.
 * @return Size of the allocated memory.
 */
KAL_UINT32 eemcs_md_gen_runtime_data(void **data)
{
    void *runtime_data = NULL;
    RUNTIME_BUFF *buf = NULL;

    runtime_data = kmalloc(sizeof(struct MODEM_RUNTIME_st) + sizeof(KAL_UINT32), GFP_KERNEL);
    if (unlikely(runtime_data == NULL)) {
        DBGLOG(BOOT, ERR, "MODEM_RUNTIME_st allocation failed !!");
        return 0;
    }
    *data = runtime_data;
    buf = runtime_data;
    buf->len = sizeof(struct MODEM_RUNTIME_st);
    eemcs_md_runtime_cfg(runtime_data + sizeof(KAL_UINT32));
    return sizeof(struct MODEM_RUNTIME_st) + sizeof(KAL_UINT32);
}

/*
 * @brief Free a modem runtime data structure.
 * @param
 *     runtime_data [in] The modem runtime data structure returned from eemcs_md_gen_runtime_data().
 * @return None.
 */
void eemcs_md_destroy_runtime_data(void *runtime_data)
{
    if (runtime_data != NULL) {
        kfree(runtime_data);
    }
}

