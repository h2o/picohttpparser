/* use `make test` to run the test */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "picotest/picotest.h"
#include "picohttpparser.h"

static int bufis(const char* s, size_t l, const char* t)
{
  return strlen(t) == l && memcmp(s, t, l) == 0;
}

static void test_request(void)
{
  const char* method;
  size_t method_len;
  const char* path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[4];
  size_t num_headers;
  
#define PARSE(s, last_len, exp, comment)                                \
  do {                                                                  \
    note(comment);                                                      \
    num_headers = sizeof(headers) / sizeof(headers[0]);                 \
    ok(phr_parse_request(s, sizeof(s) - 1, &method, &method_len, &path, \
                         &path_len, &minor_version, headers,            \
                         &num_headers, last_len)                        \
       == (exp == 0 ? strlen(s) : exp));                                \
  } while (0)
  
  PARSE("GET / HTTP/1.0\r\n\r\n", 0, 0, "simple");
  ok(num_headers == 0);
  ok(bufis(method, method_len, "GET"));
  ok(bufis(path, path_len, "/"));
  ok(minor_version == 0);
  
  PARSE("GET / HTTP/1.0\r\n\r", 0, -2, "partial");
  
  PARSE("GET /hoge HTTP/1.1\r\nHost: example.com\r\nCookie: \r\n\r\n", 0, 0,
        "parse headers");
  ok(num_headers == 2);
  ok(bufis(method, method_len, "GET"));
  ok(bufis(path, path_len, "/hoge"));
  ok(minor_version == 1);
  ok(bufis(headers[0].name, headers[0].name_len, "Host"));
  ok(bufis(headers[0].value, headers[0].value_len, "example.com"));
  ok(bufis(headers[1].name, headers[1].name_len, "Cookie"));
  ok(bufis(headers[1].value, headers[1].value_len, ""));
  
  PARSE("GET / HTTP/1.0\r\nfoo: \r\nfoo: b\r\n  \tc\r\n\r\n", 0, 0,
        "parse multiline");
  ok(num_headers == 3);
  ok(bufis(method, method_len, "GET"));
  ok(bufis(path, path_len, "/"));
  ok(minor_version == 0);
  ok(bufis(headers[0].name, headers[0].name_len, "foo"));
  ok(bufis(headers[0].value, headers[0].value_len, ""));
  ok(bufis(headers[1].name, headers[1].name_len, "foo"));
  ok(bufis(headers[1].value, headers[1].value_len, "b"));
  ok(headers[2].name == NULL);
  ok(bufis(headers[2].value, headers[2].value_len, "  \tc"));
  
  PARSE("GET", 0, -2, "incomplete 1");
  ok(method == NULL);
  PARSE("GET ", 0, -2, "incomplete 2");
  ok(bufis(method, method_len, "GET"));
  PARSE("GET /", 0, -2, "incomplete 3");
  ok(path == NULL);
  PARSE("GET / ", 0, -2, "incomplete 4");
  ok(bufis(path, path_len, "/"));
  PARSE("GET / H", 0, -2, "incomplete 5");
  PARSE("GET / HTTP/1.", 0, -2, "incomplete 6");
  PARSE("GET / HTTP/1.0", 0, -2, "incomplete 7");
  ok(minor_version == -1);
  PARSE("GET / HTTP/1.0\r", 0, -2, "incomplete 8");
  ok(minor_version == 0);
  
  PARSE("GET /hoge HTTP/1.0\r\n\r", strlen("GET /hoge HTTP/1.0\r\n\r") - 1,
        -2, "slowloris (incomplete)");
  PARSE("GET /hoge HTTP/1.0\r\n\r\n", strlen("GET /hoge HTTP/1.0\r\n\r\n") - 1,
        0, "slowloris (complete)");
  
  PARSE("GET / HTTP/1.0\r\n:a\r\n\r\n", 0, -1, "empty header name");
  PARSE("GET / HTTP/1.0\r\n :a\r\n\r\n", 0, -1, "header name (space only)");

  PARSE("G\0T / HTTP/1.0\r\n\r\n", 0, -1, "NUL in method");
  PARSE("G\tT / HTTP/1.0\r\n\r\n", 0, -1, "tab in method");
  PARSE("GET /\x7fhello HTTP/1.0\r\n\r\n", 0, -1, "DEL in uri-path");
  PARSE("GET / HTTP/1.0\r\na\0b: c\r\n\r\n", 0, -1, "NUL in header name");
  PARSE("GET / HTTP/1.0\r\nab: c\0d\r\n\r\n", 0, -1, "NUL in header value");
  PARSE("GET /\xa0 HTTP/1.0\r\nh: c\xa2y\r\n\r\n", 0, 0, "accept MSB chars");
  ok(num_headers == 1);
  ok(bufis(method, method_len, "GET"));
  ok(bufis(path, path_len, "/\xa0"));
  ok(minor_version == 0);
  ok(bufis(headers[0].name, headers[0].name_len, "h"));
  ok(bufis(headers[0].value, headers[0].value_len, "c\xa2y"));

#undef PARSE
}

