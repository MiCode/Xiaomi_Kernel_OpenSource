/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_offloadv1.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio offloadv1 playback
 *
 * Author:
 * -------
 * Doug Wang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    E X T E R N A L   R E F E R E N C E
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_OffloadCommon.h"
#include <linux/compat.h>


/*****************************************************************************
 * Variable Definition
****************************************************************************/
static int8   OffloadService_name[]         = "offloadserivce_driver_device";

static struct AFE_OFFLOAD_SERVICE_T afe_offload_service = {
	.write_blocked   = false,
	.enable          = false,
	.setVol          = NULL,
	.offload_mode    = 0,
	.hw_gain         = 0x10000,
};

static uint32 Data_Wait_Queue_flag;
DECLARE_WAIT_QUEUE_HEAD(Data_Wait_Queue);



/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D   W R I T E   B L O C K   F U N C T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
void OffloadService_ProcessWriteblocked(int flag)
{
	if (flag == 1) {
		PRINTK_AUDDRV("offload drain wait\n");
		Data_Wait_Queue_flag = 0;
		wait_event_interruptible(Data_Wait_Queue, Data_Wait_Queue_flag);
		PRINTK_AUDDRV("offload drain write restart\n");
	} else if (afe_offload_service.write_blocked) {
		PRINTK_AUDDRV("offload write wait\n");
		Data_Wait_Queue_flag = 0;
		wait_event_interruptible(Data_Wait_Queue, Data_Wait_Queue_flag);
		PRINTK_AUDDRV("offload write restart\n");
	}
}

void OffloadService_SetWriteblocked(bool flag)
{
	afe_offload_service.write_blocked = flag;
}

void OffloadService_ReleaseWriteblocked(void)
{
	if (Data_Wait_Queue_flag == 0) {
		Data_Wait_Queue_flag = 1;
		wake_up_interruptible(&Data_Wait_Queue);
	}
}


/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                    O F F L O A D   C O N T R O L   F U N C T I O N S
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
void OffloadService_SetVolumeCbk(void (*setVol)(int vol))
{
	afe_offload_service.setVol = setVol;
	pr_warn("%s callback:%p\n", __func__, setVol);
}

void OffloadService_SetVolume(int vol)
{
	pr_warn("%s gain:0x%x\n", __func__, vol);
	afe_offload_service.hw_gain = vol;
	afe_offload_service.setVol(vol);
}

int OffloadService_GetVolume(void)
{
	return afe_offload_service.hw_gain;
}


void OffloadService_SetOffloadMode(int mode)
{
	afe_offload_service.offload_mode = mode;

	if (mode == OFFLOAD_MODE_SW)
		SetOffloadSWMode(true);
	else
		SetOffloadSWMode(false);

	pr_warn("%s mode:0x%x\n", __func__, mode);
}

int OffloadService_GetOffloadMode(void)
{
	return afe_offload_service.offload_mode;
}

void OffloadService_SetEnable(bool enable)
{
	afe_offload_service.enable = enable;
	pr_warn("%s enable:0x%x\n", __func__, enable);
}

bool OffloadService_GetEnable(void)
{
	pr_warn("%s enable:0x%x\n", __func__, afe_offload_service.enable);
	return afe_offload_service.enable;
}

/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                        P L A T F O R M   D R I V E R   F O R   O F F L O A D   C O M M O N
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
static int OffloadService_open(struct inode *inode, struct file *fp)
{
	pr_warn("%s inode:%p, file:%p\n", __func__, inode, fp);
	return 0;
}

static int OffloadService_release(struct inode *inode, struct file *fp)
{
	pr_warn("%s inode:%p, file:%p\n", __func__, inode, fp);

	if (!(fp->f_mode & FMODE_WRITE || fp->f_mode & FMODE_READ))
		return -ENODEV;
	return 0;
}

