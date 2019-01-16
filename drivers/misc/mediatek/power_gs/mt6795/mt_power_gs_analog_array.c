#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_power_gs.h>

const unsigned int AP_ANALOG_gs_dpidle_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_dpidle = AP_ANALOG_gs_dpidle_data;

unsigned int AP_ANALOG_gs_dpidle_len = 9;

const unsigned int AP_ANALOG_gs_suspend_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_suspend = AP_ANALOG_gs_suspend_data;

unsigned int AP_ANALOG_gs_suspend_len = 9;

const unsigned int AP_ANALOG_gs_vp_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_vp = AP_ANALOG_gs_vp_data;

unsigned int AP_ANALOG_gs_vp_len = 9;

const unsigned int AP_ANALOG_gs_paging_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_paging = AP_ANALOG_gs_paging_data;

unsigned int AP_ANALOG_gs_paging_len = 9;

const unsigned int AP_ANALOG_gs_mp3_play_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_mp3_play = AP_ANALOG_gs_mp3_play_data;

unsigned int AP_ANALOG_gs_mp3_play_len = 9;

const unsigned int AP_ANALOG_gs_idle_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_idle = AP_ANALOG_gs_idle_data;

unsigned int AP_ANALOG_gs_idle_len = 9;

const unsigned int AP_ANALOG_gs_clkon_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_clkon = AP_ANALOG_gs_clkon_data;

unsigned int AP_ANALOG_gs_clkon_len = 9;

const unsigned int AP_ANALOG_gs_talk_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_talk = AP_ANALOG_gs_talk_data;

unsigned int AP_ANALOG_gs_talk_len = 9;

const unsigned int AP_ANALOG_gs_connsys_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_connsys = AP_ANALOG_gs_connsys_data;

unsigned int AP_ANALOG_gs_connsys_len = 9;

const unsigned int AP_ANALOG_gs_mne_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000000 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_mne = AP_ANALOG_gs_mne_data;

unsigned int AP_ANALOG_gs_mne_len = 9;

const unsigned int AP_ANALOG_gs_datalink_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_datalink = AP_ANALOG_gs_datalink_data;

unsigned int AP_ANALOG_gs_datalink_len = 9;

const unsigned int AP_ANALOG_gs_vr_data[] = {
 // Address     Mask        Golden Setting Value
    0xF000F644, 0x00000100, 0x00000000,// MEMPLL_LDOA
    0xF0012644, 0x00000100, 0x00000000,// MEMPLL_LDOB
    0xF0209604, 0xFFFFFFFF, 0x00000030 // TEMPERATURE
};

const unsigned int *AP_ANALOG_gs_vr = AP_ANALOG_gs_vr_data;

unsigned int AP_ANALOG_gs_vr_len = 9;

