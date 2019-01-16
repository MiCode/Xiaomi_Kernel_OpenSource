#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/rtpm_prio.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "disp_drv_log.h"

#include "mt_boot.h"
#include "disp_helper.h"
#include "disp_drv_platform.h"

// use this magic_code to detect memory corruption
#define MAGIC_CODE 0xDEADAAA0U

// CONFIG_MTK_FPGA is used in linux kernel for early porting.
// if the macro name changed, please modify the code here too.
#ifdef CONFIG_MTK_FPGA
static unsigned int disp_global_stage = MAGIC_CODE | DISP_HELPER_STAGE_EARLY_PORTING;
#else
// please change this to DISP_HELPER_STAGE_NORMAL after bring up done
static unsigned int disp_global_stage = MAGIC_CODE | DISP_HELPER_STAGE_NORMAL;
#endif

static _is_E1(void)
{
	CHIP_SW_VER ver = mt_get_chip_sw_ver();
	if(CHIP_SW_VER_01 == ver)
		return 1;

	return 0;
}

static _is_E2(void)
{
	CHIP_SW_VER ver = mt_get_chip_sw_ver();
	if(CHIP_SW_VER_02 == ver)
		return 1;

	return 0;
}

static _is_E3(void)
{
	return !(_is_E1() || _is_E2());
}

static _is_early_porting_stage(void)
{
	return ((disp_global_stage&(~MAGIC_CODE)) == DISP_HELPER_STAGE_EARLY_PORTING);
}

static _is_bringup_stage(void)
{
	return ((disp_global_stage&(~MAGIC_CODE)) == DISP_HELPER_STAGE_BRING_UP);
}

static _is_normal_stage(void)
{
	return ((disp_global_stage&(~MAGIC_CODE)) == DISP_HELPER_STAGE_NORMAL);
}

static int screen_idle_switch_decouple = 1;
void enable_screen_idle_switch_decouple()
{
	screen_idle_switch_decouple = 1;
}
void disable_screen_idle_switch_decouple()
{
	screen_idle_switch_decouple = 0;
}

// FIXME: should include header file here.
extern UINT32 DISP_GetScreenWidth(void);
extern UINT32 DISP_GetScreenHeight(void);

static int _disp_helper_option_value[DISP_HELPER_OPTION_NUM] = {0};

const char *disp_helper_option_spy(DISP_HELPER_OPTION option)
{
	switch(option)
	{
		case DISP_HELPER_OPTION_USE_CMDQ: 							return "USE_CMDQ";
		case DISP_HELPER_OPTION_USE_M4U: 							return "USE_M4U";
		case DISP_HELPER_OPTION_USE_CLKMGR: 						return "USE_CLKMGR";
		case DISP_HELPER_OPTION_MIPITX_ON_CHIP: 					return "MIPITX_ON_CHIP";
		case DISP_HELPER_OPTION_USE_DEVICE_TREE: 					return "USE_DEVICE_TREE";
		case DISP_HELPER_OPTION_FAKE_LCM_X: 						return "FAKE_LCM_X";
		case DISP_HELPER_OPTION_FAKE_LCM_Y: 						return "FAKE_LCM_Y";
		case DISP_HELPER_OPTION_FAKE_LCM_WIDTH: 					return "FAKE_LCM_WIDTH";
		case DISP_HELPER_OPTION_FAKE_LCM_HEIGHT: 					return "FAKE_LCM_HEIGHT";
		case DISP_HELPER_OPTION_OVL_WARM_RESET: 					return "OVL_WARM_RESET";
		case DISP_HELPER_OPTION_DYNAMIC_SWITCH_UNDERFLOW_EN: 		return "DYNAMIC_SWITCH_UNDERFLOW_EN";
		case DISP_HELPER_OPTION_IDLEMGR_SWTCH_DECOUPLE: 			return "IDLEMGR_SWTCH_DECOUPLE";
		case DISP_HELPER_OPTION_IDLEMGR_DISABLE_ROUTINE_IRQ: 		return "IDLEMGR_DISABLE_ROUTINE_IRQ";
		case DISP_HELPER_OPTION_DECOUPLE_MODE_USE_RGB565: 			return "DECOUPLE_MODE_USE_RGB565";
		case DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH: 			return "TWO_PIPE_INTERFACE_PATH";
		case DISP_HELPER_OPTION_NO_LCM_FOR_LOW_POWER_MEASUREMENT: 	return "NO_LCM_FOR_LOW_POWER_MEASUREMENT";
		case DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE:				return "DEFAULT_DECOUPLE_MODE";
		case DISP_HELPER_OPTION_DISPLAY_PATH_DEBUG_PATTERN:			return "DISPLAY_PATH_DEBUG_PATTERN";
		case DISP_HELPER_OPTION_AUTO_1L_DL:							return "AUTO_1L_DL";
		default: 													return "unknown";
	}
}

int disp_helper_set_option(DISP_HELPER_OPTION option, int value)
{
	if(option < DISP_HELPER_OPTION_NUM)
	{
		DISPCHECK("Set Option %d(%s) from (%d) to (%d)\n", option, disp_helper_option_spy(option), disp_helper_get_option(option), value);
		_disp_helper_option_value[option] = value;
		DISPCHECK("After set (%s) is (%d)\n", disp_helper_option_spy(option), disp_helper_get_option(option));
	}
	else
	{
		DISPERR("Wrong option: %d\n", option);
	}
}

