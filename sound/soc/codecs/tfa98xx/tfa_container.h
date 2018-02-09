/*
 * tfaContainer.h
 *
 *  Created on: Sep 11, 2013
 *      Author: wim
 */

#ifndef TFACONTAINER_H_
#define TFACONTAINER_H_

/* static limits */
#define TFACONT_MAXDEVS  (4)   /* maximum nr of devices */
#define TFACONT_MAXPROFS (16) /* maximum nr of profiles */

#include "tfa98xx_parameters.h"

/**
 * Enable/Disable partial messaging feature
 * @param enp 0 disable partial update
 */
void tfa_set_partial_update(int enp);

/**
 * Check the container file and set module global
 * @param cnt pointer to container file
 * @param length the length of the container file
 * @return tfa_error
 */
enum tfa_error tfa_load_cnt(void *cnt, int length);

/**
 * Resets init variables
 */
void tfa_deinit(void);

/**
 * Verify the calibration results from each channel
 * @param handle the index of the device
 */
void individual_calibration_results(Tfa98xx_handle_t handle);

/**
 * Return the descriptor string
 * @param dsc pointer to nxpTfa descriptor
 * @return descriptor string
 */
char *tfaContGetString(nxpTfaDescPtr_t *dsc);

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
int tfa_cnt_get_devid(nxpTfaContainer_t *cnt, int dev_idx) ;

/**
 * Get the number of devices from the container
 * @return number of devices
 */
int tfa98xx_cnt_max_device(void);

/**
 * Set verbosity level
 * @param level used as boolean
 */
void tfa_cnt_verbose(int level);

/**
 * Return the pointer to the loaded container file
 * @return pointer to container, NULL if not loaded.
 */
nxpTfaContainer_t *tfa98xx_get_cnt(void);

/**
 * Lookup slave and return device index
 * @param slave_addr address of the slave device
 * @return device index
 */
int tfa98xx_cnt_slave2idx(int slave_addr);

/**
 * Lookup slave and return device revid.
 * @param slave_addr address of the slave device
 * @return device revid
 */
int tfa98xx_cnt_slave2revid(int slave_addr);

/**
 * Get the slave for the device if it exists.
 * @param dev_idx the index of the device
 * @param slave the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContGetSlave(int dev_idx, uint8_t *slave_addr);

/**
 * Write reg and bitfield items in the devicelist to the target.
 * @param device the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteRegsDev(int dev_idx);

/**
 * Write  reg  and bitfield items in the profilelist to the target.
 * @param device the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteRegsProf(int dev_idx, int prof_idx);

/**
 * Write a patchfile in the devicelist to the target.
 * @param dev_idx the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWritePatch(int dev_idx);

/**
 * Write all  param files in the devicelist to the target.
 * @param dev_idx the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteFiles(int dev_idx);

/**
 * Get sample rate from passed profile index
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return sample rate value
 */
unsigned int tfa98xx_get_profile_sr(int dev_idx, unsigned int prof_idx);

/**
 * Open the specified device after looking up the target address.
 * @param dev_idx the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContOpen(int dev_idx);

/**
 * Close the  device.
 * @param dev_idx the index of the device
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContClose(int dev_idx);

/**
 * Get the device name string
 * @param dev_idx the index of the device
 * @return device name string or error string if not found
 */
char  *tfaContDeviceName(int dev_idx);

/**
 * Get the application name from the container file application field
 * @param name the input stringbuffer with size: sizeof(application field)+1
 * @return actual string length
 */
int tfa_cnt_get_app_name(char *name);

/**
 * Get profile index of the calibration profile
 * @param dev_idx the index of the device
 * @return profile index, -2 if no calibration profile is found or -1 on error
 */
int tfaContGetCalProfile(int dev_idx);

/**
 * Is the profile a tap profile ?
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return 1 if the profile is a tap profile or 0 if not
 */
int tfaContIsTapProfile(int dev_idx, int prof_idx);

/**
 * Get the name of the profile at certain index for a device in the container file
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile name string or error string if not found
 */
char  *tfaContProfileName(int dev_idx, int prof_idx);

/**
 * Get the number of profiles for a device
 * @param dev_idx index of the device
 * @return the profile count
 */
int tfaContMaxProfile(int dev_idx);

