/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-sp-common.h  --  Mediatek Smart Phone Common
 *
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef _MTK_SP_COMMON_H_
#define _MTK_SP_COMMON_H_

#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>

#define AUDIO_AEE(message) \
	(aee_kernel_exception_api(__FILE__, \
				  __LINE__, \
				  DB_OPT_FTRACE, message, \
				  "audio assert"))
#else
#define AUDIO_AEE(message) WARN_ON(true)
#endif

bool mtk_get_speech_status(void);

#endif
