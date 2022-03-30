/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */
#ifndef __MTK_IMG_TRACE_H__
#define __MTK_IMG_TRACE_H__

int mtk_imgsys_runtime_resume(struct device *dev);
int mtk_imgsys_runtime_suspend(struct device *dev);

int mtk_imgsys_probe(struct platform_device *pdev);
int mtk_imgsys_remove(struct platform_device *pdev);

#endif
