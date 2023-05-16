#ifndef __ONEWIRE_GPIO_SLAVE_H__
#define __ONEWIRE_GPIO_SLAVE_H__

void Delay_us_slave(unsigned int T);
void Delay_ns_slave(unsigned int T);
unsigned char ow_reset_slave(void);
unsigned char read_byte_slave(void);
void write_byte_slave(char val);

#endif
