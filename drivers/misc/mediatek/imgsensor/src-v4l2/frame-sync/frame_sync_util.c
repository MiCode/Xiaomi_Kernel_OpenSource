// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "frame_sync_util.h"

inline unsigned int
convert2TotalTime(unsigned int lineTimeInNs, unsigned int lc)
{
	if (lineTimeInNs == 0)
		return 0;

	return (unsigned int)((unsigned long long)(lc)
				* lineTimeInNs / 1000);
}


inline unsigned int
convert2LineCount(unsigned int lineTimeInNs, unsigned int val)
{
	return ((1000 * (unsigned long long)val) / lineTimeInNs) +
		((1000 * (unsigned long long)val) % lineTimeInNs ? 1 : 0);
}

