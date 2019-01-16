#define MHL_LINUXDRV_MAIN_C
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include "mhl_linuxdrv.h"
#include "osal/include/osal.h"
#include "si_mhl_tx_api.h"
static bool bTxOpen;
int32_t SiiMhlOpen(struct inode *pInode, struct file *pFile)
{
	if (bTxOpen) {
		SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,
				"Driver already open, failing open request\n");
		return -EBUSY;
	}
	bTxOpen = true;
	return 0;
}

int32_t SiiMhlRelease(struct inode *pInode, struct file *pFile)
{
	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE, "Close %s\n", MHL_DRIVER_NAME);
	bTxOpen = false;
	return 0;
}