int disp_helper_get_option(DISP_HELPER_OPTION option)
{
	//DISPMSG("stage=0x%08x\n", disp_global_stage);
	switch(option)
	{
		case DISP_HELPER_OPTION_USE_CMDQ:
		{
			if(_is_normal_stage())
				return 1;
			else if(_is_bringup_stage())
				return 0;
			else if(_is_early_porting_stage())
				return 0;
			else
				BUG_ON(1);
		}
		case DISP_HELPER_OPTION_USE_M4U:
		{
			if(_is_normal_stage())
				return 1;
			else if(_is_bringup_stage())
				return 0;
			else if(_is_early_porting_stage())
				return 0;
			else
				BUG_ON(1);
		}
		case DISP_HELPER_OPTION_USE_CLKMGR:
		{
			if(_is_normal_stage())
				return 1;
			else if(_is_bringup_stage())
				return 0;
			else if(_is_early_porting_stage())
				return 0;
			else
				BUG_ON(1);
		}
		case DISP_HELPER_OPTION_MIPITX_ON_CHIP:
		{
			if(_is_normal_stage())
				return 1;
			else if(_is_bringup_stage())
				return 1;
			else if(_is_early_porting_stage())
				return 0;
			else
				BUG_ON(1);
		}			
		case DISP_HELPER_OPTION_FAKE_LCM_X:
		{
			int x = 0;
			#ifdef CONFIG_CUSTOM_LCM_X
				x = simple_strtoul(CONFIG_CUSTOM_LCM_X, NULL, 0);
			#endif
			return x;
		}			
		case DISP_HELPER_OPTION_FAKE_LCM_Y:
		{
			int y = 0;
			#ifdef CONFIG_CUSTOM_LCM_Y
				y = simple_strtoul(CONFIG_CUSTOM_LCM_Y, NULL, 0);
			#endif
			return y;
		}	
		case DISP_HELPER_OPTION_FAKE_LCM_WIDTH:
		{
			int x = 0;
			int w = DISP_GetScreenWidth();
			#ifdef CONFIG_CUSTOM_LCM_X
				x = simple_strtoul(CONFIG_CUSTOM_LCM_X, NULL, 0);
				if(x != 0)
				{
					w = ALIGN_TO(w, 16);
				}
			#endif
			return w;
		}			
		case DISP_HELPER_OPTION_FAKE_LCM_HEIGHT:
		{
			int h = DISP_GetScreenHeight();
			return h;
		}
		case DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE:
		{
#ifdef WQHD_VCORE10_SUPPORT
			return 1;
#else
			return 0;
#endif
		}
		case DISP_HELPER_OPTION_AUTO_1L_DL:
		{
#ifdef WQHD_VCORE10_SUPPORT
			return 1;
#else
			return 0;
#endif
		}
		default:
		{
			return _disp_helper_option_value[option];
		}
	}

	return ;
}

DISP_HELPER_STAGE disp_helper_get_stage(void)
{
	return (disp_global_stage&(~MAGIC_CODE));
}

const char *disp_helper_stage_spy(void)
{
	if(disp_helper_get_stage() == DISP_HELPER_STAGE_EARLY_PORTING)
		return "EARLY_PORTING";
	else if(disp_helper_get_stage() == DISP_HELPER_STAGE_BRING_UP)
		return "BRINGUP";
	else if(disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL)
		return "NORMAL";
}

void disp_helper_option_init(void)
{
	/* test solution for 6795 rdma underflow caused by ufoe LR mode(ufoe fifo is larger than rdma) */
	disp_helper_set_option(DISP_HELPER_OPTION_DYNAMIC_SWITCH_UNDERFLOW_EN, 		0);
	
	/* warm reset ovl before each trigger for cmd mode */
	disp_helper_set_option(DISP_HELPER_OPTION_OVL_WARM_RESET, 					1);
	
	/* switch to decouple mode for screen idle, only for video mode */
	disp_helper_set_option(DISP_HELPER_OPTION_IDLEMGR_SWTCH_DECOUPLE, 			1);
	
	/* disable routine irq for screen idle */
	disp_helper_set_option(DISP_HELPER_OPTION_IDLEMGR_DISABLE_ROUTINE_IRQ, 		1);
	
	/* use rdma0->dsi0, rdma2->dsi1 for 6795 video mode */
	disp_helper_set_option(DISP_HELPER_OPTION_TWO_PIPE_INTERFACE_PATH, 			0);
	
	/* use fake vsync timer for low power measurement */
	disp_helper_set_option(DISP_HELPER_OPTION_NO_LCM_FOR_LOW_POWER_MEASUREMENT, 0);
	
	/* use RGB565 format for decouple mode intermediate buffer */
	disp_helper_set_option(DISP_HELPER_OPTION_DECOUPLE_MODE_USE_RGB565, 		0);

	/* use decouple mode by default */
	disp_helper_set_option(DISP_HELPER_OPTION_DEFAULT_DECOUPLE_MODE, 			1);

	/* enable display path debug pattern by default for htc test*/
	disp_helper_set_option(DISP_HELPER_OPTION_DISPLAY_PATH_DEBUG_PATTERN, 		0);

	/* auto switch to directlink mode 1.0v for ovl 1 layer */
	disp_helper_set_option(DISP_HELPER_OPTION_AUTO_1L_DL, 						1);
}

int disp_helper_get_option_list(char* stringbuf, int buf_len)
{
	int len = 0;
	int i = 0;
	for(i=0;i<DISP_HELPER_OPTION_NUM;i++)
	{
		DISPMSG("Option: [%s] Value: [%d]\n", disp_helper_option_spy(i), disp_helper_get_option(i));
		len += scnprintf(stringbuf+len, buf_len - len, "Option: [%d][%s] Value: [%d]\n", i, disp_helper_option_spy(i), disp_helper_get_option(i));	
	}
	
	return len;
}
