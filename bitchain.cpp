#include "bitchain.hpp"

//#define BC_DEBUG

#ifdef BC_DEBUG
#define BC_MSG printf
#else
#define BC_MSG(...)
#endif

Bitchain::Bitchain(string fn, bool reader)
{
    isReader = reader;

    buf = (BC_UNIT*)malloc(BC_BUF_NBYTES);
    if(isReader){
        fp = fopen(fn.c_str(), "rb");
        fill();
        data = buf[0];
    }else{
        fp = fopen(fn.c_str(), "wb");
    }
}

Bitchain::~Bitchain()
{
    if(!isReader)
        fwrite(buf, buf_pos*sizeof(BC_UNIT), 1, fp);
    fclose(fp);
    free(buf);
}

Bitchain& Bitchain::write(uint64_t value, uint64_t bits)
{
    while( bits + bit_pos >= BC_BLEN){
        data |= (value >> (bits - (BC_BLEN - bit_pos)));
        buf[buf_pos++] = data;
        if(buf_pos == BC_BUF_NELEM){
            fwrite(buf, sizeof(BC_UNIT), BC_BUF_NELEM, fp);
            buf_pos = 0;
        }
        bits -= (BC_BLEN - bit_pos);
        data = 0;
        bit_pos = 0;
    }
    if(bits){
        data |= value << (BC_BLEN - (bit_pos + bits));
        bit_pos += bits;
    }

    return *this;
}

Bitchain& Bitchain::write_align(void)
{
    buf[buf_pos++] = data;
    if(buf_pos == BC_BUF_NELEM){
        fwrite(buf, sizeof(BC_UNIT), BC_BUF_NELEM, fp);
        buf_pos = 0;
    }
    data = 0;
    bit_pos = 0;

    return *this;
}

bool Bitchain::fill(void)
{
    if(!end){
        buf_fill = fread(buf, sizeof(BC_UNIT), BC_BUF_NELEM, fp);
        end = !(buf_fill == BC_BUF_NELEM);
        return true;
    }
    return false;
}


Bitchain& Bitchain::read_align(uint64_t &err)
{
    err = BC_OK;
    buf_pos++;
    if(buf_pos == buf_fill){
        if(fill())
            buf_pos = 0;
        else{
            err = BC_FILE_END;
            return *this;
        }
    }
    data = buf[buf_pos];
    bit_pos = 0;
    return *this;
}

Bitchain& Bitchain::readbits(uint64_t bits, uint64_t &value, uint64_t &err)
{
    value = 0;
    err = BC_OK;

    while( (bits + bit_pos) >= BC_BLEN){
        int nbit = (BC_BLEN - bit_pos);
        BC_MSG("B:%lX %lX %ld %d %lX\n", value, data, bits, bit_pos, (data & ((UINT64_C(1) << nbit) - 1)));
        uint64_t mask = (nbit == 64) ? ~0UL : ((UINT64_C(1) << nbit) - 1);
        value <<= nbit;
        value |= (data & mask);
        buf_pos++;
        if(buf_pos == buf_fill){
            if(fill()){
                buf_pos = 0;
            }else{
                err = BC_FILE_END;
                value = 0;
                return *this;
            }
        }
        data = buf[buf_pos];
        bit_pos = 0;
        bits -= nbit;
        BC_MSG("E:%lX %lX %ld %d\n", value, data, bits, bit_pos);
    }
    if(bits){
        value <<= bits;
        value |= (data >> (BC_BLEN - bits - bit_pos)) & ((UINT64_C(1) << bits) - 1);
        bit_pos += bits;
    }
    return *this;
}

Bitchain& Bitchain::getbits(uint64_t bits, uint64_t &value, uint64_t &err)
{
    value = 0;
    err = BC_OK;

    long offset = ftell(fp);
    int _buf_pos = buf_pos;
    int _bit_pos = bit_pos;
    BC_UNIT _data = data;

    while( bits + _bit_pos >= BC_BLEN){
        int nbit = (BC_BLEN - _bit_pos);
        uint64_t mask = (nbit == 64) ? ~0UL : ((UINT64_C(1) << nbit) - 1);
        value <<= nbit;
        value |= (_data & mask);
        _buf_pos++;
        if(_buf_pos >= buf_fill){
            BC_UNIT tmp;
            if(fread(&tmp, sizeof(BC_UNIT), 1, fp) == 0){
                err = BC_FILE_END;
                value = 0;
                return *this;
            }
            _data = tmp;
        }else{
            _data = buf[_buf_pos];
        }
        bits -= (BC_BLEN - _bit_pos);
        _bit_pos = 0;
    }
    if(bits){
        value <<= bits;
        value |= (_data >> (BC_BLEN - bits - _bit_pos)) & ((UINT64_C(1) << bits) - 1);
    }

    if(offset != ftell(fp))
        fseek(fp, offset - ftell(fp), SEEK_CUR);
    return *this;
}

Bitchain& Bitchain::skipbits(uint64_t bits, uint64_t &err)
{
    err = BC_OK;
    while( bits + bit_pos >= BC_BLEN){
        buf_pos++;
        if(buf_pos == buf_fill){
            if(fill()){
                buf_pos = 0;
            }else{
                err = BC_FILE_END;
                return *this;
            }
        }
        bits -= (BC_BLEN - bit_pos);
        bit_pos = 0;
    }
    data = buf[buf_pos];
    bit_pos += bits;
    return *this;
}

#ifdef BC_TEST
#include <unistd.h>
#include <time.h>

#define NUM_TESTS 1
#define NUM_ITEMS 32

int main(int ac, char **av)
{

    int num_tests = NUM_TESTS;
    int num_items = NUM_ITEMS;

    int opt;
    while ((opt = getopt(ac, av, "t:i:")) != -1) {
        switch(opt){
            case 't':
                num_tests = atoi(optarg);
                break;
            case 'i':
                num_items = atoi(optarg);
                break;
        }
    }

    printf("loop %d times, each %d r/w times\n", num_tests, num_items);

    uint64_t *bits = (uint64_t*)calloc(sizeof(uint64_t), num_items);
    uint64_t *values = (uint64_t*)calloc(sizeof(uint64_t), num_items);
    srand(time(NULL));

    for(int j = 0; j < num_tests; j++){
        {
            Bitchain ctx("test.bin", false);
            for(int i = 0; i < num_items; i++){
                bits[i] = (rand()%64) + 1;
                uint64_t mask = (bits[i] == 64) ? ~0UL : ((UINT64_C(1) << bits[i]) - 1);
                values[i] = ((uint64_t)rand()*rand()) & mask;
                ctx.write(values[i], bits[i]);
            }
            ctx.write_align();
        }

        {
            Bitchain ctx("test.bin", true);
            for(int i = 0; i < num_items; i++){
                uint64_t err;
                uint64_t value;
            #if 0
                ctx.readbits(bits[i], value, err);
            #else
                ctx.getbits(bits[i], value, err).skipbits(bits[i], err);
            #endif

                if(err != BC_OK){
                    printf("abnormal file end\n");
                }

                if(value != values[i])
                    printf("%d: bits:%lu %lX %lX\n", i, bits[i], value, values[i]);
            }
        }
    }

    free(bits);
    free(values);
    return 0;
}
#endif
