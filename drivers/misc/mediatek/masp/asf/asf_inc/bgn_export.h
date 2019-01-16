#ifndef _BIGNUM_EXPORT_H
#define _BIGNUM_EXPORT_H

typedef struct
{
    int s;
    int n;
    unsigned long *p;
}
bgn;


/**************************************************************************
 *  EXPORT FUNCTIONS
 **************************************************************************/
int bgn_read_bin( bgn *X, const unsigned char *buf, int buflen );
int bgn_write_bin( const bgn *X, unsigned char *buf, int buflen );
int bgn_read_str( bgn *X, int radix, const char *s, int length );
int bgn_exp_mod( bgn *X, const bgn *E, const bgn *N, bgn *_RR );


/**************************************************************************
 *  ERROR CODE
 **************************************************************************/
#define E_BGN_FILE_IOEOR                        0x0001
#define E_BGN_BAD_INPUT_DATA                    0x0002
#define E_BGN_INVALID_CHARACTER                 0x0003
#define E_BGN_BUFFER_TOO_SMALL                  0x0004
#define E_BGN_NEGATIVE_VALUE                    0x0005
#define E_BGN_DIVISION_BY_ZERO                  0x0006
#define E_BGN_NOT_ACCEPTABLE                    0x0007

#endif /* bgn.h */
