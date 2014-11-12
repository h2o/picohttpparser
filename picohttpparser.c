/*
 * Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase
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

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "picohttpparser.h"

/* $Id$ */

#if __GNUC__ >= 3
# define likely(x) __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#define IS_PRINTABLE_ASCII(c) ((unsigned char)(c) - 040u < 0137u)

#define CHECK_EOF() \
  if (buf == buf_end) { \
    *ret = -2; \
    return NULL; \
  }

#define EXPECT_CHAR(ch) \
  CHECK_EOF(); \
  if (*buf++ != ch) { \
    *ret = -1; \
    return NULL; \
  }

#define ADVANCE_TOKEN(tok, toklen) do { \
    const char* tok_start = buf; \
    for (; ; ++buf) { \
      CHECK_EOF(); \
      if (*buf == ' ') { \
        break; \
      } else if (unlikely(! IS_PRINTABLE_ASCII(*buf))) { \
        if ((unsigned char)*buf < '\040' || *buf == '\177') { \
          *ret = -1; \
          return NULL; \
        } \
      } \
    } \
    tok = tok_start; \
    toklen = buf - tok_start; \
  } while (0)

static const char* token_char_map =
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\1\1\1\1\1\1\1\0\0\1\1\0\1\1\0\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0"
  "\0\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\1\1"
  "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\1\0\1\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

static const char* get_token_to_eol(const char* buf, const char* buf_end,
                                    const char** token, size_t* token_len,
                                    int* ret)
{
  const char* token_start = buf;
  
  /* find non-printable char within the next 8 bytes, this is the hottest code; manually inlined */
  while (likely(buf_end - buf >= 8)) {
#define DOIT() if (unlikely(! IS_PRINTABLE_ASCII(*buf))) goto NonPrintable; ++buf
    DOIT(); DOIT(); DOIT(); DOIT();
    DOIT(); DOIT(); DOIT(); DOIT();
#undef DOIT
    continue;
  NonPrintable:
    if ((likely((unsigned char)*buf < '\040') && likely(*buf != '\011')) || unlikely(*buf == '\177')) {
      goto FOUND_CTL;
    }
  }
  for (; ; ++buf) {
    CHECK_EOF();
    if (unlikely(! IS_PRINTABLE_ASCII(*buf))) {
      if ((likely((unsigned char)*buf < '\040') && likely(*buf != '\011')) || unlikely(*buf == '\177')) {
        goto FOUND_CTL;
      }
    }
  }
 FOUND_CTL:
  if (likely(*buf == '\015')) {
    ++buf;
    EXPECT_CHAR('\012');
    *token_len = buf - 2 - token_start;
  } else if (*buf == '\012') {
    *token_len = buf - token_start;
    ++buf;
  } else {
    *ret = -1;
    return NULL;
  }
  *token = token_start;
  
  return buf;
}
  
static const char* is_complete(const char* buf, const char* buf_end,
                               size_t last_len, int* ret)
{
  int ret_cnt = 0;
  buf = last_len < 3 ? buf : buf + last_len - 3;
  
  while (1) {
    CHECK_EOF();
    if (*buf == '\015') {
      ++buf;
      CHECK_EOF();
      EXPECT_CHAR('\012');
      ++ret_cnt;
    } else if (*buf == '\012') {
      ++buf;
      ++ret_cnt;
    } else {
      ++buf;
      ret_cnt = 0;
    }
    if (ret_cnt == 2) {
      return buf;
    }
  }
  
  *ret = -2;
  return NULL;
}

/* *_buf is always within [buf, buf_end) upon success */
static const char* parse_int(const char* buf, const char* buf_end, int* value,
                             int* ret)
{
  int v;
  CHECK_EOF();
  if (! ('0' <= *buf && *buf <= '9')) {
    *ret = -1;
    return NULL;
  }
  v = 0;
  for (; ; ++buf) {
    CHECK_EOF();
    if ('0' <= *buf && *buf <= '9') {
      v = v * 10 + *buf - '0';
    } else {
      break;
    }
  }
  
  *value = v;
  return buf;
}

/* returned pointer is always within [buf, buf_end), or null */
static const char* parse_http_version(const char* buf, const char* buf_end,
                                      int* minor_version, int* ret)
{
  EXPECT_CHAR('H'); EXPECT_CHAR('T'); EXPECT_CHAR('T'); EXPECT_CHAR('P');
  EXPECT_CHAR('/'); EXPECT_CHAR('1'); EXPECT_CHAR('.');
  return parse_int(buf, buf_end, minor_version, ret);
}

