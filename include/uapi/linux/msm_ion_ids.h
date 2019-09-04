/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_ION_IDS_H
#define _MSM_ION_IDS_H

/**
 * These are the only ids that should be used for Ion heap ids.
 * The ids listed are the order in which allocation will be attempted
 * if specified. Don't swap the order of heap ids unless you know what
 * you are doing!
 * Id's are spaced by purpose to allow new Id's to be inserted in-between (for
 * possible fallbacks)
 */

#define ION_CP_MM_HEAP_ID		8
#define ION_SECURE_HEAP_ID		9
#define ION_SECURE_DISPLAY_HEAP_ID	10
#define ION_SPSS_HEAP_ID		13
#define ION_SECURE_CARVEOUT_HEAP_ID	14
#define ION_QSECOM_TA_HEAP_ID		19
#define ION_CAMERA_HEAP_ID		20
#define ION_ADSP_HEAP_ID		22
#define ION_SYSTEM_HEAP_ID		25
#define ION_USER_CONTIG_HEAP_ID		26
#define ION_QSECOM_HEAP_ID		27
#define ION_AUDIO_HEAP_ID		28
#define ION_HEAP_ID_RESERVED		31

#endif /* _MSM_ION_IDS_H */
