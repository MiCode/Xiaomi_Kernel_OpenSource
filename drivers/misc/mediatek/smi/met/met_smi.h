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

#ifndef __MET_SMI_H__
#define __MET_SMI_H__

#define MET_SMI_SUCCESS    0
#define MET_SMI_FAIL      -1

/* Parallel	[31:24] */
#define SMI_MET_BIT_PM		24
/* Master	[15:12] */
#define SMI_MET_BIT_MASTER	12
/* Port		[11:6] */
#define SMI_MET_BIT_PORT	6
/* Read/Write	[5:4] */
#define SMI_MET_BIT_RW		4
/* DST		[3:2] */
#define SMI_MET_BIT_DST		2
/* REQ		[1:0] */
#define SMI_MET_BIT_REQ		0
#define SMI_MET_DEBUGBUF_SIZE	512

#define SMI_MET_MAX_PORT_NUM	4
#define SMI_MET_IOMEM_ADDR(base, offset) \
	((void __iomem *)(((unsigned long)base)+offset))
/* LARB */
#define SMI_MET_LARB_MON_ENA(b)		SMI_MET_IOMEM_ADDR((b), 0x400)
#define SMI_MET_LARB_MON_CLR(b)		SMI_MET_IOMEM_ADDR((b), 0x404)
#define SMI_MET_LARB_MON_PORT(b)	SMI_MET_IOMEM_ADDR((b), 0x408)
#define SMI_MET_LARB_MON_CON(b)		SMI_MET_IOMEM_ADDR((b), 0x40C)
#define SMI_MET_LARB_MON_ACT_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x410)
#define SMI_MET_LARB_MON_REQ_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x414)
#define SMI_MET_LARB_MON_BEA_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x418)
#define SMI_MET_LARB_MON_BYT_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x41C)
#define SMI_MET_LARB_MON_CP_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x420)
#define SMI_MET_LARB_MON_DP_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x424)
#define SMI_MET_LARB_MON_OSTD_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x428)
#define SMI_MET_LARB_MON_CP_MAX(b)	SMI_MET_IOMEM_ADDR((b), 0x430)
#define SMI_MET_LARB_MON_OSTD_MAX(b)	SMI_MET_IOMEM_ADDR((b), 0x434)
/* COMMON */
#define SMI_MET_COMM_MON_ENA(b)		SMI_MET_IOMEM_ADDR((b), 0x1A0)
#define SMI_MET_COMM_MON_CLR(b)		SMI_MET_IOMEM_ADDR((b), 0x1A4)
#define SMI_MET_COMM_MON_TYPE(b)	SMI_MET_IOMEM_ADDR((b), 0x1AC)
#define SMI_MET_COMM_MON_CON(b)		SMI_MET_IOMEM_ADDR((b), 0x1B0)
#define SMI_MET_COMM_MON_ACT_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1C0)
#define SMI_MET_COMM_MON_REQ_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1C4)
#define SMI_MET_COMM_MON_OSTD_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1C8)
#define SMI_MET_COMM_MON_BEA_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1CC)
#define SMI_MET_COMM_MON_BYT_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1D0)
#define SMI_MET_COMM_MON_CP_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1D4)
#define SMI_MET_COMM_MON_DP_CNT(b)	SMI_MET_IOMEM_ADDR((b), 0x1D8)
#define SMI_MET_COMM_MON_CP_MAX(b)	SMI_MET_IOMEM_ADDR((b), 0x1DC)
#define SMI_MET_COMM_MON_OSTD_MAX(b)	SMI_MET_IOMEM_ADDR((b), 0x1E0)

#ifdef CONFIG_TRACING
#define SMI_MET_TRACE_PUTS(_THIS_IP_, SOB, size) \
	__trace_puts(_THIS_IP_, (SOB), (size))
#else
#define SMI_MET_TRACE_PUTS(_THIS_IP_, SOB, size) do {} while (0)
#endif

DEFINE_PER_CPU(char[MET_STRBUF_SIZE], smi_met_strbuf_nmi);
DEFINE_PER_CPU(char[MET_STRBUF_SIZE], smi_met_strbuf_irq);
DEFINE_PER_CPU(char[MET_STRBUF_SIZE], smi_met_strbuf_sirq);
DEFINE_PER_CPU(char[MET_STRBUF_SIZE], smi_met_strbuf);

#define SMI_MET_PRINTK_GETBUF(pSOB, pEOB) \
	({ \
		preempt_disable(); \
		if (in_nmi()) \
			*pSOB = per_cpu(smi_met_strbuf_nmi, \
				smp_processor_id()); \
		else if (in_irq()) \
			*pSOB = per_cpu(smi_met_strbuf_irq, \
				smp_processor_id()); \
		else if (in_softirq()) \
			*pSOB = per_cpu(smi_met_strbuf_sirq, \
				smp_processor_id()); \
		else \
			*pSOB = per_cpu(smi_met_strbuf, smp_processor_id()); \
		*pEOB = *pSOB; \
	})

#define SMI_MET_PRINTK_PUTBUF(SOB, EOB) \
	({ \
		SMI_MET_TRACE_PUTS(_THIS_IP_, (SOB), \
			(uintptr_t)((EOB)-(SOB))); \
		preempt_enable_no_resched(); \
	})

#define SMI_MET_PRINTK(FORMAT, args...) \
	do { \
		char *pmet_strbuf; \
		preempt_disable(); \
		if (in_nmi()) \
			pmet_strbuf = per_cpu(smi_met_strbuf_nmi, \
				smp_processor_id()); \
		else if (in_irq()) \
			pmet_strbuf = per_cpu(smi_met_strbuf_irq, \
				smp_processor_id()); \
		else if (in_softirq()) \
			pmet_strbuf = per_cpu(smi_met_strbuf_sirq, \
				smp_processor_id()); \
		else \
			pmet_strbuf = per_cpu(smi_met_strbuf, \
				smp_processor_id()); \
		snprintf(pmet_strbuf, MET_STRBUF_SIZE, FORMAT, ##args); \
		TRACE_PUTS(pmet_strbuf); \
		preempt_enable_no_resched(); \
	} while (0)
#endif /* __MET_SMI_H__ */
