#ifndef _hive_isp_css_hrt_h
#define _hive_isp_css_hrt_h

#include "hive_isp_css_host_ids_hrt.h"

#ifdef HRT_ISP_CSS_CUSTOM_HOST
#ifndef HRT_USE_VIR_ADDRS
#define HRT_USE_VIR_ADDRS
#endif
/*#include "hive_isp_css_custom_host_hrt.h"*/
#endif

#include <timed_controller.h>
#include <gpio_block.h>
#include <gp_regs.h>
#ifdef _HIVE_ISP_CSS_FPGA_SYSTEM
#include <i2c_api.h> // specific for css_dev, not used in ghanius
#include <dis_sensor.h>
#include <display_driver.h>
#include <display.h>
#include <display_driver.h>
#include <shi_sensor_api.h>
#ifdef _HIVE_ISP_CSS_PI_SYSTEM
#include <isp2300_medfield_demo_pi_params.h>
#else
#include <isp2300_medfield_demo_params.h>
#endif
#include <isp2300_support.h>
#include "isp_css_dev_flash_hrt.h"
#include "isp_css_dev_display_hrt.h"
#include "isp_css_dev_i2c_hrt.h"
#ifdef _HIVE_ISP_CSS_FPGA_MIPI_SYSTEM
#include "isp_css_mipi_tb.h"
#include <css_receiver_ahb_hrt.h>
#elif defined _HIVE_ISP_CSS_PI_SYSTEM
#include "hive_isp_css_pi_system.h"
#else
#include "isp_css_dev_tb.h"
#endif
#else
#include <css_receiver_ahb_hrt.h>
#ifdef _HIVE_ISP_CSS_2310_SYSTEM
#include <isp2310_medfield_params.h>
#include <isp2310_support.h>
#else
#include <isp2300_medfield_params.h>
#include <isp2300_support.h>
#endif
/* insert idle signal clearing and setting around hrt_main */
#if !defined(HRT_HW) || defined(HRT_ISP_CSS_INSERT_IDLE_SIGNAL)
#define hrt_main _hrt_isp_css_main
#endif
#ifdef _HIVE_ISP_CSS_2310_SYSTEM
#include "hive_isp_css_system_2310.h"
#else
#include "hive_isp_css_system.h"
#endif
#endif
#include <sp_hrt.h>
#include <stream2memory.h>
#include <sig_monitor_hrt.h>
#include <test_pat_gen.h>

#include "hive_isp_css_defs.h"
#include "hive_isp_css_stream_switch_hrt.h"
#include "hive_isp_css_input_selector_hrt.h"
#include "hive_isp_css_input_switch_hrt.h"
#include "hive_isp_css_sync_gen_hrt.h"
#include "hive_isp_css_prbs_hrt.h"
#include "hive_isp_css_tpg_hrt.h"
#include "hive_isp_css_sdram_wakeup_hrt.h"
#include "hive_isp_css_idle_signal_hrt.h"
#include "hive_isp_css_sp_hrt.h"
#include "hive_isp_css_isp_hrt.h"
#include "hive_isp_css_ddr_hrt.h"
#include "hive_isp_css_streaming_to_mipi_hrt.h"
#include "hive_isp_css_testbench_hrt.h"
#include "hive_isp_css_streaming_monitors_hrt.h"
#include "hive_isp_css_dma_set_hrt.h"
#include "hive_isp_css_gp_regs_hrt.h"

#endif /* _hive_isp_css_hrt_h */
