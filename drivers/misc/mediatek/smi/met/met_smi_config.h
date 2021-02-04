/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef __MET_SMI_CONFIGURATION_H__
#define __MET_SMI_CONFIGURATION_H__

#if IS_ENABLED(CONFIG_MACH_MT6758)
#define MET_SMI_LARB_NUM	8
#elif IS_ENABLED(CONFIG_MACH_MT6765)
#define MET_SMI_LARB_NUM	4
#elif IS_ENABLED(CONFIG_MACH_MT6761)
#define MET_SMI_LARB_NUM	3
#elif IS_ENABLED(CONFIG_MACH_MT3967)
#define MET_SMI_LARB_NUM	7
#elif IS_ENABLED(CONFIG_MACH_MT6779)
#define MET_SMI_LARB_NUM	12
#elif IS_ENABLED(CONFIG_MACH_MT6763)
#define MET_SMI_LARB_NUM	4
#else
#define MET_SMI_LARB_NUM	0
#endif
#define MET_SMI_COMM_NUM	1
#define SMI_MET_TOTAL_MASTER_NUM	(MET_SMI_COMM_NUM + MET_SMI_LARB_NUM)

struct smi_desc {
	unsigned long port;
	enum SMI_DEST desttype;
	enum SMI_RW rwtype;
	enum SMI_BUS bustype;
};

struct chip_smi {
	int master;
	struct smi_desc *desc;
	unsigned int count;
};

extern struct chip_smi smi_map[SMI_MET_TOTAL_MASTER_NUM];
#endif /* __MET_SMI_CONFIGURATION_H__ */
