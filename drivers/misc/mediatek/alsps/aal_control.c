#include <linux/miscdevice.h>
#include <linux/sensors_io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include "alsps.h"
#include "aal_control.h"
#include <linux/hwmsen_dev.h>

int aal_use = 0;

static int AAL_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int AAL_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long AAL_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{ 
		long err = 0;
		void __user *ptr = (void __user*) arg;
		int dat;
		uint32_t enable;
		
		switch (cmd)
		{
			
			case AAL_SET_ALS_MODE:
				if(copy_from_user(&enable, ptr, sizeof(enable)))
				{
					err = -EFAULT;
					goto err_out;
				}

                if (enable) 
				    aal_use = 1;
				else 
				    aal_use = 0; 
				
				if((err = alsps_aal_enable(enable)) != 0)
				{
					AAL_LOG("als driver don't support new arch, goto execute old arch: %ld\n", err); 
				    if((err = hwmsen_aal_enable(enable)) != 0)
						AAL_ERR("Enable als driver fail %ld\n", err);
				}
				break;
	
			case AAL_GET_ALS_MODE:
				AAL_LOG("AAL_GET_ALS_MODE do nothing\n");
				break;
	
			case AAL_GET_ALS_DATA: 
	            if ((dat = alsps_aal_get_data()) < 0){
					AAL_LOG("alsps_aal_get_data fail\n");
					dat = hwmsen_aal_get_data();
	            }
				
				AAL_LOG("Get als dat :%d\n", dat);
				
				if(copy_to_user(ptr, &dat, sizeof(dat)))
				{
					err = -EFAULT;
					goto err_out;
				}			   
				break;
			
			default:
				AAL_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
				err = -ENOIOCTLCMD;
				break;
		}
	
		err_out:
		return err;    
	}

static struct file_operations AAL_fops = {
	.owner = THIS_MODULE,
	.open = AAL_open,
	.release = AAL_release,
	.unlocked_ioctl = AAL_unlocked_ioctl,
};

static struct miscdevice AAL_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aal_als",
	.fops = &AAL_fops,
};


/*----------------------------------------------------------------------------*/
static int __init AAL_init(void)
{
	int err;
    if((err = misc_register(&AAL_device)))
	{
		AAL_ERR("AAL_device misc_register failed: %d\n", err);
	}
	AAL_FUN("OK!\n");
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit AAL_exit(void)
{
   int err;
   if((err = misc_deregister(&AAL_device)))
	{
		AAL_ERR("AAL_device misc_deregister fail: %d\n", err);    
	}
  return;
}
/*----------------------------------------------------------------------------*/
late_initcall(AAL_init);
//module_init(AAL_init);
//module_exit(AAL_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("AAL driver");
MODULE_LICENSE("GPL");


