/*
 * @file   silead_fp_stub.h
 * @brief  Contains silead_fp platform stub specific head file.
 *
 *
 * Copyright 2016-2021 Gigadevice/Silead Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * ------------------- Revision History ------------------------------
 * <author>    <date>   <version>     <desc>
 * Melvin cao  2019/3/18    0.1.0      Init version
 *
 */

#ifndef __SILEAD_FP_STUB_H__
#define __SILEAD_FP_STUB_H__

#include <linux/semaphore.h>
#include <linux/wait.h>
#include "silead_fp.h"

#ifdef BSP_SIL_DYNAMIC_SPI

#define SIL_STUB_MAJOR   0

struct sil_stub_dev {
    dev_t       devt;
    struct cdev cdev;
    spinlock_t  lock;
    int         fp_init;
};

#define SIL_STUB_IOCPRINT   _IO(SIFP_IOC_MAGIC, 1)
#define SIL_STUB_IOCINIT    _IO(SIFP_IOC_MAGIC, 2)
#define SIL_STUB_IOCDEINIT  _IO(SIFP_IOC_MAGIC, 3)

#define SIL_STUB_IOCGETDATA  _IOR(SIFP_IOC_MAGIC, 4, int)
#define SIL_STUB_IOCSETDATA  _IOW(SIFP_IOC_MAGIC, 5, int)

#define SIL_STUB_MAXNR      5

#define FP_STUB_DEV_NAME    "silead_stub"
#define FP_STUB_CLASS_NAME  "silead_stub_class"

extern void silfp_dev_exit(void);
extern int silfp_dev_init(void);

#endif  /* BSP_SIL_DYNAMIC_SPI */

#endif /* __SILEAD_FP_STUB_H__ */
