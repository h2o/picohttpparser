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

  PARSE("GET /hoge HTTP/1.1\r\nHost: example.com\r\nUser-Agent: \343\201\262\343/1.0\r\n\r\n", 0, 0,
        "multibyte included");
  ok(num_headers == 2);
  ok(bufis(method, method_len, "GET"));
  ok(bufis(path, path_len, "/hoge"));
  ok(minor_version == 1);
  ok(bufis(headers[0].name, headers[0].name_len, "Host"));
  ok(bufis(headers[0].value, headers[0].value_len, "example.com"));
  ok(bufis(headers[1].name, headers[1].name_len, "User-Agent"));
  ok(bufis(headers[1].value, headers[1].value_len, "\343\201\262\343/1.0"));

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

int main(int argc, char **argv)
{
  subtest("request", test_request);
  subtest("response", test_response);
  return done_testing();
}
