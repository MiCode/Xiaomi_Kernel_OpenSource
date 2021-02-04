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

#ifndef __MTK_PE_INTF_H
#define __MTK_PE_INTF_H


#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT
extern int mtk_pep_init(void);
extern int mtk_pep_reset_ta_vchr(void);
extern int mtk_pep_check_charger(void);
extern int mtk_pep_start_algorithm(void);
extern int mtk_pep_set_charging_current(enum CHR_CURRENT_ENUM *ichg,
	enum CHR_CURRENT_ENUM *aicr);

extern void mtk_pep_set_to_check_chr_type(bool check);
extern void mtk_pep_set_is_enable(bool enable);
extern void mtk_pep_set_is_cable_out_occur(bool out);

extern bool mtk_pep_get_to_check_chr_type(void);
extern bool mtk_pep_get_is_connect(void);
extern bool mtk_pep_get_is_enable(void);

#else /* NOT CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT */

static inline int mtk_pep_init(void)
{
	return -ENOTSUPP;
}
static inline int mtk_pep_reset_ta_vchr(void)
{
	return -ENOTSUPP;
}
static inline int mtk_pep_check_charger(void)
{
	return -ENOTSUPP;
}
static inline int mtk_pep_start_algorithm(void)
{
	return -ENOTSUPP;
}
#if 0
static inline int mtk_pep_plugout_reset(void)
{
	return -ENOTSUPP;
}
#endif
static inline int mtk_pep_set_charging_current(enum CHR_CURRENT_ENUM *ichg,
	enum CHR_CURRENT_ENUM *aicr)
{
	return -ENOTSUPP;
}

static inline void mtk_pep_set_to_check_chr_type(bool check)
{
}
static inline void mtk_pep_set_is_cable_out_occur(bool out)
{
}
static inline void mtk_pep_set_is_enable(bool enable)
{
}

static inline bool mtk_pep_get_to_check_chr_type(void)
{
	return false;
}
static inline bool mtk_pep_get_is_connect(void)
{
	return false;
}
static inline bool mtk_pep_get_is_enable(void)
{
	return false;
}
#endif /* CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT */


#endif /* __MTK_PE_INTF_H */
