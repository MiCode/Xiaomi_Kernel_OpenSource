#ifndef __CSDEF_H__
#define __CSDEF_H__

#include "cirrus_cal.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MTK_PLATFORM
#define AHAL_INFO ALOGI
#define AHAL_ERR ALOGE
#define AHAL_DBG ALOGD

#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) \
    (sizeof(array) / sizeof(array[0]))
#endif

#define xSTDOUT_DEBUG
#if defined(STDOUT_DEBUG)
#undef AHAL_INFO
#undef AHAL_ERR
#define AHAL_INFO(...) fprintf(stdout, __VA_ARGS__)
#define AHAL_ERR(...) fprintf(stderr, __VA_ARGS__)
#endif

#define xTIME_MEASURE
#if defined (TIME_MEASURE)
#define MEAS_TIME_INIT() \
    struct timeval tval_before, tval_after, tval_result; \
    gettimeofday(&tval_before, NULL)

#define MEAS_TIME(msg) \
    gettimeofday(&tval_after, NULL); \
    timersub(&tval_after, &tval_before, &tval_result); \
    AHAL_INFO("%s: %s took: %ld.%06ld seconds\n", __func__, msg, (long int)tval_result.tv_sec, (long int)tval_result.tv_usec); \
    gettimeofday(&tval_before, NULL)
#else
#define MEAS_TIME_INIT() (0)
#define MEAS_TIME(msg) (0)
#endif
#define TEST_WAV "/sdcard/Music/lrp_loop.wav"
#define CRUS_IOCTL_NODE "/dev/msm_cirrus_playback"

typedef void (*func_cal_init_t)(struct cirrus *cs);
typedef void (*func_cal_exit_t)(struct cirrus *cs);

//struct cirrus;
struct cirrus {
    struct mixer_string *ctls[MAX_CHANNELS];
    struct mixer_string *cal_ctls[MAX_CHANNELS];
    struct mixer_string *diag_ctls[MAX_CHANNELS];
    struct mixer_string *qdsp_ctls;
    struct rtlog rtlogs[MAX_CHANNELS];
    struct cal_result_t cal_result[MAX_CHANNELS];
    unsigned int calr_default[MAX_CHANNELS];
    unsigned int calr_range[MAX_CHANNELS][2];
    pthread_mutex_t calib_mutex;
    pthread_t boot_thread;
    //volatile bool thread_exit;
    bool initialized;
    unsigned int card; /* Asoc sound card number */
    unsigned int device;/* Asoc device number */
    char *backend; /* backend dai name */
    bool f0_enable;
    void *libcrussp;
    int (*dummy_playback)(struct cirrus *cs);
    char *wav_file;
    char *calr_bin;
    char *calr_txt;
    char *f0_file;
    int (*fwrite)(unsigned int *buf, size_t size, char *file);
    int (*fread)(unsigned int *buf, size_t size, char *file);
    int (*ascii_fwrite)(unsigned int *buf, size_t size, char *file);
    int (*channel_swap)(bool swap);
    int channels; /* channels on device */
    int calib_state; /* state of calibraion process*/
    bool calib_valid; /* state of calibraion value*/
    char *channels_desc[MAX_CHANNELS];
    int ch_swap;
    void (*calib_exit)(void);
    int (*calib_run)(void);
    int (*channel_do_swap)(bool swap);
    int (*cal_run_diagnostic)(void);
    int (*cal_fstore)(unsigned int *buf, size_t chs);
    int (*cal_fread)(unsigned int *buf, size_t chs);
    int (*cal_get_result)(float *buf, size_t chs);
    int (*cal_apply)(unsigned int *buf, size_t chs);
    int (*cal_str_format_out)(char *buf, size_t size);
};

#ifdef __cplusplus
}
#endif

#endif
