#ifndef __UAPI_USB_CCID_BRIDGE_H
#define __UAPI_USB_CCID_BRIDGE_H

#include <linux/ioctl.h>

/**
 * struct usb_ccid_data - Used to receive the CCID class descriptor,
 *        clock rates and data rates supported by the device.
 * @length: The length of the buffer.
 * @data: The buffer as it is returned by the device for GET_DESCRIPTOR,
 *        GET_CLOCK_FREQUENCIES and GET_DATA_RATES requests.
 */
struct usb_ccid_data {
	uint8_t length;
	void *data;
};

/**
 * struct usb_ccid_abort - Used to abort an already sent command.
 * @seq: The sequence number of the command.
 * @slot: The slot of the IC, on which the command is sent.
 */
struct usb_ccid_abort {
	uint8_t seq;
	uint8_t slot;
};

#define USB_CCID_NOTIFY_SLOT_CHANGE_EVENT 1
#define USB_CCID_HARDWARE_ERROR_EVENT 2
#define USB_CCID_RESUME_EVENT 3
/**
 * struct usb_ccid_event - Used to receive notify slot change or hardware
 *        error event.
 * @notify: If the event is USB_CCID_NOTIFY_SLOT_CHANGE_EVENT, slot_icc_state
 *        has the information about the current slots state.
 * @error: If the event is USB_CCID_HARDWARE_ERROR_EVENT, error has
 *        information about the hardware error condition.
 */
struct usb_ccid_event {
	uint8_t event;
	union {
		struct {
			uint8_t slot_icc_state;
		} notify;

		struct {
			uint8_t slot;
			uint8_t seq;
			uint8_t error_code;
		} error;
	} u;
};

#define USB_CCID_GET_CLASS_DESC _IOWR('C', 0, struct usb_ccid_data)

#define USB_CCID_GET_CLOCK_FREQUENCIES _IOWR('C', 1, struct usb_ccid_data)

#define USB_CCID_GET_DATA_RATES _IOWR('C', 2, struct usb_ccid_data)

#define USB_CCID_ABORT _IOW('C', 3, struct usb_ccid_abort)

#define USB_CCID_GET_EVENT _IOR('C', 4, struct usb_ccid_event)

#endif /* __UAPI_USB_CCID_BRIDGE_H */
