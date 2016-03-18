#ifndef _UAPI__MSM_ADC_H
#define _UAPI__MSM_ADC_H

#include <linux/sched.h>

#define MSM_ADC_MAX_CHAN_STR 64

/* must be <= to the max buffer size in the modem implementation */
#define MSM_ADC_DEV_MAX_INFLIGHT 9

#define MSM_ADC_IOCTL_CODE		0x90

struct msm_adc_conversion {
	/* hwmon channel number - this is not equivalent to the DAL chan */
	uint32_t chan;
	/* returned result in ms */
	int result;
};

struct adc_chan_result {
	/* The channel number of the requesting/requested conversion */
	uint32_t chan;
	/* The pre-calibrated digital output of a given ADC relative to the
	ADC reference */
	int32_t adc_code;
	/* in units specific for a given ADC; most ADC uses reference voltage
	 *  but some ADC uses reference current.  This measurement here is
	 *  a number relative to a reference of a given ADC */
	int64_t measurement;
	/* The data meaningful for each individual channel whether it is
	 * voltage, current, temperature, etc. */
	int64_t physical;
};

/*
 * Issue a blocking adc conversion request. Once the call returns, the data
 * can be found in the 'physical' field of adc_chan_result. This call will
 * return ENODATA if there is an invalid result returned by the modem driver.
 */
#define MSM_ADC_REQUEST			_IOWR(MSM_ADC_IOCTL_CODE, 1,	\
					     struct adc_chan_result)

/*
 * Issue a non-blocking adc conversion request. The results from this
 * request can be obtained by calling AIO_READ once the transfer is
 * completed. To verify completion, the blocking call AIO_POLL can be used.
 * If there are no slot resources, this call will return an error with errno
 * set to EWOULDBLOCK.
 */
#define MSM_ADC_AIO_REQUEST		_IOWR(MSM_ADC_IOCTL_CODE, 2,	\
					     struct adc_chan_result)

/*
 * Same non-blocking semantics as AIO_REQUEST, except this call will block
 * if there are no available slot resources. This call can fail with errno
 * set to EDEADLK if there are no resources and the file descriptor in question
 * has outstanding conversion requests already. This is done so the client
 * does not block on resources that can only be freed by reading the results --
 * effectively deadlocking the system. In this case, the client must read
 * pending results before proceeding to free up resources.
 */
#define MSM_ADC_AIO_REQUEST_BLOCK_RES	_IOWR(MSM_ADC_IOCTL_CODE, 3,	\
					     struct adc_chan_result)

/*
 * Returns the number of pending results that are associated with a particular
 * file descriptor. If there are no pending results, this call will block until
 * there is at least one. If there are no requests queued at all on this file
 * descriptor, this call will fail with EDEADLK. This is to prevent deadlock in
 * a single-threaded scenario where POLL would never return.
 */
#define MSM_ADC_AIO_POLL		_IOR(MSM_ADC_IOCTL_CODE, 4,	\
					     uint32_t)

#define MSM_ADC_FLUID_INIT	_IOR(MSM_ADC_IOCTL_CODE, 5,	\
					     uint32_t)

#define MSM_ADC_FLUID_DEINIT	_IOR(MSM_ADC_IOCTL_CODE, 6,	\
					     uint32_t)

struct msm_adc_aio_result {
	uint32_t chan;
	int result;
};

/*
 * Read the results from an AIO / non-blocking conversion request. AIO_POLL
 * should be used before using this command to verify how many pending requests
 * are available for the file descriptor. This call will fail with errno set to
 * ENOMSG if there are no pending messages to be read at the time of the call.
 * The call will return ENODATA if there is an invalid result returned by the
 * modem driver.
 */
#define MSM_ADC_AIO_READ		_IOR(MSM_ADC_IOCTL_CODE, 5,	\
					     struct adc_chan_result)

struct msm_adc_lookup {
	/* channel name (input) */
	char name[MSM_ADC_MAX_CHAN_STR];
	/* local channel index (output) */
	uint32_t chan_idx;
};

/*
 * Look up a channel name and get back an index that can be used
 * as a parameter to the conversion request commands.
 */
#define MSM_ADC_LOOKUP			_IOWR(MSM_ADC_IOCTL_CODE, 6,	\
					     struct msm_adc_lookup)

#endif /* _UAPI_MSM_ADC_H */