static void test_response(void)
{
  int minor_version;
  int status;
  const char *msg;
  size_t msg_len;
  struct phr_header headers[4];
  size_t num_headers;
  
#define PARSE(s, last_len, exp, comment)                         \
  do {                                                           \
    note(comment);                                               \
    num_headers = sizeof(headers) / sizeof(headers[0]);          \
    ok(phr_parse_response(s, strlen(s), &minor_version, &status, \
                         &msg, &msg_len, headers,                \
                         &num_headers, last_len)                 \
       == (exp == 0 ? strlen(s) : exp));                         \
  } while (0)
  
  PARSE("HTTP/1.0 200 OK\r\n\r\n", 0, 0, "simple");
  ok(num_headers == 0);
  ok(status      == 200);
  ok(minor_version = 1);
  ok(bufis(msg, msg_len, "OK"));
  
  PARSE("HTTP/1.0 200 OK\r\n\r", 0, -2, "partial");

  PARSE("HTTP/1.1 200 OK\r\nHost: example.com\r\nCookie: \r\n\r\n", 0, 0,
        "parse headers");
  ok(num_headers == 2);
  ok(minor_version == 1);
  ok(status == 200);
  ok(bufis(msg, msg_len, "OK"));
  ok(bufis(headers[0].name, headers[0].name_len, "Host"));
  ok(bufis(headers[0].value, headers[0].value_len, "example.com"));
  ok(bufis(headers[1].name, headers[1].name_len, "Cookie"));
  ok(bufis(headers[1].value, headers[1].value_len, ""));

  PARSE("HTTP/1.0 200 OK\r\nfoo: \r\nfoo: b\r\n  \tc\r\n\r\n", 0, 0,
        "parse multiline");
  ok(num_headers == 3);
  ok(minor_version == 0);
  ok(status == 200);
  ok(bufis(msg, msg_len, "OK"));
  ok(bufis(headers[0].name, headers[0].name_len, "foo"));
  ok(bufis(headers[0].value, headers[0].value_len, ""));
  ok(bufis(headers[1].name, headers[1].name_len, "foo"));
  ok(bufis(headers[1].value, headers[1].value_len, "b"));
  ok(headers[2].name == NULL);
  ok(bufis(headers[2].value, headers[2].value_len, "  \tc"));

  PARSE("HTTP/1.0 500 Internal Server Error\r\n\r\n", 0, 0,
        "internal server error");
  ok(num_headers == 0);
  ok(minor_version == 0);
  ok(status == 500);
  ok(bufis(msg, msg_len, "Internal Server Error"));
  ok(msg_len == sizeof("Internal Server Error")-1);
  
  PARSE("H", 0, -2, "incomplete 1");
  PARSE("HTTP/1.", 0, -2, "incomplete 2");
  PARSE("HTTP/1.1", 0, -2, "incomplete 3");
  ok(minor_version == -1);
  PARSE("HTTP/1.1 ", 0, -2, "incomplete 4");
  ok(minor_version == 1);
  PARSE("HTTP/1.1 2", 0, -2, "incomplete 5");
  PARSE("HTTP/1.1 200", 0, -2, "incomplete 6");
  ok(status == 0);
  PARSE("HTTP/1.1 200 ", 0, -2, "incomplete 7");
  ok(status == 200);
  PARSE("HTTP/1.1 200 O", 0, -2, "incomplete 8");
  PARSE("HTTP/1.1 200 OK\r", 0, -2, "incomplete 9");
  ok(msg == NULL);
  PARSE("HTTP/1.1 200 OK\r\n", 0, -2, "incomplete 10");
  ok(bufis(msg, msg_len, "OK"));
  PARSE("HTTP/1.1 200 OK\n", 0, -2, "incomplete 11");
  ok(bufis(msg, msg_len, "OK"));

  PARSE("HTTP/1.1 200 OK\r\nA: 1\r", 0, -2, "incomplete 11");
  ok(num_headers == 0);
  PARSE("HTTP/1.1 200 OK\r\nA: 1\r\n", 0, -2, "incomplete 12");
  ok(num_headers == 1);
  ok(bufis(headers[0].name, headers[0].name_len, "A"));
  ok(bufis(headers[0].value, headers[0].value_len, "1"));
  
  PARSE("HTTP/1.0 200 OK\r\n\r", strlen("GET /hoge HTTP/1.0\r\n\r") - 1,
        -2, "slowloris (incomplete)");
  PARSE("HTTP/1.0 200 OK\r\n\r\n", strlen("HTTP/1.0 200 OK\r\n\r\n") - 1,
        0, "slowloris (complete)");
  
  PARSE("HTTP/1. 200 OK\r\n\r\n", 0, -1, "invalid http version");
  PARSE("HTTP/1.2z 200 OK\r\n\r\n", 0, -1, "invalid http version 2");
  PARSE("HTTP/1.1  OK\r\n\r\n", 0, -1, "no status code");
  
#undef PARSE
}

