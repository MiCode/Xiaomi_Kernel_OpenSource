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

#include <mach/mt_typedefs.h>

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

#ifdef PWM_DEBUG
#define PWMDBG(fmt, args ...) printk(KERN_INFO "pwm %5d: " fmt, __LINE__, ##args)
#else
#define PWMDBG(fmt, args ...)
#endif

#define PWMMSG(fmt, args ...)  printk(KERN_INFO fmt, ##args)

#define PWM_DEVICE "mt-pwm"

void mt_pwm_power_on_hal(U32 pwm_no, BOOL pmic_pad, unsigned long *power_flag);
void mt_pwm_power_off_hal(U32 pwm_no, BOOL pmic_pad, unsigned long *power_flag);
void mt_pwm_init_power_flag(unsigned long *power_flag);
S32 mt_pwm_sel_pmic_hal(U32 pwm_no);
S32 mt_pwm_sel_ap_hal(U32 pwm_no);
void mt_set_pwm_enable_hal(U32 pwm_no);
void mt_set_pwm_disable_hal(U32 pwm_no);
void mt_set_pwm_enable_seqmode_hal(void);
void mt_set_pwm_disable_seqmode_hal(void);
S32 mt_set_pwm_test_sel_hal(U32 val);
void mt_set_pwm_clk_hal(U32 pwm_no, U32 clksrc, U32 div);
S32 mt_get_pwm_clk_hal(U32 pwm_no);
S32 mt_set_pwm_con_datasrc_hal(U32 pwm_no, U32 val);
S32 mt_set_pwm_con_mode_hal(U32 pwm_no, U32 val);
S32 mt_set_pwm_con_idleval_hal(U32 pwm_no, U16 val);
S32 mt_set_pwm_con_guardval_hal(U32 pwm_no, U16 val);
void mt_set_pwm_con_stpbit_hal(U32 pwm_no, U32 stpbit, U32 srcsel);
S32 mt_set_pwm_con_oldmode_hal(U32 pwm_no, U32 val);
void mt_set_pwm_HiDur_hal(U32 pwm_no, U16 DurVal);
void mt_set_pwm_LowDur_hal(U32 pwm_no, U16 DurVal);
void mt_set_pwm_GuardDur_hal(U32 pwm_no, U16 DurVal);
void mt_set_pwm_send_data0_hal(U32 pwm_no, U32 data);
void mt_set_pwm_send_data1_hal(U32 pwm_no, U32 data);
void mt_set_pwm_wave_num_hal(U32 pwm_no, U16 num);
void mt_set_pwm_data_width_hal(U32 pwm_no, U16 width);
void mt_set_pwm_thresh_hal(U32 pwm_no, U16 thresh);
S32 mt_get_pwm_send_wavenum_hal(U32 pwm_no);
void mt_set_intr_enable_hal(U32 pwm_intr_enable_bit);
S32 mt_get_intr_status_hal(U32 pwm_intr_status_bit);
void mt_set_intr_ack_hal(U32 pwm_intr_ack_bit);

void mt_pwm_dump_regs_hal(void);

void pwm_debug_store_hal(void);
void pwm_debug_show_hal(void);

void mt_set_pwm_buf0_addr_hal(U32 pwm_no, U32 addr);	//add by mtk
void mt_set_pwm_buf0_size_hal(U32 pwm_no, U16 size);	//add by mktk


#endif
