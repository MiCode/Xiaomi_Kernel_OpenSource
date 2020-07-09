/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFACONTAINER_H_
#define TFACONTAINER_H_

/* static limits */
#define TFACONT_MAXDEVS  (4)   /* maximum nr of devices */
#define TFACONT_MAXPROFS (16) /* maximum nr of profiles */

#include "tfa98xx_parameters.h"

/**
* Pass the container buffer, initialize and allocate internal memory.
*
* @param cnt pointer to the start of the buffer holding the container file
* @param length of the data in bytes
* @return
*  - tfa_error_ok if normal
*  - tfa_error_container invalid container data
*  - tfa_error_bad_param invalid parameter
*
*/
enum tfa_error tfa_load_cnt(void *cnt, int length);

/**
 * Return the descriptor string
 * @param cnt pointer to the container struct
 * @param dsc pointer to nxpTfa descriptor
 * @return descriptor string
 */
char *tfaContGetString(nxpTfaContainer_t *cnt, nxpTfaDescPtr_t *dsc);

/**
 * Gets the string for the given command type number
 * @param type number representing a command
 * @return string of a command
 */
char *tfaContGetCommandString(uint32_t type);

/**
 * get the device type from the patch in this devicelist
 *  - find the patch file for this devidx
 *  - return the devid from the patch or 0 if not found
 * @param cnt pointer to container file
 * @param dev_idx device index
 * @return descriptor string
 */
int tfa_cnt_get_devid(nxpTfaContainer_t *cnt, int dev_idx);

/**
 * Get the slave for the device if it exists.
 * @param tfa the device struct pointer
 * @param slave_addr the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContGetSlave(struct tfa_device *tfa, uint8_t *slave_addr);

void tfaContSetSlave(uint8_t slave_addr);

/**
 * Get the index for a skave address.
 * @param tfa the device struct pointer
 * @return the device index
 */
int tfa_cont_get_idx(struct tfa_device *tfa);

/**
 * Write reg and bitfield items in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteRegsDev(struct tfa_device *tfa);

/**
 * Write  reg  and bitfield items in the profilelist to the target.
 * @param tfa the device struct pointer
 * @param prof_idx the profile index
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteRegsProf(struct tfa_device *tfa, int prof_idx);

/**
 * Write a patchfile in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWritePatch(struct tfa_device *tfa);

/**
 * Write all  param files in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteFiles(struct tfa_device *tfa);

/**
 * Get sample rate from passed profile index
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @return sample rate value
 */
unsigned int tfa98xx_get_profile_sr(struct tfa_device *tfa, unsigned int prof_idx);

/**
 * Get the device name string
 * @param cnt the pointer to the container struct
 * @param dev_idx the index of the device
 * @return device name string or error string if not found
 */
char *tfaContDeviceName(nxpTfaContainer_t *cnt, int dev_idx);

/**
 * Get the application name from the container file application field
 * @param tfa the device struct pointer
 * @param name the input stringbuffer with size: sizeof(application field)+1
 * @return actual string length
 */
int tfa_cnt_get_app_name(struct tfa_device *tfa, char *name);

/**
 * Get profile index of the calibration profile
 * @param tfa the device struct pointer
 * @return profile index, -2 if no calibration profile is found or -1 on error
 */
int tfaContGetCalProfile(struct tfa_device *tfa);

/**
 * Is the profile a tap profile ?
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @return 1 if the profile is a tap profile or 0 if not
 */
int tfaContIsTapProfile(struct tfa_device *tfa, int prof_idx);

/**
 * Get the name of the profile at certain index for a device in the container file
 * @param cnt the pointer to the container struct
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile name string or error string if not found
 */
char *tfaContProfileName(nxpTfaContainer_t *cnt, int dev_idx, int prof_idx);

/**
 * Process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 * @param tfa the device struct pointer
 * @param prof_idx index of the profile
 * @param vstep_idx index of the vstep
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteProfile(struct tfa_device *tfa, int prof_idx, int vstep_idx);

/**
 * Specify the speaker configurations (cmd id) (Left, right, both, none)
 * @param dev_idx index of the device
 * @param configuration name string of the configuration
 */
void tfa98xx_set_spkr_select(int dev_idx, char *configuration);

enum Tfa98xx_Error tfa_cont_write_filterbank(struct tfa_device *tfa, nxpTfaFilter_t *filter);

/**
 * Write all  param files in the profilelist to the target
 * this is used during startup when maybe ACS is set
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @param vstep_idx the index of the vstep
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteFilesProf(struct tfa_device *tfa, int prof_idx, int vstep_idx);
enum Tfa98xx_Error tfaContWriteFilesVstep(struct tfa_device *tfa, int prof_idx, int vstep_idx);
enum Tfa98xx_Error tfaContWriteDrcFile(struct tfa_device *tfa, int size, uint8_t data[]);

/**
 * Get the device list dsc from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @return device list pointer
 */