static const char* parse_headers(const char* buf, const char* buf_end,
                                 struct phr_header* headers,
                                 size_t* num_headers, size_t max_headers,
                                 int* ret)
{
  for (; ; ++*num_headers) {
    CHECK_EOF();
    if (*buf == '\015') {
      ++buf;
      EXPECT_CHAR('\012');
      break;
    } else if (*buf == '\012') {
      ++buf;
      break;
    }
    if (*num_headers == max_headers) {
      *ret = -1;
      return NULL;
    }
    if (! (*num_headers != 0 && (*buf == ' ' || *buf == '\t'))) {
      if (! token_char_map[(unsigned char)*buf]) {
        *ret = -1;
        return NULL;
      }
      /* parsing name, but do not discard SP before colon, see
       * http://www.mozilla.org/security/announce/2006/mfsa2006-33.html */
      headers[*num_headers].name = buf;
      for (; ; ++buf) {
        CHECK_EOF();
        if (*buf == ':') {
          break;
        } else if (*buf < ' ') {
          *ret = -1;
          return NULL;
        }
      }
      headers[*num_headers].name_len = buf - headers[*num_headers].name;
      ++buf;
      for (; ; ++buf) {
        CHECK_EOF();
        if (! (*buf == ' ' || *buf == '\t')) {
          break;
        }
      }
    } else {
      headers[*num_headers].name = NULL;
      headers[*num_headers].name_len = 0;
    }
    if ((buf = get_token_to_eol(buf, buf_end, &headers[*num_headers].value,
                                &headers[*num_headers].value_len, ret))
        == NULL) {
      return NULL;
    }
  }
  return buf;
}

const char* parse_request(const char* buf, const char* buf_end,
                          const char** method, size_t* method_len,
                          const char** path, size_t* path_len,
                          int* minor_version, struct phr_header* headers,
                          size_t* num_headers, size_t max_headers, int* ret)
{
  /* skip first empty line (some clients add CRLF after POST content) */
  CHECK_EOF();
  if (*buf == '\015') {
    ++buf;
    EXPECT_CHAR('\012');
  } else if (*buf == '\012') {
    ++buf;
  }
  
  /* parse request line */
  ADVANCE_TOKEN(*method, *method_len);
  ++buf;
  ADVANCE_TOKEN(*path, *path_len);
  ++buf;
  if ((buf = parse_http_version(buf, buf_end, minor_version, ret)) == NULL) {
    return NULL;
  }
  if (*buf == '\015') {
    ++buf;
    EXPECT_CHAR('\012');
  } else if (*buf == '\012') {
    ++buf;
  } else {
    *ret = -1;
    return NULL;
  }
  
  return parse_headers(buf, buf_end, headers, num_headers, max_headers, ret);
}

int phr_parse_request(const char* buf_start, size_t len, const char** method,
                      size_t* method_len, const char** path, size_t* path_len,
                      int* minor_version, struct phr_header* headers,
                      size_t* num_headers, size_t last_len)
{
  const char * buf = buf_start, * buf_end = buf_start + len;
  size_t max_headers = *num_headers;
  int r;
  
  *method = NULL;
  *method_len = 0;
  *path = NULL;
  *path_len = 0;
  *minor_version = -1;
  *num_headers = 0;
  
  /* if last_len != 0, check if the request is complete (a fast countermeasure
     againt slowloris */
  if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
    return r;
  }
  
  if ((buf = parse_request(buf, buf_end, method, method_len, path, path_len,
                           minor_version, headers, num_headers, max_headers,
                           &r))
      == NULL) {
    return r;
  }
  
  return (int)(buf - buf_start);
}

static const char* parse_response(const char* buf, const char* buf_end,
                                  int* minor_version, int* status,
                                  const char** msg, size_t* msg_len,
                                  struct phr_header* headers,
                                  size_t* num_headers, size_t max_headers,
                                  int* ret)
{
  /* parse "HTTP/1.x" */
  if ((buf = parse_http_version(buf, buf_end, minor_version, ret)) == NULL) {
    return NULL;
  }
  /* skip space */
  if (*buf++ != ' ') {
    *ret = -1;
    return NULL;
  }
  /* parse status code */
  if ((buf = parse_int(buf, buf_end, status, ret)) == NULL) {
    return NULL;
  }
  /* skip space */
  if (*buf++ != ' ') {
    *ret = -1;
    return NULL;
  }
  /* get message */
  if ((buf = get_token_to_eol(buf, buf_end, msg, msg_len, ret)) == NULL) {
    return NULL;
  }
  
  return parse_headers(buf, buf_end, headers, num_headers, max_headers, ret);
}

