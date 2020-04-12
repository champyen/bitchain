# bitchain
library for read / write bitstream data

# compilation
default BC_UNIT is uint64_t,
if you want to use different unit (uint8_t for example)
add '-DBC_UNIT=uint8_t' in your compilation clags

## Bitstream writing steps
1. create context by 'bcw_open'
```
    bc_context *ctx = bcw_open("test.bin");
```
2. write 1~63 bits you want to write by 'bcw_write'
```
    bcw_write(ctx, bits, value);
```
3. if you want to have alignment write, next writing will be aligned to BC_UNIT
```
    bcw_align(ctx);
```
4. close context by 'bcw_close'
```
    bcw_close(ctx);
```
## Bitstream reading steps
1. create context by 'bcr_open'
```
    bc_context *ctx = bcr_open("test.bin");
```
2. read 1~63 bits you want to read by 'bcr_read', return value will be negative for abnormal case
```
    int64_t value = bcr_readbits(ctx, bits);
```
3. get value without change current context steps by 'bcr_getbits', this is useful for VLC decoding
```
    int64_t value = bcr_readbits(ctx, bits);
```
4. skip bits without get the value, moving the context pointer forward by 'bcr_skipbits'
```
    bcr_skipbits(ctx, bits);
```
5. if you want to have alignment write, next reading will be aligned to BC_UNIT
```
    bcr_align(ctx);
```
6. close context by 'bcr_close'
```
    bcr_close(ctx);
```
