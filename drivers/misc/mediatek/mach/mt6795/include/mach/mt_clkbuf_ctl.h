/**
* @file    mt_clk_buf_ctl.c
* @brief   Driver for RF clock buffer control
*
*/
#ifndef __MT_CLK_BUF_CTL_H__
#define __MT_CLK_BUF_CTL_H__

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <cust_clk_buf.h>

/* //FIXME, for build error
#ifndef GPIO68
#define GPIO68 68
#endif
#ifndef GPIO69
#define GPIO69 69
#endif
#ifndef GPIO70
#define GPIO70 70
#endif
#ifndef GPIO71
#define GPIO71 71
#endif
#ifndef GPIO72
#define GPIO72 72
#endif
*/

#ifndef GPIO_RFIC0_BSI_CK
#define GPIO_RFIC0_BSI_CK         (GPIO68|0x80000000)    /* RFIC0_BSI_CK = GPIO68 */
#endif
#ifndef GPIO_RFIC0_BSI_D0
#define GPIO_RFIC0_BSI_D0         (GPIO69|0x80000000)    /* RFIC0_BSI_D0 = GPIO69 */
#endif
#ifndef GPIO_RFIC0_BSI_D1
#define GPIO_RFIC0_BSI_D1         (GPIO70|0x80000000)    /* RFIC0_BSI_D1 = GPIO70 */
#endif
#ifndef GPIO_RFIC0_BSI_D2
#define GPIO_RFIC0_BSI_D2         (GPIO71|0x80000000)    /* RFIC0_BSI_D2 = GPIO71 */
#endif
#ifndef GPIO_RFIC0_BSI_CS
#define GPIO_RFIC0_BSI_CS         (GPIO72|0x80000000)    /* RFIC0_BSI_CS = GPIO72 */
#endif


enum clk_buf_id{
    CLK_BUF_BB			= 0,
    CLK_BUF_6605		= 1,
    CLK_BUF_5193		= 2,
    CLK_BUF_AUDIO		= 3,
    CLK_BUF_INVALID		= 4,
};
typedef enum
{
   CLK_BUF_SW_DISABLE = 0,
   CLK_BUF_SW_ENABLE  = 1,
}CLK_BUF_SWCTRL_STATUS_T;
#define CLKBUF_NUM         4 
#if 0
typedef struct{
   CLK_BUF_SWCTRL_STATUS_T	ClkBuf_SWCtrl_Status[CLKBUF_NUM];
}CLKBUF_SWCTRL_RESULT_T;
#endif

#define STA_CLK_ON      1
#define STA_CLK_OFF     0

bool clk_buf_ctrl(enum clk_buf_id id,bool onoff);
void clk_buf_get_swctrl_status(CLK_BUF_SWCTRL_STATUS_T *status);
bool clk_buf_init(void);

extern struct mutex clk_buf_ctrl_lock;
//extern DEFINE_MUTEX(clk_buf_ctrl_lock);

#endif

