#include "accdet_custom_def.h"
#include <accdet_custom.h>

//key press customization: long press time
struct headset_key_custom headset_key_custom_setting = {
	2000
};

struct headset_key_custom* get_headset_key_custom_setting(void)
{
	return &headset_key_custom_setting;
}

#if defined  ACCDET_EINT || defined ACCDET_EINT_IRQ
static struct headset_mode_settings cust_headset_settings = {
	0x900, 0x200, 1, 0x1f0, 0x800, 0x800, 0x20
};
#else
//ACCDET only mode register settings
static struct headset_mode_settings cust_headset_settings = {
	0x900, 0x600, 1, 0x5f0, 0x3000, 0x3000, 0x400
};
#endif

struct headset_mode_settings* get_cust_headset_settings(void)
{
	return &cust_headset_settings;
}