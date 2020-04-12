#include "bitchain.h"

//#define BC_DEBUG

#ifdef BC_DEBUG
#define BC_MSG printf
#else
#define BC_MSG
#endif

bc_context *bcw_open(char *fn)
{
    bc_context *ctx = (bc_context*)calloc(1, sizeof(bc_context));
    ctx->fp = fopen(fn, "wb");
    ctx->buf = (BC_UNIT*)malloc(BC_BUF_NBYTES);
    return ctx;
}

void bcw_close(bc_context *ctx)
{
    if(ctx){
        // flush buffer
        // pending
        fwrite(ctx->buf, ctx->buf_pos*sizeof(BC_UNIT), 1, ctx->fp);
        fclose(ctx->fp);
        free(ctx);
    }
}

void bcw_write(bc_context *ctx, uint64_t bits, uint64_t data)
{
    while( bits + ctx->bit_pos >= BC_BLEN){
        ctx->data |= (data >> (bits - (BC_BLEN - ctx->bit_pos)));
        ctx->buf[ctx->buf_pos++] = ctx->data;
        if(ctx->buf_pos == BC_BUF_NELEM){
            fwrite(ctx->buf, sizeof(BC_UNIT), BC_BUF_NELEM, ctx->fp);
            ctx->buf_pos = 0;
        }
        bits -= (BC_BLEN - ctx->bit_pos);
        ctx->data = 0;
        ctx->bit_pos = 0;
    }
    if(bits){
        ctx->data |= data << (BC_BLEN - (ctx->bit_pos + bits));
        ctx->bit_pos += bits;
    }
}

void bcw_align(bc_context *ctx)
{
    ctx->buf[ctx->buf_pos++] = ctx->data;
    if(ctx->buf_pos == BC_BUF_NELEM){
        fwrite(ctx->buf, sizeof(BC_UNIT), BC_BUF_NELEM, ctx->fp);
        ctx->buf_pos = 0;
    }
    ctx->data = 0;
    ctx->bit_pos = 0;
}

int _bcr_fill(bc_context *ctx)
{
    if(ctx->end == 0){
        ctx->buf_fill = fread(ctx->buf, sizeof(BC_UNIT), BC_BUF_NELEM, ctx->fp);
        ctx->end = ctx->buf_fill == BC_BUF_NELEM ? 0 : 1;
        return 1;
    }
    return 0;
}

bc_context *bcr_open(char *fn)
{
    bc_context *ctx = (bc_context*)calloc(1, sizeof(bc_context));
    ctx->buf = (BC_UNIT*)malloc(BC_BUF_NBYTES);
    ctx->fp = fopen(fn, "rb");
    _bcr_fill(ctx);
    ctx->data = ctx->buf[0];
    return ctx;
}

void bcr_close(bc_context *ctx)
{
    if(ctx){
        fclose(ctx->fp);
        free(ctx);
    }
}

void bcr_align(bc_context *ctx, uint64_t *err)
{
    *err = BC_OK;
    ctx->buf_pos++;
    if(ctx->buf_pos == ctx->buf_fill){
        if(_bcr_fill(ctx))
            ctx->buf_pos = 0;
        else{
            *err = BC_FILE_END;
            return;
        }
    }
    ctx->data = ctx->buf[ctx->buf_pos];
    ctx->bit_pos = 0;
}

