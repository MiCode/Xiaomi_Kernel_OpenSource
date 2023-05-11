/*
 * @file   silead_fp_qcom.h
 * @brief  Contains silead_fp Qualcomm platform specific head file.
 *
 *
 * Copyright 2016-2021 Gigadevice/Silead Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * ------------------- Revision History ------------------------------
 * <author>    <date>   <version>     <desc>
 * Bill Yu    2018/5/2    0.1.0      Init version
 *
 */

#ifndef __SILEAD_FP_QCOM_H__
#define __SILEAD_FP_QCOM_H__

#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
//#include <soc/qcom/scm.h>

struct fp_plat_t {
    u32 qup_id;
#ifdef QSEE_V4
    u32 max_speed_hz;
#else
    /* pinctrl info */
    struct pinctrl  *pinctrl;
    struct pinctrl_state  *active;
    struct pinctrl_state  *sleep;
#ifdef BSP_SIL_POWER_SUPPLY_PINCTRL
    struct pinctrl_state *pins_avdd_h, *pins_vddio_h;
#endif /* BSP_SIL_POWER_SUPPLY_PINCTRL */
    /* clock info */
    struct clk    *core_clk;
    struct clk    *iface_clk;
#endif /* QSEE_V4 */
};

#endif /* __SILEAD_FP_QCOM_H__ */

/* End of file silead_fp_qcom.h */
