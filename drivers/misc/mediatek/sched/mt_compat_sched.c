#ifdef CONFIG_MT_SCHED
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "mt_sched_drv.h"

#ifdef CONFIG_COMPAT
struct compat_ioctl_arg {
	compat_int_t  pid;
	compat_uint_t len;
	compat_uptr_t mask;
	compat_uptr_t mt_mask;
};

#define COMPAT_IOCTL_SETAFFINITY       _IOW(IOC_MAGIC, 0, struct compat_ioctl_arg)
#define COMPAT_IOCTL_GETAFFINITY       _IOR(IOC_MAGIC, 2, struct compat_ioctl_arg)

static int compat_get_sched_allocation_data(
			struct compat_ioctl_arg __user *data32,
			struct ioctl_arg __user *data)
{
	compat_int_t i;
	compat_uint_t u;
	compat_uptr_t p;
	int err;
	
	err = get_user(i, &data32->pid);	
	err |= put_user(i, &data->pid);	
	err |= get_user(u, &data32->len);	
	err |= put_user(u, &data->len);	
	err |= get_user(p, &data32->mask);	
	err |= put_user(compat_ptr(p), &data->mask);	
	err |= get_user(p, &data32->mt_mask);	
	err |= put_user(compat_ptr(p), &data->mt_mask);	

	return err;
}

static int compat_put_sched_allocation_data(
			struct compat_ioctl_arg __user *data32,
			struct ioctl_arg __user *data)
{
	pid_t i;
	unsigned int u;
	unsigned long *p;
	int err;
	
	err = get_user(i, &data->pid);	
	err |= put_user(i, &data32->pid);	
	err |= get_user(u, &data->len);	
	err |= put_user(u, &data32->len);	
	err |= get_user(p, &data->mask);	
	err |= put_user(ptr_to_compat(p), &data32->mask);	
	err |= get_user(p, &data->mt_mask);	
	err |= put_user(ptr_to_compat(p), &data32->mt_mask);	

	return err;
}

long sched_ioctl_compat(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_IOCTL_SETAFFINITY:
	{
		struct compat_ioctl_arg __user *data32;
		struct ioctl_arg __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
		
		err = compat_get_sched_allocation_data(data32, data);
		if (err)
			return err;
		
		return filp->f_op->unlocked_ioctl(filp, IOCTL_SETAFFINITY, (unsigned long)data);
	}
	case COMPAT_IOCTL_GETAFFINITY:
	{
		struct compat_ioctl_arg __user *data32;
		struct ioctl_arg __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
		
		err = compat_get_sched_allocation_data(data32, data);
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, IOCTL_GETAFFINITY, (unsigned long)data);

		err = compat_put_sched_allocation_data(data32, data);
		return ret ? ret : err;
	}
	case IOCTL_EXITAFFINITY:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
	default:
		return -ENOIOCTLCMD;
	}
}
#endif
#endif