int phr_parse_response(const char* buf_start, size_t len, int* minor_version,
                       int* status, const char** msg, size_t* msg_len,
                       struct phr_header* headers, size_t* num_headers,
                       size_t last_len)
{
  const char * buf = buf_start, * buf_end = buf + len;
  size_t max_headers = *num_headers;
  int r;
  
  *minor_version = -1;
  *status = 0;
  *msg = NULL;
  *msg_len = 0;
  *num_headers = 0;
  
  /* if last_len != 0, check if the response is complete (a fast countermeasure
     against slowloris */
  if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
    return r;
  }
  
  if ((buf = parse_response(buf, buf_end, minor_version, status, msg, msg_len,
                            headers, num_headers, max_headers, &r))
      == NULL) {
    return r;
  }
  
  return (int)(buf - buf_start);
}

int phr_parse_headers(const char* buf_start, size_t len,
                      struct phr_header* headers, size_t* num_headers,
                      size_t last_len)
{
  const char* buf = buf_start, * buf_end = buf + len;
  size_t max_headers = *num_headers;
  int r;

  *num_headers = 0;

  /* if last_len != 0, check if the response is complete (a fast countermeasure
     against slowloris */
  if (last_len != 0 && is_complete(buf, buf_end, last_len, &r) == NULL) {
    return r;
  }

  if ((buf = parse_headers(buf, buf_end, headers, num_headers, max_headers, &r))
      == NULL) {
    return r;
  }

  return (int)(buf - buf_start);
}

enum {
  CHUNKED_IN_CHUNK_SIZE,
  CHUNKED_IN_CHUNK_EXT,
  CHUNKED_IN_CHUNK_DATA
};

static ssize_t decode_hexnum(const char *bytes, size_t len, size_t *value)
{
  size_t v = 0, i;
  int ch;

  for (i = 0; i != len; ++i) {
    if (i >= sizeof(size_t) / 4)
      return -1;
    ch = bytes[i];
    if ('0' <= ch && ch <= '9') {
      v = v * 16 + ch - '0';
    } else if ('A' <= ch && ch <= 'F') {
      v = v * 16 + ch - 'A' + 0xa;
    } else if ('a' <= ch && ch <= 'f') {
      v = v * 16 + ch - 'a' + 0xa;
    } else {
      if (i == 0)
        return -1;
      *value = v;
      return i;
    }
  }
  return -2;
}

ssize_t phr_decode_chunked(struct phr_chunked_decoder *decoder, char *buf,
                           size_t *bufsz)
{
  size_t data_offset = decoder->pos;
  ssize_t ret = -2; /* incomplete */

  while (1) {
    switch (decoder->state) {
    case CHUNKED_IN_CHUNK_SIZE:
      if ((ret = decode_hexnum(buf + decoder->pos, *bufsz - decoder->pos,
                               &decoder->chunk_data_size))
          < 0)
        goto Exit;
      decoder->pos += ret;
      ret = -2; /* reset to incomplete */
      decoder->state = CHUNKED_IN_CHUNK_EXT;
      /* fallthru */
    case CHUNKED_IN_CHUNK_EXT:
      /* RFC 7230 A.2 "Line folding in chunk extensions is disallowed" */
      for (; ; ++decoder->pos) {
        if (*bufsz - decoder->pos < 2)
            goto Exit;
        if (memcmp(buf + decoder->pos, "\r\n", 2) == 0)
            break;
      }
      decoder->pos += 2;
      if (decoder->chunk_data_size == 0) {
        ret = data_offset;
        goto Exit;
      }
      decoder->state = CHUNKED_IN_CHUNK_DATA;
      /* fallthru */
    case CHUNKED_IN_CHUNK_DATA:
      if (*bufsz - decoder->pos < decoder->chunk_data_size)
        goto Exit;
      memmove(buf + data_offset, buf + decoder->pos, decoder->chunk_data_size);
      decoder->pos += decoder->chunk_data_size;
      data_offset += decoder->chunk_data_size;
      decoder->state = CHUNKED_IN_CHUNK_SIZE;
      break;
    default:
      assert(!"decoder is corrupt");
    }
  }

  Exit:
  if (data_offset != decoder->pos) {
    memmove(buf + data_offset, buf + decoder->pos, *bufsz - decoder->pos);
    *bufsz -= decoder->pos - data_offset;
    decoder->pos = data_offset;
  }
  return ret;
}

#undef CHECK_EOF
#undef EXPECT_CHAR
#undef ADVANCE_TOKEN
