// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _MTK_EEMG_INTERNAL_H_
#define _MTK_EEMG_INTERNAL_H_

#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
/**
 * Genearte a mask wher MSB to LSB are all 0b1
 * @r:	Range in the form of MSB:LSB
 */
#define BITMASK(r)	\
	(((unsigned int) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))

#define GET_BITS_VAL(_bits_, _val_) \
	(((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

#define EEMG_TAG	 "[xxxxEEMG] "
#if 1
	#define eemg_error(fmt, args...)	pr_notice(EEMG_TAG fmt, ##args)
	#define eemg_warning(fmt, args...)
	#define eemg_notice(fmt, args...)
	#define eemg_info(fmt, args...)
	#define eemg_debug(fmt, args...)
#else
	#define eemg_error(fmt, args...)	 pr_debug(EEMG_TAG fmt, ##args)
	#define eemg_warning(fmt, args...)   pr_debug(EEMG_TAG fmt, ##args)
	#define eemg_notice(fmt, args...)   pr_debug(EEMG_TAG fmt, ##args)
	#define eemg_info(fmt, args...)   pr_debug(EEMG_TAG fmt, ##args)
	#define eemg_debug(fmt, args...)   pr_debug(EEMG_TAG fmt, ##args)
#endif

#if EN_ISR_LOG /* For Interrupt use */
	#define eemg_isr_info(fmt, args...)  eemg_debug(fmt, ##args)
#else
	#define eemg_isr_info(fmt, args...)
#endif

/* module, platform driver interface */
#define FUNC_LV_MODULE		  BIT(0)
/* cpufreq driver interface		  */
#define FUNC_LV_CPUFREQ		 BIT(1)
/* mt_cpufreq driver global function */
#define FUNC_LV_API			 BIT(2)
/* mt_cpufreq driver lcaol function  */
#define FUNC_LV_LOCAL		   BIT(3)
/* mt_cpufreq driver help function   */
#define FUNC_LV_HELP			BIT(4)


#if CONFIG_EEMG_SHOWLOG
	static unsigned int func_lv_mask = (
		FUNC_LV_MODULE |
		FUNC_LV_CPUFREQ |
		FUNC_LV_API |
		FUNC_LV_LOCAL |
		FUNC_LV_HELP
		);
	#define FUNC_ENTER(lv)	\
		do { if ((lv) & func_lv_mask) \
			eemg_debug(">> %s()\n", \
				__func__); } while (0)
	#define FUNC_EXIT(lv)	\
		do { if ((lv) & func_lv_mask) \
			eemg_debug("<< %s():%d\n", \
				__func__, __LINE__); } while (0)
#else
	#define FUNC_ENTER(lv)
	#define FUNC_EXIT(lv)
#endif /* CONFIG_CPU_DVFS_SHOWLOG */

#define TIME_TH_US 3000
#define EEMG_IS_TOO_LONG()   \
do {	\
	eemg_diff_us = eemg_cTime_us - eemg_pTime_us;	\
	if (eemg_diff_us > TIME_TH_US) {				\
		eemg_debug(EEMG_TAG "caller_addr %p: %llu us\n", \
			__builtin_return_address(0), eemg_diff_us); \
	} else if (eemg_diff_us < 0) {	\
		eemg_debug(EEMG_TAG "E: misuse caller_addr %p\n", \
			__builtin_return_address(0)); \
	}	\
} while (0)

#define eemg_read(addr)	__raw_readl((void __iomem *)(addr))/*DRV_Reg32(addr)*/
#define eemg_read_field(addr, range)	\
	((eemg_read(addr) & BITMASK(range)) >> LSB(range))
#define eemg_write(addr, val)	mt_reg_sync_writel(val, addr)

/**
 * Write a field of a register.
 * @addr:	Address of the register
 * @range:	The field bit range in the form of MSB:LSB
 * @val:	The value to be written to the field
 */
#define eemg_write_field(addr, range, val)	\
	eemg_write(addr, (eemg_read(addr) & ~BITMASK(range)) | BITS(range, val))

/**
 * Helper macros
 */
/**
 * iterate over list of detectors
 * @det:	the detector * to use as a loop cursor.
 */
#define for_each_det(det) \
		for (det = eemg_detectors; \
		det < (eemg_detectors + ARRAY_SIZE(eemg_detectors)); \
		det++)

/**
 * iterate over list of detectors and its controller
 * @det:	the detector * to use as a loop cursor.
 * @ctrl:	the eemg_ctrl * to use as ctrl pointer of current det.
 */
#define for_each_det_ctrl(det, ctrl)				\
		for (det = eemg_detectors,				\
		ctrl = id_to_eemg_ctrl(det->ctrl_id);		\
		det < (eemg_detectors + ARRAY_SIZE(eemg_detectors)); \
		det++,						\
		ctrl = id_to_eemg_ctrl(det->ctrl_id))

/**
 * iterate over list of controllers
 * @pos:	the eemg_ctrl * to use as a loop cursor.
 */
#define for_each_ctrl(ctrl) \
		for (ctrl = eemg_ctrls; \
		ctrl < (eemg_ctrls + ARRAY_SIZE(eemg_ctrls)); \
		ctrl++)

/**
 * Given a eemg_det * in eemg_detectors. Return the id.
 * @det:	pointer to a eemg_det in eemg_detectors
 */
#define det_to_id(det)	((det) - &eemg_detectors[0])

/**
 * Given a eemg_ctrl * in eemg_ctrls. Return the id.
 * @det:	pointer to a eemg_ctrl in eemg_ctrls
 */
#define ctrl_to_id(ctrl)	((ctrl) - &eemg_ctrls[0])

/**
 * Check if a detector has a feature
 * @det:	pointer to a eemg_det to be check
 * @feature:	enum eemg_features to be checked
 */
#define HAS_FEATURE(det, feature)	((det)->features & feature)

#define PERCENT(numerator, denominator)	\
	(unsigned char)(((numerator) * 100 + (denominator) - 1) / (denominator))

struct eemg_ctrl {
	const char *name;
	enum eemg_det_id det_id;
	/* struct completion init_done; */
	/* atomic_t in_init; */

	/* for voltage setting thread */
	wait_queue_head_t wq;

	int volt_update;
	struct task_struct *thread;
};

/* define main structures in mtk_eemg_internal.c */
extern struct eemg_ctrl eemg_ctrls[NR_EEMG_CTRL];
extern struct eemg_det eemg_detectors[NR_EEMG_DET];
extern struct eemg_det_ops eemg_det_base_ops;
extern unsigned int gpu_vb_turn_pt;


/* define common operations in mtk_eemg_internal.c */
extern int base_ops_volt_2_pmic_gpu(struct eemg_det *det, int volt);
extern int base_ops_volt_2_eemg(struct eemg_det *det, int volt);
extern int base_ops_pmic_2_volt_gpu(struct eemg_det *det, int pmic_val);
extern int base_ops_eemg_2_pmic(struct eemg_det *det, int eev_val);
#endif
