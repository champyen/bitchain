#ifndef _BITCHAIN_H_
#define _BITCHAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if 0
#define BC_UNIT         uint8_t
#else
#define BC_UNIT         uint16_t
#endif

#define BC_BLEN         (sizeof(BC_UNIT)*8)
#define BC_BUF_NBYTES   8192
#define BC_BUF_NELEM    (BC_BUF_NBYTES/sizeof(BC_UNIT))

typedef struct
{
    FILE *fp;

    BC_UNIT data;
    int bit_pos;

    BC_UNIT *buf;
    int buf_pos;
    int buf_fill;
    int end;            // reader specific
} bc_context;

bc_context *bcw_open(char *);
void bcw_close(bc_context *);
void bcw_write(bc_context *, uint64_t, uint64_t);
void bcw_align(bc_context *);

bc_context *bcr_open(char *);
void bcr_close(bc_context *);
int64_t bcr_readbits(bc_context *, uint64_t);
int64_t bcr_align(bc_context *);
int64_t bcr_getbits(bc_context *, uint64_t);
int64_t bcr_skipbits(bc_context *, uint64_t);

#endif