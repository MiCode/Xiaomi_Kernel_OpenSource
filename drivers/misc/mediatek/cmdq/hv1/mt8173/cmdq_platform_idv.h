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

#include "cmdq_def.h"


/* use to generate [CMDQ_ENGINE_ENUM_id and name] mapping for status print */
#define CMDQ_FOREACH_STATUS_MODULE_PRINT(ACTION)\
{		\
	ACTION(CMDQ_ENG_ISP_IMGI,   ISP_IMGI)	\
	ACTION(CMDQ_ENG_MDP_RDMA0,  MDP_RDMA0)	\
	ACTION(CMDQ_ENG_MDP_RDMA1,  MDP_RDMA1)	\
	ACTION(CMDQ_ENG_MDP_RSZ0,   MDP_RSZ0)	\
	ACTION(CMDQ_ENG_MDP_RSZ1,   MDP_RSZ1)	\
	ACTION(CMDQ_ENG_MDP_RSZ2,   MDP_RSZ2)	\
	ACTION(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0)	\
	ACTION(CMDQ_ENG_MDP_TDSHP1, MDP_TDSHP1)	\
	ACTION(CMDQ_ENG_MDP_WROT0,  MDP_WROT0)	\
	ACTION(CMDQ_ENG_MDP_WROT1,  MDP_WROT1)	\
	ACTION(CMDQ_ENG_MDP_WDMA,   MDP_WDMA)	\
}

