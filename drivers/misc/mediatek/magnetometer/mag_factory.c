#include "mag_factory.h"

static int mag_factory_open(struct inode *inode, struct file *file)
{
    file->private_data = mag_context_obj;

    if (file->private_data == NULL)
    {
        MAG_ERR("null pointer!!\n");
        return -EINVAL;
    }
    return nonseekable_open(inode, file);
}

static int mag_factory_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}

static long mag_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *data;
    long err = 0;
	struct mag_context *cxt = mag_context_obj;
	int x,y,z,status;
	char strbuf[256];
    //int cali[3] = {0};
    //SENSOR_DATA sensor_data = {0};
	
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
        MAG_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
    case MSENSOR_IOCTL_SENSOR_ENABLE:
		if(cxt->mag_ctl.m_enable != NULL){
			err = cxt->mag_ctl.m_enable(1);
			if(err < 0)
			{
				MAG_ERR("MSENSOR_IOCTL_SENSOR_ENABLE read data fail!\n");
				break;
			}
		}
        break;    
    case MSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
		if(cxt->mag_dev_data.get_data_m != NULL){
			err = cxt->mag_dev_data.get_raw_data(&x, &y, &z);
			if(err < 0)
			{
				MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA read data fail!\n");
				break;
			}
			sprintf(strbuf, "%x %x %x", x, y, z);
			MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA read data : (%d, %d, %d)!\n", x, y, z);
			MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA read strbuf : (%s)!\n", strbuf);
			
	        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
	        {
	            err = -EFAULT;
	            break;    
	        }
		}else{
			MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA NULL!\n");
		}
        break;
		
    case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
		if(cxt->mag_dev_data.get_data_o != NULL){
			err = cxt->mag_dev_data.get_data_o(&x, &y, &z, &status);
			if(err < 0)
			{
				MAG_ERR("MSENSOR_IOCTL_READ_FACTORY_SENSORDATA read data fail!\n");
				break;
			}
			sprintf(strbuf, "%x %x %x %x %x", x, y, z, status, 1);
			MAG_ERR("MSENSOR_IOCTL_READ_FACTORY_SENSORDATA read data : (%d, %d, %d, %d)!\n", x, y, z, status);
	        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
	        {
	            err = -EFAULT;
	            break;    
	        }
		}else{
			MAG_ERR("MSENSOR_IOCTL_READ_FACTORY_SENSORDATA NULL!\n ");
		}
        break;   
		
    default:
        MAG_ERR("unknown IOCTL: 0x%08x\n", cmd);
        err = -ENOIOCTLCMD;
        break;

    }
    return err;
}


static struct file_operations mag_factory_fops = {
    .open = mag_factory_open,
    .release = mag_factory_release,
    .unlocked_ioctl = mag_factory_unlocked_ioctl,
};

static struct miscdevice mag_factory_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &mag_factory_fops,
};

int mag_factory_device_init()
{
	int error = 0;
	struct mag_context *cxt = mag_context_obj;
	if (!cxt->mag_ctl.is_use_common_factory) {
		MAG_LOG("Node of '/dev/msensor' has already existed!\n");
		return -1;
	}
    if ((error = misc_register(&mag_factory_device)))
    {
        MAG_ERR("mag_factory_device register failed\n");
		error = -1;
    }	
	return error;
}




