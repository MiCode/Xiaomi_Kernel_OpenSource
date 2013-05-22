#ifndef _IPT_NATTYPE_H_target
#define _IPT_NATTYPE_H_target

#define NATTYPE_TIMEOUT 300

enum nattype_mode {
	MODE_DNAT,
	MODE_FORWARD_IN,
	MODE_FORWARD_OUT
};

enum nattype_type {
	TYPE_PORT_ADDRESS_RESTRICTED,
	TYPE_ENDPOINT_INDEPENDENT,
	TYPE_ADDRESS_RESTRICTED
};


struct ipt_nattype_info {
	u_int16_t mode;
	u_int16_t type;
};

extern bool nattype_refresh_timer(unsigned long nattype,
unsigned long timeout_value);

#endif /*_IPT_NATTYPE_H_target*/

