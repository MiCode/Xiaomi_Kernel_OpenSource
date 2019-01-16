#include "barometer_factory.h"

static int baro_factory_open(struct inode *inode, struct file *file)
{
    file->private_data = baro_context_obj;

    if (file->private_data == NULL)
    {
        BARO_ERR("null pointer!!\n");
        return -EINVAL;
    }
    return nonseekable_open(inode, file);
}

static int baro_factory_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}

static long baro_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    //void __user *data;
    long err = 0;
	struct baro_context *cxt = baro_context_obj;
	void __user *ptr = (void __user*) arg;
	int dat;
	//uint32_t enable = 0;
	//int ps_cali;
	//int threshold_data[2];

	
    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
        BARO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
    case BAROMETER_IOCTL_INIT:
		BARO_LOG("BAROMETER_IOCTL_INIT enable\n");
		if(cxt->baro_ctl.enable_nodata!= NULL){
			err = cxt->baro_ctl.enable_nodata(1);
			if(err < 0)
			{
				BARO_ERR("BAROMETER_IOCTL_INIT fail!\n");
				break;
			}
		}
	    break;
    case BAROMETER_GET_PRESS_DATA:		
		if(cxt->baro_data.get_raw_data!= NULL){
			err = cxt->baro_data.get_raw_data(TYPE_PRESS, &dat);
			if(err < 0)
			{
				BARO_ERR("BAROMETER_GET_PRESS_DATA fail!\n");
				break;
			}
		}

		if(copy_to_user(ptr, &dat, sizeof(dat)))
		{
			err = -EFAULT;
			break;
		}  
        break;    
    case BAROMETER_GET_TEMP_DATA:
		if(cxt->baro_data.get_raw_data!= NULL){
			err = cxt->baro_data.get_raw_data(TYPE_TEMP, &dat);
			if(err < 0)
			{
				BARO_ERR("BAROMETER_GET_PRESS_DATA fail!\n");
				break;
			}
		}


		if(copy_to_user(ptr, &dat, sizeof(dat)))
		{
			err = -EFAULT;
			break;
		}
        break;   
		
    default:
        BARO_ERR("unknown IOCTL: 0x%08x\n", cmd);
        err = -ENOIOCTLCMD;
        break;

    }
    return err;
}


static struct file_operations baro_factory_fops = {
    .open = baro_factory_open,
    .release = baro_factory_release,
    .unlocked_ioctl = baro_factory_unlocked_ioctl,
};

static struct miscdevice baro_factory_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "barometer",
    .fops = &baro_factory_fops,
};

int baro_factory_device_init()
{
	int error = 0;
	struct baro_context *cxt = baro_context_obj;

	if (!cxt->baro_ctl.is_use_common_factory) {
		BARO_LOG("Node of '/dev/barometer' has already existed!\n");
		return -1;
	}
    if ((error = misc_register(&baro_factory_device)))
    {
        BARO_ERR("baro_factory_device register failed\n");
		error = -1;
    }	
	return error;
}




