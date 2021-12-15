/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#ifndef __MT_CPU_TOPO_PLATFORM_H__
#define __MT_CPU_TOPO_PLATFORM_H__

#ifdef __cplusplus
extern "C" {
#endif


/*==============================================================*/
/* Macros							*/
/*==============================================================*/
/* TODO: remove these workaround for k49 migration */
#define NO_SCHEDULE_API		(1)

#define TOTAL_CORE_NUM	(CORE_NUM_L)
#define CORE_NUM_L	(4)

#define get_cluster_cpu_core(id)	\
		(id ? 0 : CORE_NUM_L)

/*==============================================================*/
/* Enum								*/
/*==============================================================*/
enum ppm_cluster {
	PPM_CLUSTER_L = 0,

	NR_PPM_CLUSTERS,
};

/*==============================================================*/
/* Data Structures						*/
/*==============================================================*/

/*==============================================================*/
/* Global Variables						*/
/*==============================================================*/

/*==============================================================*/
/* APIs								*/
/*==============================================================*/


#ifdef __cplusplus
}
#endif

#endif
