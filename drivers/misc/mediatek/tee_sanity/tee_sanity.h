/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __TEE_SANITY_H__
#define __TEE_SANITY_H__

#define PFX                     KBUILD_MODNAME ": "

#define tee_log(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

/* TEE sanity UT commands */
#define TEE_UT_READ_INTR	0
#define TEE_UT_TRIGGER_INTR	1

/* MTK_SIP_KERNEL_TEE_CONTROL SMC op_id */
#define TEE_OP_ID_NONE                  (0xFFFF0000)
#define TEE_OP_ID_SET_PENDING           (0xFFFF0001)

/* TEE tracing log prefix */
#define TEE_BEGIN_TRACE         "tee_trace_begin"
#define TEE_END_TRACE           "tee_trace_end"
#define TEE_TRACING_MARK	"tee_tracing"

#define BEGINED_PID		(current->tgid)

/* TEE tracing error code */
#define TEE_TRACE_OK			0
#define TEE_TRACE_PREFIX_NOT_MATCH	(-1)
#define TEE_TRACE_PARSE_FAILED		(-2)

/* TEE tracing */
int32_t mtk_tee_log_tracing(u32 cpuid, u16 tee_pid, char *line, u32 line_len);
void mtk_set_prefer_bigcore(struct task_struct *current_task);
void mtk_set_task_basic_util(struct task_struct *current_task);

/* Extern API */
extern int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type);
extern int set_task_util_min_pct(pid_t pid, unsigned int min);

struct tee_trace_struct {
	u32 cpuid;
	char *ktimestamp;
	u32 tee_pid;
	char *tee_postfix;
};

#endif /* __TEE_SANITY_H__ */
