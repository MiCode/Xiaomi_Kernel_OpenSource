#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <asm/memblock.h>
#include <mach/sec_osal.h>
#include <mach/mt_sec_export.h>
#include <mach/mt_boot.h>
#include <asm/setup.h>
#include <linux/of_fdt.h>
#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>
#include "port_kernel.h"
#include "ccci_support.h"

static struct ccci_setting ccci_cfg_setting;

void ccci_reload_md_type(struct ccci_modem *md, int type)
{
	if(type != md->config.load_type) {
		set_modem_support_cap(md->index,type);
		md->config.load_type = type;
		md->config.setting |= MD_SETTING_RELOAD;
	}
}

struct ccci_setting* ccci_get_common_setting(int md_id)
{
#ifdef EVDO_DT_SUPPORT
	unsigned char*	str_slot1_mode = MTK_TELEPHONY_BOOTUP_MODE_SLOT1;
	unsigned char*	str_slot2_mode = MTK_TELEPHONY_BOOTUP_MODE_SLOT2;
	ccci_cfg_setting.slot1_mode = str_slot1_mode[0] - '0';
	ccci_cfg_setting.slot2_mode = str_slot2_mode[0] - '0';
#endif
    return &ccci_cfg_setting;
}

int ccci_store_sim_switch_mode(struct ccci_modem *md, int simmode)
{
    if (ccci_cfg_setting.sim_mode!= simmode) {
        ccci_cfg_setting.sim_mode = simmode;
        ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_CFG_UPDATE, 1);
    } else {
        CCCI_INF_MSG(md->index, CORE, "same sim mode as last time(0x%x)\n", simmode);
    }
    return 0;
}

int ccci_get_sim_switch_mode(void)
{
   return ccci_cfg_setting.sim_mode;
}
