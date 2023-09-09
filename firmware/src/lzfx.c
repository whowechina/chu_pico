/*
 * Lzfx decompressor
 * WHowe <github.com/whowechina>
 * This is actually taken from CrazyRedMachine's repo
 * <https://github.com/CrazyRedMachine/RedBoard/blob/main/io_dll/src/utils/hid_impl.c>
 */


#include <stdlib.h>
#include "lzfx.h"

typedef unsigned char u8;
typedef const u8 *LZSTATE[LZFX_HSIZE];


/* Guess len. No parameters may be NULL; this is not checked. */
static int lzfx_getsize(const void *ibuf, unsigned int ilen, unsigned int *olen)
{

    u8 const *ip = (const u8 *)ibuf;
    u8 const *const in_end = ip + ilen;
    int tot_len = 0;

    while (ip < in_end)
    {

        unsigned int ctrl = *ip++;

        if (ctrl < (1 << 5))
        {

            ctrl++;

            if (ip + ctrl > in_end)
                return LZFX_ECORRUPT;

            tot_len += ctrl;
            ip += ctrl;
        }
        else
        {

            unsigned int len = (ctrl >> 5);

            if (len == 7)
            { /* i.e. format #2 */
                len += *ip++;
            }

            len += 2; /* len is now #octets */

            if (ip >= in_end)
                return LZFX_ECORRUPT;

            ip++; /* skip the ref byte */

            tot_len += len;
        }
    }

    *olen = tot_len;

    return 0;
}

/* Decompressor */
int lzfx_decompress(const void *ibuf, unsigned int ilen,
                    void *obuf, unsigned int *olen)
{

    u8 const *ip = (const u8 *)ibuf;
    u8 const *const in_end = ip + ilen;
    u8 *op = (u8 *)obuf;
    u8 const *const out_end = (olen == NULL ? NULL : op + *olen);

    unsigned int remain_len = 0;
    int rc;

    if (olen == NULL)
        return LZFX_EARGS;
    if (ibuf == NULL)
    {
        if (ilen != 0)
            return LZFX_EARGS;
        *olen = 0;
        return 0;
    }
    if (obuf == NULL)
    {
        if (olen != 0)
            return LZFX_EARGS;
        return lzfx_getsize(ibuf, ilen, olen);
    }

    do
    {
        unsigned int ctrl = *ip++;

        /* Format 000LLLLL: a literal byte string follows, of length L+1 */
        if (ctrl < (1 << 5))
        {

            ctrl++;

            if (fx_expect_false(op + ctrl > out_end))
            {
                --ip; /* Rewind to control byte */
                goto guess;
            }
            if (fx_expect_false(ip + ctrl > in_end))
                return LZFX_ECORRUPT;

            do
                *op++ = *ip++;
            while (--ctrl);

            /*  Format #1 [LLLooooo oooooooo]: backref of length L+1+2
                              ^^^^^ ^^^^^^^^
                                A      B
                       #2 [111ooooo LLLLLLLL oooooooo] backref of length L+7+2
                              ^^^^^          ^^^^^^^^
                                A               B
                In both cases the location of the backref is computed from the
                remaining part of the data as follows:

                    location = op - A*256 - B - 1
            */
        }
        else
        {

            unsigned int len = (ctrl >> 5);
            u8 *ref = op - ((ctrl & 0x1f) << 8) - 1;

            if (len == 7)
                len += *ip++; /* i.e. format #2 */

            len += 2; /* len is now #octets */

            if (fx_expect_false(op + len > out_end))
            {
                ip -= (len >= 9) ? 2 : 1; /* Rewind to control byte */
                goto guess;
            }
            if (fx_expect_false(ip >= in_end))
                return LZFX_ECORRUPT;

            ref -= *ip++;

            if (fx_expect_false(ref < (u8 *)obuf))
                return LZFX_ECORRUPT;

            do
                *op++ = *ref++;
            while (--len);
        }

    } while (ip < in_end);

    *olen = op - (u8 *)obuf;

    return 0;

guess:
    rc = lzfx_getsize(ip, ilen - (ip - (u8 *)ibuf), &remain_len);
    if (rc >= 0)
        *olen = remain_len + (op - (u8 *)obuf);
    return rc;
}
