#ifndef _UAPI_EPM_ADC_H
#define _UAPI_EPM_ADC_H

struct epm_chan_request {
	/* EPM ADC device index. 0 - ADC1, 1 - ADC2 */
	uint32_t device_idx;
	/* Channel number within the EPM ADC device  */
	uint32_t channel_idx;
	/* The data meaningful for each individual channel whether it is
	 * voltage, current etc. */
	int32_t physical;
};

#define EPM_ADC_IOCTL_CODE		0x91

#define EPM_ADC_REQUEST		_IOWR(EPM_ADC_IOCTL_CODE, 1,	\
					struct epm_chan_request)

#define EPM_ADC_INIT		_IOR(EPM_ADC_IOCTL_CODE, 2,	\
					     uint32_t)

#define EPM_ADC_DEINIT		_IOR(EPM_ADC_IOCTL_CODE, 3,	\
					     uint32_t)
#endif /* _UAPI_EPM_ADC_H */
