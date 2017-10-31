/*
 *Copyright 2015 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

#ifndef TFA_H_
#define TFA_H_

/* set the limit for the container file length */
#define TFA_MAX_CNT_LENGTH (256*1024)

/**
 * tfa error returns
 */
enum tfa_error {
	tfa_error_ok, /**< no error */
	tfa_error_device, /**< no response from device */
	tfa_error_bad_param, /**< parameter no accepted */
	tfa_error_noclock, /**< required clock not present */
	tfa_error_timeout, /**< a timeout occurred */
	tfa_error_dsp, /**< a DSP error was returned */
	tfa_error_container, /**< no or wrong container file */
	tfa_error_max /**< impossible value, max enum */
};

/**
 * Pass the container buffer, initialize and allocate internal memory.
 *
 * Note that this buffer will be kept and should not be freed until
 * tfa_deinit() has been called
 *
 * @param pointer to the start of the buffer holding the container file
 * @param length of the data in bytes
 * @return
 *  - tfa_error_ok if normal
 *  - tfa_error_container invalid container data
 *  - tfa_error_bad_param invalid parameter
 *
 */
enum tfa_error tfa_load_cnt(void *cnt, int length);

/**
 * Probe/init the device.
 *
 * This function should only be called when the container file is loaded.
 * It checks if an device with slave address is available in the container file.
 * When a container file is available its assigns an handle matching the index
 * in the container file and otherwise it will assign the first handle.
 * It will check the device type and fill the device specific structures and
 * functions.
 *
 * @param slave_address i2c slave address (8 bit format)
 * @param pDevice the index in the conainer file
 * @return enum tfa_error
 */
enum Tfa98xx_Error
tfa_probe(unsigned char slave_address, int *pDevice);

/**
 * Start/Restart the SpeakerBoost on all devices/channels.
 *
 * This should only be called when the audio input clock is active.\n
 * When the device is in coldstart-state (ACS=1) then a full initialization
 * will be performed.\n
 * In case of a warm start only a power-on and un-mute will be executed.\n
 *
 * @param profile the profile to load, if -1 then don't change profile
 * @param vsteps the volume step selections for each channel, if -1 then softmute
 *                        0 sets the maximum volume
 * @return enum tfa_error
 */
enum tfa_error tfa_start(int profile, int *vstep);

/**
 * Stop SpeakerBoost on all devices/channels.
 *
 * This the notification of the audio clock to be taken away by the host.
 *
 * Note that the function will block until the amplifiers are actually switched
 * off unless timed-out.
 *
 * @return enum tfa_error
 */
enum tfa_error tfa_stop(void);

/**
 * discard container buffer and free all resources.\n
 * This includes discarding all callbacks.
 */
void tfa_deinit(void);

/**
 * bring all devices/channels in the cold state (ACS==1).\n
 * This will cause reloading of all data at the next start
 *
 * @return
 *  - tfa_error_ok if normal
 *  - tfa_error_container invalid container data
 *  - tfa_error_device channel error
 *  - tfa_error_noclock only register level init could be preformed
 */
enum tfa_error tfa_reset(void);

enum Tfa98xx_Error tfa_write_filters(int dev_idx, int prof_idx);

#endif /* TFA_H_ */
