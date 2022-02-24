// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_EEM_INTERNAL_H_
#define _MTK_EEM_INTERNAL_H_

/*
 * bit operation
 */
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

/*
 * LOG
 */
#define EEM_TAG	 "[CPU][EEM]"
#if 1
	#define eem_error(fmt, args...)		pr_notice(EEM_TAG fmt, ##args)
	#define eem_warning(fmt, args...)
	#define eem_notice(fmt, args...)
	#define eem_info(fmt, args...)
	#define eem_debug(fmt, args...)
#else
	#define eem_error(fmt, args...)	 pr_debug(EEM_TAG fmt, ##args)
	#define eem_warning(fmt, args...)   pr_debug(EEM_TAG fmt, ##args)
	#define eem_notice(fmt, args...)   pr_debug(EEM_TAG fmt, ##args)
	#define eem_info(fmt, args...)   pr_debug(EEM_TAG fmt, ##args)
	#define eem_debug(fmt, args...)   pr_debug(EEM_TAG fmt, ##args)
#endif

#if EN_ISR_LOG /* For Interrupt use */
	#define eem_isr_info(fmt, args...)  eem_debug(fmt, ##args)
#else
	#define eem_isr_info(fmt, args...)
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


#if CONFIG_EEM_SHOWLOG
	static unsigned int func_lv_mask = (
		FUNC_LV_MODULE |
		FUNC_LV_CPUFREQ |
		FUNC_LV_API |
		FUNC_LV_LOCAL |
		FUNC_LV_HELP
		);
	#define FUNC_ENTER(lv)	\
		do { if ((lv) & func_lv_mask) \
			eem_debug(">> %s()\n", \
				__func__); } while (0)
	#define FUNC_EXIT(lv)	\
		do { if ((lv) & func_lv_mask) \
			eem_debug("<< %s():%d\n", \
				__func__, __LINE__); } while (0)
#else
	#define FUNC_ENTER(lv)
	#define FUNC_EXIT(lv)
#endif /* CONFIG_CPU_DVFS_SHOWLOG */

#define TIME_TH_US 3000
#define EEM_IS_TOO_LONG()   \
	do {	\
		eem_diff_us = eem_cTime_us - eem_pTime_us;	\
		if (eem_diff_us > TIME_TH_US) {				\
			eem_debug(EEM_TAG "caller_addr %p: %llu us\n", \
				__builtin_return_address(0), eem_diff_us); \
		} else if (eem_diff_us < 0) {	\
			eem_debug(EEM_TAG "E: misuse caller_addr %p\n", \
				__builtin_return_address(0)); \
		}	\
	} while (0)

/*
 * REG ACCESS
 */
#define eem_read(addr)	__raw_readl((void __iomem *)(addr))/*DRV_Reg32(addr)*/
#define eem_read_field(addr, range)	\
	((eem_read(addr) & BITMASK(range)) >> LSB(range))
#define eem_write(addr, val)	mt_reg_sync_writel(val, addr)

/**
 * Write a field of a register.
 * @addr:	Address of the register
 * @range:	The field bit range in the form of MSB:LSB
 * @val:	The value to be written to the field
 */
#define eem_write_field(addr, range, val)	\
	eem_write(addr, (eem_read(addr) & ~BITMASK(range)) | BITS(range, val))

/**
 * Helper macros
 */
/**
 * iterate over list of detectors
 * @det:	the detector * to use as a loop cursor.
 */
#define for_each_det(det) \
		for (det = eemsn_detectors; \
		det < (eemsn_detectors + ARRAY_SIZE(eemsn_detectors)); \
		det++)



/**
 * Given a eem_det * in eemsn_detectors. Return the id.
 * @det:	pointer to a eem_det in eemsn_detectors
 */
#define det_to_id(det)	((det) - &eemsn_detectors[0])


/**
 * Check if a detector has a feature
 * @det:	pointer to a eem_det to be check
 * @feature:	enum eem_features to be checked
 */
#define HAS_FEATURE(det, feature)	((det)->features & feature)

#define PERCENT(numerator, denominator)	\
	(unsigned char)(((numerator) * 100 + (denominator) - 1) / (denominator))


/* define main structures in mtk_eem_internal.c */

extern struct eemsn_det eemsn_detectors[NR_EEMSN_DET];
extern struct eemsn_det_ops eem_det_base_ops;
extern unsigned int sn_mcysys_reg_base[NUM_SN_CPU];
extern unsigned short sn_mcysys_reg_dump_off[SIZE_SN_MCUSYS_REG];


/* define common operations in mtk_eem_internal.c */
extern int base_ops_volt_2_pmic(struct eemsn_det *det, int volt);
extern int base_ops_volt_2_eem(struct eemsn_det *det, int volt);
extern int base_ops_pmic_2_volt(struct eemsn_det *det, int pmic_val);
extern int base_ops_eem_2_pmic(struct eemsn_det *det, int eev_val);
extern unsigned int detid_to_dvfsid(struct eemsn_det *det);

#endif
