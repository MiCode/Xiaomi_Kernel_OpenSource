#ifndef _MT_SYNC_WRITE_H
#define _MT_SYNC_WRITE_H

#if defined(__KERNEL__)

#include <linux/io.h>
#include <asm/cacheflush.h>
//#include <asm/system.h>

/*
 * Define macros.
 */
#define mt_reg_sync_writel(v, a)	mt65xx_reg_sync_writel(v, a)
#define mt_reg_sync_writew(v, a)	mt65xx_reg_sync_writew(v, a)
#define mt_reg_sync_writeb(v, a)	mt65xx_reg_sync_writeb(v, a)

#define mt65xx_reg_sync_writel(v, a) \
        do {    \
            __raw_writel((v), IOMEM((a)));   \
            dsb();  \
        } while (0)

#define mt65xx_reg_sync_writew(v, a) \
        do {    \
            __raw_writew((v), IOMEM((a)));   \
            dsb();  \
        } while (0)

#define mt65xx_reg_sync_writeb(v, a) \
        do {    \
            __raw_writeb((v), IOMEM((a)));   \
            dsb();  \
        } while (0)

#ifdef CONFIG_64BIT
#define mt_reg_sync_writeq(v, a) \
        do {    \
            __raw_writeq((v), IOMEM((a)));   \
            dsb();  \
        } while (0)
#endif

#else   /* __KERNEL__ */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define mt_reg_sync_writel(v, a)        mt65xx_reg_sync_writel(v, a)
#define mt_reg_sync_writew(v, a)        mt65xx_reg_sync_writew(v, a)
#define mt_reg_sync_writeb(v, a)        mt65xx_reg_sync_writeb(v, a)

#define dsb()   \
        do {    \
            __asm__ __volatile__ ("dsb" : : : "memory"); \
        } while (0)

#define mt65xx_reg_sync_writel(v, a) \
        do {    \
            *(volatile unsigned int *)(a) = (v);    \
            dsb(); \
        } while (0)

#define mt65xx_reg_sync_writew(v, a) \
        do {    \
            *(volatile unsigned short *)(a) = (v);    \
            dsb(); \
        } while (0)

#define mt65xx_reg_sync_writeb(v, a) \
        do {    \
            *(volatile unsigned char *)(a) = (v);    \
            dsb(); \
        } while (0)

#endif  /* __KERNEL__ */

#endif  /* !_MT_SYNC_WRITE_H */
