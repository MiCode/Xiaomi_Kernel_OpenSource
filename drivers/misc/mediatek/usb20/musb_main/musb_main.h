/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */


#ifndef _MUSB_MAIN_H_
#define _MUSB_MAIN_H_

extern int musb_init_controller(struct device *dev, int nIrq,
					void __iomem *ctrl, void __iomem *ctrlp);

extern int usb_disabled(void);
extern int musb_remove(struct platform_device *pdev);
extern void musb_shutdown(struct platform_device *pdev);

#endif	/* _MUSB_MAIN_H_ */
