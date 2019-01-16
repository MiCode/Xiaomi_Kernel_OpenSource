#ifndef __CMDQ_PROF_H__
#define __CMDQ_PROF_H__

#include <linux/types.h>

int32_t cmdq_prof_estimate_command_exe_time(
		const uint32_t *pCmd, uint32_t commandSize);

#endif				/* __CMDQ_PROF_H__ */
