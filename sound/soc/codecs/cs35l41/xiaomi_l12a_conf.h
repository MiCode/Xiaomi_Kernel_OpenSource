#ifndef __XIAOMI_L3_H__
#define __XIAOMI_L3_H__

#ifdef __cplusplus
extern "C" {
#endif

struct cirrus *cirrus_this;
static struct mixer_cell unload_path[] = {
    {"SPK DSP Booted", "0"},
    {"SPK DSP1 Preload Switch", "0"},
    {"SPK AMP Enable Switch", "0"},

    {"RCV DSP Booted", "0"},
    {"RCV DSP1 Preload Switch", "0"},
    {"RCV AMP Enable Switch", "0"},
};

static struct mixer_cell cal_path[] = {
    {"SPK Ambient Temperature", "30"},
    {"SPK DSP1 Firmware", "Calibration"},
    {"SPK DSP1 Preload Switch", "1"},
    {"SPK PCM Source", "DSP"},
    {"SPK AMP Enable Switch", "1"},

    {"RCV Ambient Temperature", "30"},
    {"RCV DSP1 Firmware", "Calibration"},
    {"RCV DSP1 Preload Switch", "1"},
    {"RCV PCM Source", "DSP"},
    {"RCV AMP Enable Switch", "1"},
};

static struct mixer_cell cal_dsp_patch_path[] = {
};

static struct mixer_cell diag_path[] = {
    {"SPK Ambient Temperature", "30"},
    {"SPK DSP1 Firmware", "Diagnostic"},
    {"SPK DSP1 Preload Switch", "1"},
    {"SPK PCM Source", "DSP"},
    {"SPK AMP Enable Switch", "1"},

    {"RCV Ambient Temperature", "30"},
    {"RCV DSP1 Firmware", "Diagnostic"},
    {"RCV DSP1 Preload Switch", "1"},
    {"RCV PCM Source", "DSP"},
    {"RCV AMP Enable Switch", "1"},
};

static struct mixer_cell bypass_no_path[] = {
    {"SPK PCM Source", "DSP"},
    {"RCV PCM Source", "DSP"},
};

static struct mixer_cell bypass_yes_path[] = {
    {"SPK PCM Source", "ASP"},
    {"RCV PCM Source", "ASP"},
};

static struct mixer_cell diag_dsp_patch_path[] = {
};

static struct mixer_cell rtlog_enable_path[] = {
    {"SPK DSP1 Protection cd RTLOG_ENABLE", "00 00 00 00"},
    {"RCV DSP1 Protection cd RTLOG_ENABLE", "00 00 00 00"},

    //6F 39F 3AC 3AD
    {"SPK DSP1 Protection cd RTLOG_VARIABLE", "0x00 0x00 0x00 0x6F 0x00 0x00 0x03 0x9F 0x00 0x00 0x03 0xAC 0x00 0x00 0x03 0xAD"},
    {"RCV DSP1 Protection cd RTLOG_VARIABLE", "0x00 0x00 0x00 0x6F 0x00 0x00 0x03 0x9F 0x00 0x00 0x03 0xAC 0x00 0x00 0x03 0xAD"},

    {"SPK DSP1 Protection cd RTLOG_COUNT", "00 00 00 04"},
    {"RCV DSP1 Protection cd RTLOG_COUNT", "00 00 00 04"},

    {"SPK DSP1 Protection cd RTLOG_ENABLE", "00 00 00 01"},
    {"RCV DSP1 Protection cd RTLOG_ENABLE", "00 00 00 01"},
};
static struct mixer_cell rtlog_disable_path[] = {
    {"SPK DSP1 Protection cd RTLOG_ENABLE", "00 00 00 00"},
    {"RCV DSP1 Protection cd RTLOG_ENABLE", "00 00 00 00"},
};

static struct mixer_cell rtlog_state_clean_path[] = {
    {"SPK DSP1 Protection cd RTLOG_STATE", "00 00 00 00"},
    {"RCV DSP1 Protection cd RTLOG_STATE", "00 00 00 00"},
};

static struct mixer_cell normal_path[] = {
    {"SPK DSP1 Firmware", "Protection"},
    {"SPK DSP1 Preload Switch", "1"},

    {"RCV DSP1 Firmware", "Protection"},
    {"RCV DSP1 Preload Switch", "1"},
};

static struct mixer_cell boot_preload[] = {
    {"SPK DSP1 Firmware", "Protection"},
    {"SPK DSP1 Preload Switch", "1"},
    {"SPK DSP1 Boot Switch", "1"},

    {"RCV DSP1 Firmware", "Protection"},
    {"RCV DSP1 Preload Switch", "1"},
    {"RCV DSP1 Boot Switch", "1"},
};

static struct mixer_string mixer_str_cali_b = {
    .str_calr =         "RCV DSP1 Calibration cd CAL_R",
    .str_status =       "RCV DSP1 Calibration cd CAL_STATUS",
    .str_chksum =       "RCV DSP1 Calibration cd CAL_CHECKSUM",
    .str_ambient =      "RCV DSP1 Calibration cd CAL_AMBIENT",
    .str_f0status =     "RCV DSP1 Calibration cd DIAG_F0_STATUS",
    .str_f0 =           "RCV DSP1 Calibration cd DIAG_F0",
    .str_f0ldiff =      "RCV DSP1 Calibration cd DIAG_Z_LOW_DIFF",
    .str_cspl_sts =     "RCV DSP1 Calibration cd CSPL_STATE",
};

static struct mixer_string mixer_str_cali_t = {
    .str_calr =         "SPK DSP1 Calibration cd CAL_R",
    .str_status =       "SPK DSP1 Calibration cd CAL_STATUS",
    .str_chksum =       "SPK DSP1 Calibration cd CAL_CHECKSUM",
    .str_ambient =      "SPK DSP1 Calibration cd CAL_AMBIENT",
    .str_f0status =     "SPK DSP1 Calibration cd DIAG_F0_STATUS",
    .str_f0 =           "SPK DSP1 Calibration cd DIAG_F0",
    .str_f0ldiff =      "SPK DSP1 Calibration cd DIAG_Z_LOW_DIFF",
    .str_cspl_sts =     "SPK DSP1 Calibration cd CSPL_STATE",
};
static struct mixer_string mixer_str_diag_b = {
    .str_calr =         "RCV DSP1 Diagnostic cd CAL_R",
    .str_status =       "RCV DSP1 Diagnostic cd CAL_STATUS",
    .str_chksum =       "RCV DSP1 Diagnostic cd CAL_CHECKSUM",
    .str_ambient =      "RCV DSP1 Diagnostic cd CAL_AMBIENT",
    .str_f0status =     "RCV DSP1 Diagnostic cd DIAG_F0_STATUS",
    .str_f0 =           "RCV DSP1 Diagnostic cd DIAG_F0",
    .str_f0ldiff =      "RCV DSP1 Diagnostic cd DIAG_Z_LOW_DIFF",
    .str_cspl_sts =     "RCV DSP1 Diagnostic cd CSPL_STATE",
};

static struct mixer_string mixer_str_diag_t = {
    .str_calr =         "SPK DSP1 Diagnostic cd CAL_R",
    .str_status =       "SPK DSP1 Diagnostic cd CAL_STATUS",
    .str_chksum =       "SPK DSP1 Diagnostic cd CAL_CHECKSUM",
    .str_ambient =      "SPK DSP1 Diagnostic cd CAL_AMBIENT",
    .str_f0status =     "SPK DSP1 Diagnostic cd DIAG_F0_STATUS",
    .str_f0 =           "SPK DSP1 Diagnostic cd DIAG_F0",
    .str_f0ldiff =      "SPK DSP1 Diagnostic cd DIAG_Z_LOW_DIFF",
    .str_cspl_sts =     "SPK DSP1 Diagnostic cd CSPL_STATE",
};
static struct mixer_string mixer_str_prot_b = {
    .str_calr =         "RCV DSP1 Protection cd CAL_R",
    .str_status =       "RCV DSP1 Protection cd CAL_STATUS",
    .str_chksum =       "RCV DSP1 Protection cd CAL_CHECKSUM",
    .str_ambient =      "RCV DSP1 Protection cd CAL_AMBIENT",
    .str_chswap =       "RCV DSP1 Protection cd CH_BAL",
    .str_cspl_sts =     "RCV DSP1 Protection cd CSPL_STATE",
    .str_zstore =       "RCV Calibration Resistance",
    .str_active =       "RCV AMP Active Status",
    .str_log_rw =       "RCV AMP WR Registers",
};

static struct mixer_string mixer_str_prot_t = {
    .str_calr =         "SPK DSP1 Protection cd CAL_R",
    .str_status =       "SPK DSP1 Protection cd CAL_STATUS",
    .str_chksum =       "SPK DSP1 Protection cd CAL_CHECKSUM",
    .str_ambient =      "SPK DSP1 Protection cd CAL_AMBIENT",
    .str_chswap =       "SPK DSP1 Protection cd CH_BAL",
    .str_cspl_sts =     "SPK DSP1 Protection cd CSPL_STATE",
    .str_zstore =       "SPK Calibration Resistance",
    .str_active =       "SPK AMP Active Status",
    .str_log_rw =       "SPK AMP WR Registers",
};

static struct mixer_string mixer_str_qdsp = {
   .str_qdsp_chswap = "Cirrus SP Channel Swap",
   .str_qdsp_delta = "Cirrus SP Delta",
   .str_qdsp_conf = "Cirrus SP Load Rx Config",
};

/* *
 * number of channel defined by [channels_desc], [cali_mixers], [diag_mixers] and [prot_mixers]
 * all the array must be placed in order of [channels_desc]
 * */
static char* channels_desc[] = {"SPK", "RCV"};
static struct mixer_string* cali_mixers[] = {&mixer_str_cali_t, &mixer_str_cali_b};
static struct mixer_string* diag_mixers[] = {&mixer_str_diag_t, &mixer_str_diag_b};
static struct mixer_string* prot_mixers[] = {&mixer_str_prot_t, &mixer_str_prot_b};

#define MAX_CHANNELS 2
static int expect_calr_range[MAX_CHANNELS][2] = {
    [0] = {8000, 12000},
    [1] = {8000, 12000},
};

#define RETRY_COUNT 1      /* N - try N time if calibraion fail */
#define ASOC_CARD 100      /* ASoC sound card number */
#define ASOC_DEVICE 100      /* ASoC sound device number */
#define BACKEND "TDM-LPAIF-RX-TERTIARY-VIRT-0"
#define FW_WAIT 0   //1500000    /* microseconds - wait for the end of loading firmware */
#define PLAY_WAIT 0 //1000000  /* microseconds - wait for the end of dummy playback */
#define CAL_SILENT_WAV "/vendor/etc/silent-3sec.wav"
#define CALIB_FILE_BIN "/data/audio/crus_calr.bin"
#define CALIB_FILE_TXT "/data/audio/crus_calr.txt"
/* No used in L1 project */
#define F0_FILE_BIN "/mnt/vendor/persist/audio/crus_f0.bin"

#ifndef
#define MTK_PLATFORM
#endif
#ifdef __cplusplus
}
#endif

#endif //__XIAOMI_L3_H__