uint64_t bcr_readbits(bc_context *ctx, uint64_t bits, uint64_t *err)
{
    uint64_t value = 0;
    *err = BC_OK;

    while( (bits + ctx->bit_pos) >= BC_BLEN){
        int nbit = (BC_BLEN - ctx->bit_pos);
        BC_MSG("B:%lX %lX %ld %d %lX\n", value, ctx->data, bits, ctx->bit_pos, (ctx->data & ((1UL << nbit) - 1)));
        uint64_t mask = (nbit == 64) ? ~0UL : ((1UL << nbit) - 1);
        value <<= nbit;
        value |= (ctx->data & mask);
        ctx->buf_pos++;
        if(ctx->buf_pos == ctx->buf_fill){
            if(_bcr_fill(ctx)){
                ctx->buf_pos = 0;
            }else{
                *err = BC_FILE_END;
                return 0;
            }
        }
        ctx->data = ctx->buf[ctx->buf_pos];
        ctx->bit_pos = 0;
        bits -= nbit;
        BC_MSG("E:%lX %lX %ld %d\n", value, ctx->data, bits, ctx->bit_pos);
    }
    if(bits){
        value <<= bits;
        value |= (ctx->data >> (BC_BLEN - bits - ctx->bit_pos)) & ((1UL << bits) - 1);
        ctx->bit_pos += bits;
    }
    return value;
}

uint64_t bcr_getbits(bc_context *ctx, uint64_t bits, uint64_t *err)
{
    uint64_t value = 0;
    *err = BC_OK;

    long offset = ftell(ctx->fp);
    int buf_pos = ctx->buf_pos;
    int bit_pos = ctx->bit_pos;
    BC_UNIT data = ctx->data;

    while( bits + bit_pos >= BC_BLEN){
        int nbit = (BC_BLEN - bit_pos);
        uint64_t mask = (nbit == 64) ? ~0UL : ((1UL << nbit) - 1);
        value <<= nbit;
        value |= (data & mask);
        buf_pos++;
        if(buf_pos >= ctx->buf_fill){
            BC_UNIT tmp;
            if(fread(&tmp, sizeof(BC_UNIT), 1, ctx->fp) == 0){
                *err = BC_FILE_END;
                return 0;
            }
            data = tmp;
        }else{
            data = ctx->buf[buf_pos];
        }
        bits -= (BC_BLEN - bit_pos);
        bit_pos = 0;
    }
    if(bits){
        value <<= bits;
        value |= (data >> (BC_BLEN - bits - bit_pos)) & ((1UL << bits) - 1);
    }

    if(offset != ftell(ctx->fp))
        fseek(ctx->fp, offset - ftell(ctx->fp), SEEK_CUR);
    return value;
}

void bcr_skipbits(bc_context *ctx, uint64_t bits, uint64_t *err)
{
    *err = BC_OK;
    while( bits + ctx->bit_pos >= BC_BLEN){
        ctx->buf_pos++;
        if(ctx->buf_pos == ctx->buf_fill){
            if(_bcr_fill(ctx)){
                ctx->buf_pos = 0;
            }else{
                *err = BC_FILE_END;
            }
        }
        bits -= (BC_BLEN - ctx->bit_pos);
        ctx->bit_pos = 0;
    }
    ctx->data = ctx->buf[ctx->buf_pos];
    ctx->bit_pos += bits;
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
        bc_context* ctx = bcw_open("test.bin");
        for(int i = 0; i < num_items; i++){
            bits[i] = (rand()%64) + 1;
            uint64_t mask = (bits[i] == 64) ? ~0UL : ((1UL << bits[i]) - 1);
            values[i] = ((uint64_t)rand()*rand()) & mask;
            bcw_write(ctx, bits[i], values[i]);
        }
        bcw_align(ctx);
        bcw_close(ctx);

        ctx = bcr_open("test.bin");
        for(int i = 0; i < num_items; i++){
            uint64_t err;

        #if 1
            int64_t value = bcr_readbits(ctx, bits[i], &err);
        #else
            int64_t value = bcr_getbits(ctx, bits[i], &err);
            bcr_skipbits(ctx, bits[i], &err);
        #endif

            if(err != BC_OK){
                printf("abnormal file end\n");
            }

            if(value != values[i])
                printf("%d: bits:%lu %lX %lX\n", i, bits[i], value, values[i]);
        }
        bcr_close(ctx);
    }

    free(bits);
    free(values);
    return 0;
}
#endif
