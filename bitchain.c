#include "bitchain.h"

bc_context *bcw_open(char *fn)
{
    bc_context *ctx = (bc_context*)calloc(1, sizeof(bc_context));
    ctx->fp = fopen(fn, "wb");
    ctx->buf = (BC_UNIT*)calloc(1, BC_BUF_NBYTES);
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
        //printf("%s: %d\n", __func__, ctx->end);
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

int64_t bcr_align(bc_context *ctx)
{
    ctx->buf_pos++;
    if(ctx->buf_pos == ctx->buf_fill){
        if(_bcr_fill(ctx))
            ctx->buf_pos = 0;
        else
            return -1;
    }
    ctx->data = ctx->buf[ctx->buf_pos];
    ctx->bit_pos = 0;
    return 0;
}

int64_t bcr_readbits(bc_context *ctx, uint64_t bits)
{
    int64_t value = 0;

    while( (bits + ctx->bit_pos) >= BC_BLEN){
        int nbit = (BC_BLEN - ctx->bit_pos);
        value <<= nbit;
        value |= (ctx->data & ((1 << nbit) - 1));
        ctx->buf_pos++;
        if(ctx->buf_pos == ctx->buf_fill){
            if(_bcr_fill(ctx))
                ctx->buf_pos = 0;
            else
                return -1;
        }
        ctx->data = ctx->buf[ctx->buf_pos];
        bits -= nbit;
        //printf("%d %d %d\n", bits, ctx->bit_pos, BC_BLEN);
        ctx->bit_pos = 0;
    }
    if(bits){
        value <<= bits;
        value |= (ctx->data >> (BC_BLEN - bits - ctx->bit_pos)) & ((1 << bits) - 1);
        ctx->bit_pos += bits;
    }
    return value;
}

int64_t bcr_getbits(bc_context *ctx, uint64_t bits)
{
    int64_t value = 0;
    long offset = ftell(ctx->fp);
    int buf_pos = ctx->buf_pos;
    int bit_pos = ctx->bit_pos;
    BC_UNIT data = ctx->data;

    while( bits + bit_pos >= BC_BLEN){
        int bits = (BC_BLEN - bit_pos);
        value <<= bits;
        value |= (data & ((1 << bits) - 1));
        buf_pos++;
        if(buf_pos >= ctx->buf_fill){
            BC_UNIT tmp;
            if(fread(&tmp, sizeof(BC_UNIT), 1, ctx->fp) == 0)
                return -1;
            data = tmp;
        }else{
            data = ctx->buf[buf_pos];
        }
        bits -= (BC_BLEN - bit_pos);
        bit_pos = 0;
    }
    value <<= bits;
    value |= (ctx->data >> (BC_BLEN - bits - bit_pos)) & ((1 << bits) - 1);

    if(offset != ftell(ctx->fp))
        fseek(ctx->fp, offset - ftell(ctx->fp), SEEK_CUR);
}

int64_t bcr_skipbits(bc_context *ctx, uint64_t bits)
{
    while( bits + ctx->bit_pos >= BC_BLEN){
        ctx->buf_pos++;
        if(ctx->buf_pos == ctx->buf_fill){
            if(_bcr_fill(ctx))
                ctx->buf_pos = 0;
            else
                return -1;
        }
        bits -= (BC_BLEN - ctx->bit_pos);
        ctx->bit_pos = 0;
    }
    ctx->data = ctx->buf[ctx->buf_pos];
    ctx->bit_pos += bits;
}

#ifdef BC_TEST
#include <time.h>

#define NUM_TESTS 10
#define NUM_ITEMS 81920

int main(int ac, char **av)
{
    uint64_t bits[NUM_ITEMS] = {0};
    uint64_t values[NUM_ITEMS] = {0};
    srand(time(NULL));

    for(int j = 0; j < NUM_TESTS; j++){
        bc_context* ctx = bcw_open("test.bin");
        for(int i = 0; i < NUM_ITEMS; i++){
            bits[i] = ((rand()%30)+1)*2;
            values[i] = rand() & ((1 << bits[i]) - 1);
            bcw_write(ctx, bits[i], values[i]);
        }
        bcw_align(ctx);
        bcw_close(ctx);

        ctx = bcr_open("test.bin");
        for(int i = 0; i < NUM_ITEMS; i++){
            int64_t value = bcr_readbits(ctx, bits[i]);
            if(value != values[i])
                printf("%d: bits:%lu %X %X\n", i, bits[i], value, values[i]);
        }
        bcr_close(ctx);
    }
    return 0;
}
#endif
