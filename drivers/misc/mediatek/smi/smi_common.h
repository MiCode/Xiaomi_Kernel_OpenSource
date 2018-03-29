/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SMI_COMMON_H__
#define __SMI_COMMON_H__

#include <aee.h>
#include "smi_configuration.h"
#ifdef CONFIG_MTK_CMDQ
#include "cmdq_core.h"
#endif

#define SMI_CLIENT_DISP 0
#define SMI_CLIENT_WFD 1
#define SMI_EVENT_DIRECT_LINK  (0x1 << 0)
#define SMI_EVENT_DECOUPLE     (0x1 << 1)
#define SMI_EVENT_OVL_CASCADE  (0x1 << 2)
#define SMI_EVENT_OVL1_EXTERNAL  (0x1 << 3)

#define SMIMSG(string, args...) pr_debug("[pid=%d]" string, current->tgid, ##args)
#define SMIMSG2(string, args...) pr_debug(string, ##args)
#ifdef CONFIG_MTK_CMDQ
#define SMIMSG3(onoff, string, args...)\
	do {\
		if (onoff == 1)\
			cmdq_core_save_first_dump(string, ##args);\
		else\
			SMIMSG(string, ##args);\
	} while (0)
#else
#define SMIMSG3(string, args...) SMIMSG(string, ##args)
#endif
#define SMITMP(string, args...) pr_debug("[pid=%d]"string, current->tgid, ##args)

#define SMIERR(string, args...)\
	do {\
		pr_debug("error: " string, ##args); \
		aee_kernel_warning(SMI_LOG_TAG, "error: "string, ##args);  \
	} while (0)
#define smi_aee_print(string, args...)\
	do {\
		char smi_name[100];\
		snprintf(smi_name, 100, "[" SMI_LOG_TAG "]" string, ##args); \
	} while (0)

/*
#define smi_aee_print(string, args...)\
	do {\
		char smi_name[100];\
		snprintf(smi_name, 100, "[" SMI_LOG_TAG "]" string, ##args); \
		aee_kernel_warning(smi_name, "["SMI_LOG_TAG"]error:"string, ##args);  \
	} while (0)
*/
/* Please use the function to instead gLarbBaseAddr to prevent the NULL pointer access error */
/* when the corrosponding larb is not exist */
/* extern unsigned int gLarbBaseAddr[SMI_LARB_NR]; */
extern unsigned long get_larb_base_addr(int larb_id);

/* extern char *smi_port_name[][21]; */
/* for slow motion force 30 fps */
extern int primary_display_force_set_vsync_fps(unsigned int fps);
extern unsigned int primary_display_get_fps(void);
extern void smi_client_status_change_notify(int module, int mode);
extern void smi_dumpLarb(unsigned int index);
extern void smi_dumpCommon(void);
/* void register_base_dump(void); */

extern struct SMI_PROFILE_CONFIG smi_profile_config[SMI_PROFILE_CONFIG_NUM];
extern int smi_bus_regs_setting(int larb_id, int profile, struct SMI_SETTING *settings);
extern int smi_common_setting(int profile, struct SMI_SETTING *settings);

#endif
