#include "sec_typedef.h"
#include "sec_boot.h"

/**************************************************************************
 *  DEFINITIONS
 **************************************************************************/
#define MOD                         "SEC_KEY_UTIL"

/**************************************************************************
 *  KEY SECRET
 **************************************************************************/
#define ENCODE_MAGIC                (0x1)

void sec_decode_key(uchar* key, uint32 key_len, uchar* seed, uint32 seed_len)
{
    uint32 i = 0;

    for(i=0; i<key_len; i++)
    {
        key[i] -= seed[i%seed_len];
        key[i] -= ENCODE_MAGIC;
    }
}
