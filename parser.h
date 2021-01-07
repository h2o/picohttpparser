/*
 * Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase,
 *                         Shigeo Mitsunari
 *
 * The software is licensed under either the MIT License (below) or the Perl
 * license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef picohttpparser_h
#define picohttpparser_h

#include <memory.h>
#include <inttypes.h>

#ifdef _MSC_VER
#define ssize_t intptr_t
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Copied from https://github.com/lpereira/lwan
/*
 * lwan - simple web server
 * Copyright (c) 2012 Leandro A. F. Pereira <leandro@hardinfo.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define STR4_INT(a, b, c, d) ((uint32_t)((a) | (b) << 8 | (c) << 16 | (d) << 24))
#define STR2_INT(a, b) ((uint16_t)((a) | (b) << 8))
#define STR8_INT(a, b, c, d, e, f, g, h)                                       \
    ((uint64_t)STR4_INT(a, b, c, d) | (uint64_t)STR4_INT(e, f, g, h) << 32)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define STR4_INT(d, c, b, a) ((uint32_t)((a) | (b) << 8 | (c) << 16 | (d) << 24))
#define STR2_INT(b, a) ((uint16_t)((a) | (b) << 8))
#define STR8_INT(a, b, c, d, e, f, g, h)                                       \
    ((uint64_t)STR4_INT(a, b, c, d) << 32 | (uint64_t)STR4_INT(e, f, g, h))
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#error A PDP? Seriously?
#endif

static inline uint16_t string_as_uint16(const char *s)
{
    uint16_t u;

    memcpy(&u, s, sizeof(u));

    return u;
}

static inline uint32_t string_as_uint32(const char *s)
{
    uint32_t u;
    memcpy(&u, s, sizeof(u));
    return u;
}

static inline uint64_t string_as_uint64(const char *s)
{
    uint64_t u;
    memcpy(&u, s, sizeof(u));
    return u;
}

#define LOWER2(s) ((s) | (uint16_t)0x2020)
#define LOWER4(s) ((s) | (uint32_t)0x20202020)
#define LOWER8(s) ((s) | (uint64_t)0x2020202020202020)

#define STR2_INT_L(a, b) LOWER2(STR2_INT(a, b))
#define STR4_INT_L(a, b, c, d) LOWER4(STR4_INT(a, b, c, d))
#define STR8_INT_L(a, b, c, d, e, f, g, h) LOWER8(STR8_INT(a, b, c, d, e, f, g, h))

#pragma endregion

typedef enum {
    M_DELETE = 0,
    M_GET,
    M_HEAD,
    M_POST,
    M_PUT,
    M_CONNECT,
    M_OPTIONS,
    M_TRACE,
    M_UNKNOWN
} http_method_t;

/* contains name and value of a header (name == NULL if is a continuing line
 * of a multiline header */
struct phr_header {
    const char *name;
    size_t name_len;
    const char *value;
    size_t value_len;
};

typedef struct phr_request_cb {
    int (*on_path)(void*, const char *, size_t);
    int (*on_header)(void*, const char *, size_t, const char *, size_t);
    int (*on_body_part)(void*, const char *, size_t);
} phr_request_cb_t;

typedef struct phr_request {
    int       method;
    long      content_length;
    int8_t    major_version;
    int8_t    minor_version;
    union {
        uint8_t flags;
        struct {
            uint8_t is_chunked: 1;
            uint8_t is_upgrade: 1;
            uint8_t is_keep_alive: 1;
            uint8_t is_close: 1;
            uint8_t is_done: 1;
            uint8_t _u3: 3;
        };
    };
    void* cbp;
} phr_request_t;

/* returns number of bytes consumed if successful, -2 if request is partial,
 * -1 if failed */
int phr_parse_request(
    const char *buf,
    size_t len,
    phr_request_t* req,
    const phr_request_cb_t* cb,
    size_t last_len);

typedef struct phr_response_cb {
    int (*on_status)(void*,    const char *, size_t);
    int (*on_header)(void *,   const char *, size_t, const char*, size_t);
    int (*on_body_part)(void*, const char *, size_t);
} phr_response_cb_t;

typedef struct phr_response {
    int       status;
    size_t    content_length;
    int8_t    major_version;
    int8_t    minor_version;
    union {
        uint8_t flags;
        struct {
            uint8_t is_chunked: 1;
            uint8_t is_keep_alive: 1;
            uint8_t is_close: 1;
            uint8_t is_done: 1;
            uint8_t is_upgrade: 1;
            uint8_t _u3: 3;
        };
    };
    void* cbp;
} phr_response_t;

/* ditto */
int phr_parse_response(
    const char *buf,
    size_t len,
    phr_response_t* resp,
    const phr_response_cb_t* cb,
    size_t last_len);

/* ditto */
int phr_parse_headers(
    const char *buf,
    size_t len,
    void* data,
    int(*on_header)(void*, const char*, size_t, const char*, size_t),
    size_t last_len);

/* should be zero-filled before start */
struct phr_chunked_decoder {
    size_t bytes_left_in_chunk; /* number of bytes left in current chunk */
    char consume_trailer;       /* if trailing headers should be consumed */
    char _hex_count;
    char _state;
};

/* the function rewrites the buffer given as (buf, bufsz) removing the chunked-
 * encoding headers.  When the function returns without an error, bufsz is
 * updated to the length of the decoded data available.  Applications should
 * repeatedly call the function while it returns -2 (incomplete) every time
 * supplying newly arrived data.  If the end of the chunked-encoded data is
 * found, the function returns a non-negative number indicating the number of
 * octets left undecoded at the tail of the supplied buffer.  Returns -1 on
 * error.
 */
ssize_t phr_decode_chunked(struct phr_chunked_decoder *decoder, char *buf, size_t *bufsz);

/* returns if the chunked decoder is in middle of chunked data */
int phr_decode_chunked_is_in_data(struct phr_chunked_decoder *decoder);

#ifdef __cplusplus
}
#endif

#endif
