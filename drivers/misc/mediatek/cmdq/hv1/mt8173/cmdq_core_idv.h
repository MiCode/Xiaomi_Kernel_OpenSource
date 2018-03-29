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


#define CMDQ_ENG_MDP_GROUP_BITS                 ((1LL << CMDQ_ENG_MDP_CAMIN) |      \
						 (1LL << CMDQ_ENG_MDP_RDMA0) |      \
						 (1LL << CMDQ_ENG_MDP_RDMA1) |      \
						 (1LL << CMDQ_ENG_MDP_RSZ0) |       \
						 (1LL << CMDQ_ENG_MDP_RSZ1) |       \
						 (1LL << CMDQ_ENG_MDP_RSZ2) |       \
						 (1LL << CMDQ_ENG_MDP_TDSHP0) |     \
						 (1LL << CMDQ_ENG_MDP_TDSHP1) |     \
						 (1LL << CMDQ_ENG_MDP_WROT0) |      \
						 (1LL << CMDQ_ENG_MDP_WROT1) |      \
						 (1LL << CMDQ_ENG_MDP_WDMA))


void cmdqCorePrintStatus_idv(char *buf);
void cmdqCorePrintStatusSeq_idv(struct seq_file *m);

