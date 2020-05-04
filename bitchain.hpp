#ifndef _BITCHAIN_H_
#define _BITCHAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>

using namespace std;

#ifndef BC_UNIT
#define BC_UNIT         uint64_t
#endif

#define BC_BLEN         (sizeof(BC_UNIT)*8)
#define BC_BUF_NBYTES   8192
#define BC_BUF_NELEM    (BC_BUF_NBYTES/sizeof(BC_UNIT))

typedef enum
{
    BC_OK = 0,
    BC_FILE_END,
} bc_retcode;

class Bitchain
{
public:
    Bitchain(string, bool);
    ~Bitchain();

    Bitchain& write(uint64_t, uint64_t);
    Bitchain& write_align();

    Bitchain& readbits(uint64_t, uint64_t &, uint64_t &);
    Bitchain& read_align(uint64_t &);
    Bitchain& getbits(uint64_t, uint64_t &, uint64_t &);
    Bitchain& skipbits(uint64_t, uint64_t &);

private:
    bool fill(void);

    bool isReader = false;
    FILE *fp = NULL;

    BC_UNIT data = 0;
    int bit_pos = 0;

    BC_UNIT *buf = NULL;
    int buf_pos = 0;
    int buf_fill = 0;
    bool end = false;            // reader specific
};


#endif // _BITCHAIN_H_