static void test_headers(void)
{
  /* only test the interface; the core parser is tested by the tests above */

  struct phr_header headers[4];
  size_t num_headers;

#define PARSE(s, last_len, exp, comment)                      \
  do {                                                        \
    note(comment);                                            \
    num_headers = sizeof(headers) / sizeof(headers[0]);       \
    ok(phr_parse_headers(s, strlen(s), headers, &num_headers, \
                         last_len)                            \
       == (exp == 0 ? strlen(s) : exp));                      \
  } while (0)

  PARSE("Host: example.com\r\nCookie: \r\n\r\n", 0, 0, "simple");
  ok(num_headers == 2);
  ok(bufis(headers[0].name, headers[0].name_len, "Host"));
  ok(bufis(headers[0].value, headers[0].value_len, "example.com"));
  ok(bufis(headers[1].name, headers[1].name_len, "Cookie"));
  ok(bufis(headers[1].value, headers[1].value_len, ""));

  PARSE("Host: example.com\r\nCookie: \r\n\r\n", 1, 0, "slowloris");
  ok(num_headers == 2);
  ok(bufis(headers[0].name, headers[0].name_len, "Host"));
  ok(bufis(headers[0].value, headers[0].value_len, "example.com"));
  ok(bufis(headers[1].name, headers[1].name_len, "Cookie"));
  ok(bufis(headers[1].value, headers[1].value_len, ""));

  PARSE("Host: example.com\r\nCookie: \r\n\r", 0, -2, "partial");

  PARSE("Host: e\7fample.com\r\nCookie: \r\n\r", 0, -1, "error");

#undef PARSE
}

