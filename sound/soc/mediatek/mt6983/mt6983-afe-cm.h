/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 * Author: yiwen chiou<yiwen.chiou@mediatek.com
 */

#ifndef MTK_AFE_CM_H_
#define MTK_AFE_CM_H_
enum {
	CM1,
	CM2,
	CM_NUM,
};

enum {
	CM_1x_SEL_FS_8K = 0,
	CM_1x_SEL_FS_11025K = 1,
	CM_1x_SEL_FS_12K = 2,
	CM_1x_SEL_FS_384K = 3,
	CM_1x_SEL_FS_16K = 4,
	CM_1x_SEL_FS_2205K = 5,
	CM_1x_SEL_FS_24K = 6,
	//CM_1x_SEL_FS_352K = 7,
	CM_1x_SEL_FS_32K = 8,
	CM_1x_SEL_FS_44K = 9,
	CM_1x_SEL_FS_48K = 10,
	CM_1x_SEL_FS_882K = 11,
	CM_1x_SEL_FS_96K = 12,
	CM_1x_SEL_FS_176K = 13,
	CM_1x_SEL_FS_192K = 14,
	//CM_1x_SEL_FS_260K = 15,
};

int mtk_set_cm(struct mtk_base_afe *afe, int id, unsigned int rate, unsigned int update,
				bool swap, unsigned int ch);
int mtk_enable_cm_bypass(struct mtk_base_afe *afe, int id, bool en1, bool en2);
int mtk_enable_cm(struct mtk_base_afe *afe, int id, bool en);
int mt6983_is_need_enable_cm(struct mtk_base_afe *afe, int id);

#endif /* MTK_AFE_CM_H_ */
