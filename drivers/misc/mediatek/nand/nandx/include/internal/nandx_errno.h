/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef	__ERRNO_H__
#define	__ERRNO_H__

/* Nand Driver Extended Error Codes */
#define NAND_OK		0

#ifndef EIO
#define	EIO			5	/* I/O error */
#define	ENOMEM		12	/* Out of memory */
#define	EFAULT		14	/* Bad address */
#define	EBUSY		16	/* Device or resource busy */
#define	ENODEV		19	/* No such device */
#define	EINVAL		22	/* Invalid argument */
#define	ENOSPC		28	/* No space left on device */
/* Operation not supported on transport endpoint */
#define	EOPNOTSUPP	95
#define ETIMEDOUT	110	/* Connection timed out */
#endif

#define ENANDFLIPS	1024	/* Too many bitflips, corrected */
#define ENANDREAD	1025	/* Read fail, can't correct */
#define ENANDWRITE	1026	/* Write fail */
#define ENANDERASE	1027	/* Erase fail */
#define ENANDBAD	1028	/* Bad block */

#define IS_NAND_ERR(err)	((err) >= -ENANDBAD && (err) <= -ENANDFLIPS)

#endif				/* __ERRNO_H__ */