nxpTfaDeviceList_t *tfaContGetDevList(nxpTfaContainer_t *cont, int dev_idx);

/**
 * Get the Nth profile for the Nth device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile list pointer
 */
nxpTfaProfileList_t *tfaContGetDevProfList(nxpTfaContainer_t *cont, int dev_idx, int prof_idx);

/**
 * Get the number of profiles for device from contaienr
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @return device list pointer
 */
int tfa_cnt_get_dev_nprof(struct tfa_device *tfa);


/**
 * Get the Nth livedata for the Nth device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param livedata_idx the index of the livedata
 * @return livedata list pointer
 */
nxpTfaLiveDataList_t *tfaContGetDevLiveDataList(nxpTfaContainer_t *cont, int dev_idx, int livedata_idx);

/**
 * Check CRC for container
 * @param cont pointer to the tfaContainer
 * @return error value 0 on error
 */
int tfaContCrcCheckContainer(nxpTfaContainer_t *cont);

/**
 * Get the device list pointer
 * @param cnt pointer to the container struct
 * @param dev_idx the index of the device
 * @return pointer to device list
 */
nxpTfaDeviceList_t *tfaContDevice(nxpTfaContainer_t *cnt, int dev_idx);

/**
 * Return the pointer to the first profile in a list from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @return pointer to first profile in profile list
 */
nxpTfaProfileList_t *tfaContGet1stProfList(nxpTfaContainer_t *cont);

/**
 * Return the pointer to the next profile in a list
 * @param prof is the pointer to the profile list
 * @return profile list pointer
 */
nxpTfaProfileList_t *tfaContNextProfile(nxpTfaProfileList_t *prof);

/**
 * Return the pointer to the first livedata in a list from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @return pointer to first livedata in profile list
 */
nxpTfaLiveDataList_t *tfaContGet1stLiveDataList(nxpTfaContainer_t *cont);

/**
 * Return the pointer to the next livedata in a list
 * @param livedata_idx is the pointer to the livedata list
 * @return livedata list pointer
 */
nxpTfaLiveDataList_t *tfaContNextLiveData(nxpTfaLiveDataList_t *livedata_idx);

/**
 * Write a bit field
 * @param tfa the device struct pointer
 * @param bf bitfield to write
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaRunWriteBitfield(struct tfa_device *tfa,  nxpTfaBitfield_t bf);

/**
 * Write a parameter file to the device
 * @param tfa the device struct pointer
 * @param file filedescriptor pointer
 * @param vstep_idx index to vstep
 * @param vstep_msg_idx index to vstep message
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteFile(struct tfa_device *tfa,  nxpTfaFileDsc_t *file, int vstep_idx, int vstep_msg_idx);

/**
 * Get the max volume step associated with Nth profile for the Nth device
 * @param tfa the device struct pointer
 * @param prof_idx profile index
 * @return the number of vsteps
 */
int tfacont_get_max_vstep(struct tfa_device *tfa, int prof_idx);

/**
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
 * @param tfa the device struct pointer
 * @param prof_idx I2C profile index in the device
 * @param type file type
 * @return 0 NULL if file type is not found
 * @return 1 file contents
 */
nxpTfaFileDsc_t *tfacont_getfiledata(struct tfa_device *tfa, int prof_idx, enum nxpTfaHeaderType type);

/**
 * Dump the contents of the file header
 * @param hdr pointer to file header data
 */
void tfaContShowHeader(nxpTfaHeader_t *hdr);

/**
 * Read a bit field
 * @param tfa the device struct pointer
 * @param bf bitfield to read out
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaRunReadBitfield(struct tfa_device *tfa,  nxpTfaBitfield_t *bf);

/**
 * Get hw feature bits from container file
 * @param tfa the device struct pointer
 * @param hw_feature_register pointer to where hw features are stored
 */
void get_hw_features_from_cnt(struct tfa_device *tfa, int *hw_feature_register);

/**
 * Get sw feature bits from container file
 * @param tfa the device struct pointer
 * @param sw_feature_register pointer to where sw features are stored
 */
void get_sw_features_from_cnt(struct tfa_device *tfa, int sw_feature_register[2]);

/**
 * Factory trimming for the Boost converter
 * check if there is a correction needed
 * @param tfa the device struct pointer
 */
enum Tfa98xx_Error tfa98xx_factory_trimmer(struct tfa_device *tfa);

/**
 * Search for filters settings and if found then write them to the device
 * @param tfa the device struct pointer
 * @param prof_idx profile to look in
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfa_set_filters(struct tfa_device *tfa, int prof_idx);

/**
 * Get the firmware version from the patch in the container file
 * @param tfa the device struct pointer
 * @return firmware version
 */
int tfa_cnt_get_patch_version(struct tfa_device *tfa);

int tfa_tib_dsp_msgmulti(struct tfa_device *tfa, int length, const char *buffer);

#endif /* TFACONTAINER_H_ */
