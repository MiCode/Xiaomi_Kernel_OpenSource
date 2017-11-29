#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#define AUDIO_SERVICE_IOCTL_MAGIC 'a'

#define AUDIO_SERVICE_CAL_SET _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, 1, void*)
#define AUDIO_SERVICE_CAL_GET _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, 2, void*)
#define AUDIO_SERVICE_PRESET_SET _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, 3, void*)
#define AUDIO_SERVICE_ULTRASOUND_SET _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, 4, void*)
#define AUDIO_SERVICE_ULTRASOUND_GET _IOWR(AUDIO_SERVICE_IOCTL_MAGIC, 5, void*)

extern int opalum_afe_set_calibration_data(int lowTemp, int highTemp);
extern int opalum_afe_set_preset(int preset);

#endif