static void test_chunked_at_once(int line, const char *encoded,
                                 const char *decoded, size_t extra)
{
  struct phr_chunked_decoder dec = {};
  char *buf;
  size_t bufsz;
  ssize_t ret;

  note("testing at-once, source at line %d", line);

  buf = strdup(encoded);
  bufsz = strlen(buf);

  ret = phr_decode_chunked(&dec, buf, &bufsz);

  ok(ret == strlen(decoded));
  ok(bufsz == ret + extra);
  ok(bufis(buf, ret, decoded));
  ok(bufis(buf + ret, extra, encoded + strlen(encoded) - extra));

  free(buf);
}

static void test_chunked_per_byte(int line, const char *encoded,
                                  const char *decoded, size_t extra)
{
  struct phr_chunked_decoder dec = {};
  char *buf = malloc(strlen(encoded));
  size_t bytes_to_consume = strlen(encoded) - extra, bufsz = 0, i;
  ssize_t ret;

  note("testing per-byte, source at line %d", line);

  for (i = 0; i < bytes_to_consume - 1; ++i) {
    buf[bufsz++] = encoded[i];
    ret = phr_decode_chunked(&dec, buf, &bufsz);
    if (ret != -2) {
      ok(0);
      return;
    }
  }
  memcpy(buf + bufsz, encoded + bytes_to_consume - 1, extra + 1);
  bufsz += extra + 1;
  ret = phr_decode_chunked(&dec, buf, &bufsz);
  ok(ret == strlen(decoded));
  ok(bufsz == ret + extra);
  ok(bufis(buf, ret, decoded));
  ok(bufis(buf + ret, extra, encoded + bytes_to_consume));

  free(buf);
}

static void test_chunked_failure(int line, const char *encoded)
{
  struct phr_chunked_decoder dec = {};
  char *buf = strdup(encoded);
  size_t bufsz, i;
  ssize_t ret;

  note("testing failure at-once, source at line %d", line);
  bufsz = strlen(buf);
  ret = phr_decode_chunked(&dec, buf, &bufsz);
  ok(ret == -1);

  note("testing failure per-byte, source at line %d", line);
  memset(&dec, 0, sizeof(dec));
  bufsz = 0;
  for (i = 0; encoded[i] != '\0'; ++i) {
    buf[bufsz++] = encoded[i];
    ret = phr_decode_chunked(&dec, buf, &bufsz);
    if (ret == -1) {
      ok(1);
      return;
    } else if (ret == -2) {
      /* continue */
    } else {
      ok(0);
      return;
    }
  }
  ok(0);
}

static void test_chunked(void)
{
  static void (*runners[])(int, const char*, const char *, size_t) = {
    test_chunked_at_once,
    test_chunked_per_byte,
    NULL
  };
  size_t i;

  for (i = 0; runners[i] != NULL; ++i) {
    runners[i](__LINE__, "b\r\nhello world0\r\n", "hello world", 0);
    runners[i](__LINE__, "6\r\nhello 5\r\nworld0\r\n", "hello world",
                         0);
    runners[i](__LINE__, "6;comment=hi\r\nhello 5\r\nworld0\r\n",
                         "hello world", 0);
    runners[i](__LINE__,
                         "6\r\nhello 5\r\nworld0\r\na: b\r\nc: d\r\n\r\n",
                         "hello world", sizeof("a: b\r\nc: d\r\n\r\n") - 1);
    runners[i](__LINE__, "b\r\nhello world0\r\n", "hello world", 0);
  }

  note("failures");
  test_chunked_failure(__LINE__, "z\r\nabcdefg");
  test_chunked_failure(__LINE__, "6\r\nhello fffffffff\r\nabcdefg");
}

int main(int argc, char **argv)
{
  subtest("request", test_request);
  subtest("response", test_response);
  subtest("headers", test_headers);
  subtest("chunked", test_chunked);
  return done_testing();
}
