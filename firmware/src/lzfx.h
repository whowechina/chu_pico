/*
 * Lzfx decompressor
 * WHowe <github.com/whowechina>
 * This is actually taken from CrazyRedMachine's repo
 * <https://github.com/CrazyRedMachine/RedBoard/blob/main/io_dll/src/utils/hid_impl.c>
 */

#define LZFX_ESIZE      -1      /* Output buffer too small */
#define LZFX_ECORRUPT   -2      /* Invalid data for decompression */
#define LZFX_EARGS      -3      /* Arguments invalid (NULL) */
#define LZFX_HLOG 16
#define LZFX_HSIZE (1 << (LZFX_HLOG))

#define fx_expect_false(expr)  (expr)
#define fx_expect_true(expr)   (expr)

int lzfx_decompress(const void* ibuf, unsigned int ilen,
                          void* obuf, unsigned int *olen);