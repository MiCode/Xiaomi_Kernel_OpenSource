
#ifndef _MJC_KERNEL_COMPAT_DRIVER_H
#define _MJC_KERNEL_COMPAT_DRIVER_H

long compat_mjc_ioctl(struct file *pfile, unsigned int u4cmd, unsigned long u4arg);

#endif