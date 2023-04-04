/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */

/* VENC physical base address */
#define VENC_BASE_PHY   0x17002000
#define VENC_REGION     0x1000

/* VDEC virtual base address */
#define VDEC_BASE_PHY   0x17000000
#define VDEC_REGION     0x50000

#define HW_BASE         0x7FFF000
#define HW_REGION       0x2000

#define INFO_BASE       0x10000000
#define INFO_REGION     0x1000

#define VCODEC_DEVNAME     "Vcodec"
#define VCODEC_DEV_MAJOR_NUMBER 160   /* 189 */

void vcodec_driver_plat_init(void);
void vcodec_driver_plat_exit(void);
int vcodec_plat_probe(struct platform_device *pdev, struct mtk_vcodec_dev *dev);
void vcodec_plat_release(void);
int vcodec_suspend(struct platform_device *pDev, pm_message_t state);
int vcodec_resume(struct platform_device *pDev);
long vcodec_lockhw(unsigned long arg);
long vcodec_unlockhw(unsigned long arg);
long vcodec_waitisr(unsigned long arg);
long vcodec_plat_unlocked_ioctl(unsigned int cmd, unsigned long arg);
