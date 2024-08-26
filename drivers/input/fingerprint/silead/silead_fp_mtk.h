/*
 * @file   silead_fp_mtk.h
 * @brief  Contains silead_fp Mediatek platform specific head file.
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

#ifndef __SILEAD_FP_MTK_H__
#define __SILEAD_FP_MTK_H__

struct fp_plat_t {
#ifdef CONFIG_OF
    u32 spi_id;
    u32 spi_irq;
    u32 spi_reg;
    struct pinctrl *pinctrl;
    struct pinctrl_state *pins_default;
    struct pinctrl_state *pins_irq, *pins_rst_h, *pins_rst_l;
    struct pinctrl_state *pins_irq_rst_h, *pins_irq_rst_l;
    struct pinctrl_state *spi_default;
#ifdef BSP_SIL_POWER_SUPPLY_PINCTRL
    struct pinctrl_state *pins_avdd_h, *pins_avdd_l, *pins_vddio_h;
#endif /* BSP_SIL_POWER_SUPPLY_PINCTRL */
#endif
};

#endif /* __SILEAD_FP_MTK_H__ */

/* End of file silead_fp_mtk.h */