static long OffloadService_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int  ret = 0;

	PRINTK_AUD_DL2("OffloadWBlock_ioctl cmd = %u arg = %lu\n", cmd, arg);

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OFFLOADSERVICE_WRITEBLOCK):
		OffloadService_ProcessWriteblocked((int)arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_SETGAIN):
		OffloadService_SetVolume((int)arg);
		break;
	case _IOC_NR(OFFLOADSERVICE_SETMODE):
		OffloadService_SetOffloadMode((int)arg);
		break;
	default:
		break;
	}

	return ret;
}

static ssize_t OffloadService_write(struct file *fp, const char __user *data, size_t count, loff_t *offset)
{
	return 0;
}

static ssize_t OffloadService_read(struct file *fp,  char __user *data, size_t count, loff_t *offset)
{
	return count;
}

static int OffloadService_flush(struct file *flip, fl_owner_t id)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static int OffloadService_fasync(int fd, struct file *flip, int mode)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static int OffloadService_remap_mmap(struct file *flip, struct vm_area_struct *vma)
{
	pr_warn("%s\n", __func__);
	return -1;
}

static int OffloadService_probe(struct platform_device *dev)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static int OffloadService_remove(struct platform_device *dev)
{
	pr_warn("%s\n", __func__);
	return 0;
}

static void OffloadService_shutdown(struct platform_device *dev)
{
	pr_warn("%s\n", __func__);
}

static int OffloadService_suspend(struct platform_device *dev, pm_message_t state)
/* only one suspend mode */
{
	pr_warn("%s\n", __func__);
	return 0;
}

static int OffloadService_resume(struct platform_device *dev) /* wake up */
{
	pr_warn("%s\n", __func__);
	return 0;
}

/*
 * ioctl32 compat
 */
#ifdef CONFIG_COMPAT

static long OffloadService_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg)
{
	file->f_op->unlocked_ioctl(file, cmd, arg);
	return 0;
}

#else
#define OffloadService_ioctl_compat   NULL
#endif

static const struct file_operations OffloadService_fops = {
	.owner          = THIS_MODULE,
	.open           = OffloadService_open,
	.release        = OffloadService_release,
	.unlocked_ioctl = OffloadService_ioctl,
	.compat_ioctl  = OffloadService_ioctl_compat,
	.write          = OffloadService_write,
	.read           = OffloadService_read,
	.flush          = OffloadService_flush,
	.fasync         = OffloadService_fasync,
	.mmap           = OffloadService_remap_mmap
};

static struct miscdevice OffloadService_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = OFFLOAD_DEVNAME,
	.fops = &OffloadService_fops,
};

const struct dev_pm_ops OffloadService_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};


static struct platform_driver OffloadService_driver = {
	.probe    = OffloadService_probe,
	.remove   = OffloadService_remove,
	.shutdown = OffloadService_shutdown,
	.suspend  = OffloadService_suspend,
	.resume   = OffloadService_resume,
	.driver   = {
#ifdef CONFIG_PM
		.pm     = &OffloadService_pm_ops,
#endif
		.name = OffloadService_name,
	},
};

static int OffloadService_mod_init(void)
{
	int ret = 0;

	pr_warn("OffloadService_mod_init\n");

	/* Register platform DRIVER */
	ret = platform_driver_register(&OffloadService_driver);
	if (ret) {
		pr_err("OffloadService Fail:%d - Register DRIVER\n", ret);
		return ret;
	}

	/* register MISC device */
	ret = misc_register(&OffloadService_misc_device);

	if (ret) {
		pr_err("OffloadService misc_register Fail:%d\n", ret);
		return ret;
	}

	return 0;
}

static void  OffloadService_mod_exit(void)
{
	pr_warn("%s\n", __func__);
}
module_init(OffloadService_mod_init);
module_exit(OffloadService_mod_exit);


/*
============================================================================================================
------------------------------------------------------------------------------------------------------------
||                        License
------------------------------------------------------------------------------------------------------------
============================================================================================================
*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek OffloadService Driver");