/**
 * Process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 * @param dev_idx index of the device
 * @param prof_idx index of the profile
 * @param vstep_idx index of the vstep
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteProfile(int dev_idx, int prof_idx, int vstep_idx);

/**
 * Specify the speaker configurations (cmd id) (Left, right, both, none)
 * @param dev_idx index of the device
 * @param configuration name string of the configuration
 */
void tfa98xx_set_spkr_select(Tfa98xx_handle_t dev_idx, char *configuration);

/**
 * Set current vstep for a given channel
 * @param vstep_idx index of the vstep
 * @param channel index of the channel
 */
void tfaContSetCurrentVstep(int channel, int vstep_idx);

/**
 * Get current vstep for a given channel
 * @param channel index of the channel
 */
int tfaContGetCurrentVstep(int channel);

enum Tfa98xx_Error tfa_cont_write_filterbank(int dev_idx, nxpTfaFilter_t *filter);

/**
 * Write all  param files in the profilelist to the target
 * this is used during startup when maybe ACS is set
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @param vstep_idx the index of the vstep
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteFilesProf(int dev_idx, int prof_idx, int vstep_idx);


enum Tfa98xx_Error tfaContWriteFilesVstep(int dev_idx, int prof_idx, int vstep_idx);
enum Tfa98xx_Error tfaContWriteDrcFile(int dev_idx, int size, uint8_t data[]);

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
 * @param dev_idx the index of the device
 * @return pointer to device list
 */
nxpTfaDeviceList_t *tfaContDevice(int dev_idx);

/**
 * Return the pointer to the profile in a list
 * @param dev_idx the index of the device
 * @param prof_ipx the index of the profile
 * @return profile list pointer
 */
nxpTfaProfileList_t *tfaContProfile(int dev_idx, int prof_ipx);

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
 * @param dev_idx device index
 * @param bf bitfield to write
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaRunWriteBitfield(Tfa98xx_handle_t dev_idx,  nxpTfaBitfield_t bf);

/**
 * Write a parameter file to the device
 * @param dev_idx device index
 * @param file filedescriptor pointer
 * @param vstep_idx index to vstep
 * @param vstep_msg_idx index to vstep message
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaContWriteFile(int dev_idx,  nxpTfaFileDsc_t *file, int vstep_idx, int vstep_msg_idx);

/**
 * Get the max volume step associated with Nth profile for the Nth device
 * @param dev_idx device index
 * @param prof_idx profile index
 * @return the number of vsteps
 */
int tfacont_get_max_vstep(int dev_idx, int prof_idx);

/**
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
 * @param dev_idx I2C device index
 * @param prof_idx I2C profile index in the device
 * @param type file type
 * @return 0 NULL if file type is not found
 * @return 1 file contents
 */
nxpTfaFileDsc_t *tfacont_getfiledata(int dev_idx, int prof_idx, enum nxpTfaHeaderType type);

/**
 * Dump the contents of the file header
 * @param hdr pointer to file header data
 */
void tfaContShowHeader(nxpTfaHeader_t *hdr);

/**
 * Read a bit field
 * @param dev_idx device index
 * @param bf bitfield to read out
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfaRunReadBitfield(Tfa98xx_handle_t dev_idx,  nxpTfaBitfield_t *bf);

/**
 * Get hw feature bits from container file
 * @param dev_idx device index
 * @param hw_feature_register pointer to where hw features are stored
 */
void get_hw_features_from_cnt(Tfa98xx_handle_t dev_idx, int *hw_feature_register);

/**
 * Get sw feature bits from container file
 * @param dev_idx device index
 * @param sw_feature_register pointer to where sw features are stored
 */
void get_sw_features_from_cnt(Tfa98xx_handle_t dev_idx, int sw_feature_register[2]);

/**
 * Factory trimming for the Boost converter
 * check if there is a correction needed
 * @param dev_idx device index
 */
void tfa_factory_trimmer(Tfa98xx_handle_t dev_idx);

/**
 * Search for filters settings and if found then write them to the device
 * @param dev_idx device index
 * @param prof_idx profile to look in
 * @return Tfa98xx_Error
 */
enum Tfa98xx_Error tfa_set_filters(int dev_idx, int prof_idx);

#endif /* TFACONTAINER_H_ */
