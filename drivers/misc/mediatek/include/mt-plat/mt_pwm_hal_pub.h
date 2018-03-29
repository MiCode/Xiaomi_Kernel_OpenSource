/*
* This program is distributed and in hope it will be useful, but WITHOUT
* ANY WARRNTY; without even the implied warranty of MERCHANTABITLITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
*
********************************************************************************
* Author : Chagnlei Gao (changlei.gao@mediatek.com)
********************************************************************************
*/

#ifndef __MT_PWM_HAL_PUB_H__
#define __MT_PWM_HAL_PUB_H__

#include <linux/types.h>
#include <linux/platform_device.h>

/*********************************
*  Define Error Number
**********************************/
#define RSUCCESS 0
#define EEXCESSPWMNO 1
#define EPARMNOSUPPORT 2
#define ERROR 3
#define EBADADDR 4
#define EEXCESSBITS 5
#define EINVALID 6
#define EEXCESS4GADDR 7

#define PWM_DEBUG
#ifdef PWM_DEBUG
#define PWMDBG(fmt, args ...)  pr_debug("pwm %5d: " fmt, __LINE__, ##args)
#else
#define PWMDBG(fmt, args ...)
#endif

#define PWMMSG(fmt, args ...)  pr_debug(fmt, ##args)

#define PWM_DEVICE "mt-pwm"

#if !defined(CONFIG_MTK_LEGACY)
#ifdef CONFIG_OF
extern void __iomem *pwm_base;
#endif
#endif

void mt_pwm_power_on_hal(uint32_t pwm_no, bool pmic_pad, unsigned long *power_flag);
void mt_pwm_power_off_hal(uint32_t pwm_no, bool pmic_pad, unsigned long *power_flag);
void mt_pwm_init_power_flag(unsigned long *power_flag);
int32_t mt_pwm_sel_pmic_hal(uint32_t pwm_no);
int32_t mt_pwm_sel_ap_hal(uint32_t pwm_no);
void mt_set_pwm_enable_hal(uint32_t pwm_no);
void mt_set_pwm_disable_hal(uint32_t pwm_no);
void mt_set_pwm_enable_seqmode_hal(void);
void mt_set_pwm_disable_seqmode_hal(void);
int32_t mt_set_pwm_test_sel_hal(uint32_t val);
void mt_set_pwm_clk_hal(uint32_t pwm_no, uint32_t clksrc, uint32_t div);
int32_t mt_get_pwm_clk_hal(uint32_t pwm_no);
int32_t mt_set_pwm_con_datasrc_hal(uint32_t pwm_no, uint32_t val);
int32_t mt_set_pwm_con_mode_hal(uint32_t pwm_no, uint32_t val);
int32_t mt_set_pwm_con_idleval_hal(uint32_t pwm_no, uint16_t val);
int32_t mt_set_pwm_con_guardval_hal(uint32_t pwm_no, uint16_t val);
void mt_set_pwm_con_stpbit_hal(uint32_t pwm_no, uint32_t stpbit, uint32_t srcsel);
int32_t mt_set_pwm_con_oldmode_hal(uint32_t pwm_no, uint32_t val);
void mt_set_pwm_HiDur_hal(uint32_t pwm_no, uint16_t DurVal);
void mt_set_pwm_LowDur_hal(uint32_t pwm_no, uint16_t DurVal);
void mt_set_pwm_GuardDur_hal(uint32_t pwm_no, uint16_t DurVal);
void mt_set_pwm_send_data0_hal(uint32_t pwm_no, uint32_t data);
void mt_set_pwm_send_data1_hal(uint32_t pwm_no, uint32_t data);
void mt_set_pwm_wave_num_hal(uint32_t pwm_no, uint16_t num);
void mt_set_pwm_data_width_hal(uint32_t pwm_no, uint16_t width);
void mt_set_pwm_thresh_hal(uint32_t pwm_no, uint16_t thresh);
int32_t mt_get_pwm_send_wavenum_hal(uint32_t pwm_no);
void mt_set_intr_enable_hal(uint32_t pwm_intr_enable_bit);
int32_t mt_get_intr_status_hal(uint32_t pwm_intr_status_bit);
void mt_set_intr_ack_hal(uint32_t pwm_intr_ack_bit);

void mt_pwm_dump_regs_hal(void);

void pwm_debug_store_hal(void);
void pwm_debug_show_hal(void);

void mt_set_pwm_buf0_addr_hal(uint32_t pwm_no, dma_addr_t addr);
void mt_set_pwm_buf0_size_hal(uint32_t pwm_no, uint16_t size);

int mt_get_pwm_clk_src(struct platform_device *pdev);
#endif
