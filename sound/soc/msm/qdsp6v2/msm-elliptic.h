/**
 * Elliptic Labs
 */

#ifndef _MSM_PCM_ELUS
#define _MSM_PCM_ELUS

#include <sound/apr_elliptic.h>

extern wait_queue_head_t ultraSoundAPRWaitQueue;
extern struct afe_ultrasound_config_command *config;

unsigned int elliptic_add_platform_controls(void *platform);
int32_t process_us_payload(uint32_t *payload);

extern void elliptic_keep_sensor_system_awake(void);
#endif
