#ifndef __ONEWIRE_GPIO_H__
#define __ONEWIRE_GPIO_H__

void Delay_us(unsigned int T);
void Delay_ns(unsigned int T);
unsigned char ow_reset(void);
unsigned char read_byte(void);
void write_byte(char val);
void Software_Reset(void);

#endif
