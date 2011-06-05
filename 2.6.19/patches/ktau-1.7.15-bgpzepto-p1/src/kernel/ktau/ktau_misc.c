
#include <linux/ktau/ktau_misc.h>

/* Efficiently find the Leftmost bit in a 32-bit int *
 * Adapted from: http://research.microsoft.com/invisible/tests/ff1.c.htm *
 */
/* 16-bit Lookup Table */
static signed char ktau_highbit[16] = {
    -1, /* 0x00 - no bits set */
    0,  /* 0x01 */
    1,  /* 0x02 */
    1,  /* 0x03 */
    2,  /* 0x04 */
    2,  /* 0x05 */
    2,  /* 0x06 */
    2,  /* 0x07 */
    3,  /* 0x08 */
    3,  /* 0x09 */
    3,  /* 0x0A */
    3,  /* 0x0B */
    3,  /* 0x0C */
    3,  /* 0x0D */
    3,  /* 0x0E */
    3   /* 0x0F */
};

/* 12 cycles using a 16 byte lut, 8 data accesses (but all in cache) */
/* This is the best one I think */
int ktau_leftmost1( unsigned int val)
{
    int ret[8];
    ret[0]=ktau_highbit[(val    )&0x0F];
    ret[1]=ktau_highbit[(val>> 4)&0x0F];
    ret[2]=ktau_highbit[(val>> 8)&0x0F];
    ret[3]=ktau_highbit[(val>>12)&0x0F];
    ret[4]=ktau_highbit[(val>>16)&0x0F];
    ret[5]=ktau_highbit[(val>>20)&0x0F];
    ret[6]=ktau_highbit[(val>>24)&0x0F];
    ret[7]=ktau_highbit[ val>>28      ];

    if (val>>  4) ret[0] = ret[1] +  4;
    if (val>>  8) ret[0] = ret[2] +  8;
    if (val>> 12) ret[0] = ret[3] + 12;
    if (val>> 16) ret[0] = ret[4] + 16;
    if (val>> 20) ret[0] = ret[5] + 20;
    if (val>> 24) ret[0] = ret[6] + 24;
    if (val>> 28) return ret[7] + 28;
    return ret[0];
}

