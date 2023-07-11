
#ifndef __XM_BATTERY_AUTH_H
#define __XM_BATTERY_AUTH_H

int StringToHex(char *str, unsigned char *out, unsigned int *outlen);
int fg_sha256_auth(struct bq_fg_chip *bq, u8 *rand_num, int length);

#endif /* __XM_BATTERY_AUTH_H */

