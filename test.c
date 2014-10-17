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
#include "picohttpparser.h"

void tests(int num)
{
  printf("1..%d\n", num);
}

void ok(int ok, const char* msg)
{
  static int testnum = 0;
  printf("%s %d - %s\n", ok ? "ok" : "not ok", ++testnum, msg);
}

int strrcmp(const char* s, size_t l, const char* t)
{
  return strlen(t) == l && memcmp(s, t, l) == 0;
}

int main(void)
{
  const char* method;
  size_t method_len;
  const char* path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[4];
  size_t num_headers;
  
  tests(56);
  
#define PARSE(s, last_len, exp, comment)				\
  num_headers = sizeof(headers) / sizeof(headers[0]);			\
  ok(phr_parse_request(s, sizeof(s) - 1, &method, &method_len, &path,	\
		       &path_len, &minor_version, headers,		\
		       &num_headers, last_len)				\
    == (exp == 0 ? strlen(s) : exp),					\
    comment)
  
  PARSE("GET / HTTP/1.0\r\n\r\n", 0, 0, "simple");
  ok(num_headers == 0, "# of headers");
  ok(strrcmp(method, method_len, "GET"), "method");
  ok(strrcmp(path, path_len, "/"), "path");
  ok(minor_version == 0, "minor version");
  
  PARSE("GET / HTTP/1.0\r\n\r", 0, -2, "partial");
  
  PARSE("GET /hoge HTTP/1.1\r\nHost: example.com\r\nCookie: \r\n\r\n", 0, 0,
	"parse headers");
  ok(num_headers == 2, "# of headers");
  ok(strrcmp(method, method_len, "GET"), "method");
  ok(strrcmp(path, path_len, "/hoge"), "path");
  ok(minor_version == 1, "minor version");
  ok(strrcmp(headers[0].name, headers[0].name_len, "Host"), "host");
  ok(strrcmp(headers[0].value, headers[0].value_len, "example.com"),
     "host value");
  ok(strrcmp(headers[1].name, headers[1].name_len, "Cookie"), "cookie");
  ok(strrcmp(headers[1].value, headers[1].value_len, ""), "cookie value");
  
  PARSE("GET / HTTP/1.0\r\nfoo: \r\nfoo: b\r\n  \tc\r\n\r\n", 0, 0,
	"parse multiline");
  ok(num_headers == 3, "# of headers");
  ok(strrcmp(method, method_len, "GET"), "method");
  ok(strrcmp(path, path_len, "/"), "path");
  ok(minor_version == 0, "minor version");
  ok(strrcmp(headers[0].name, headers[0].name_len, "foo"), "header #1 name");
  ok(strrcmp(headers[0].value, headers[0].value_len, ""), "header #1 value");
  ok(strrcmp(headers[1].name, headers[1].name_len, "foo"), "header #2 name");
  ok(strrcmp(headers[1].value, headers[1].value_len, "b"), "header #2 value");
  ok(headers[2].name == NULL, "header #3");
  ok(strrcmp(headers[2].value, headers[2].value_len, "  \tc"),
     "header #3 value");
  
  PARSE("GET", 0, -2, "incomplete 1");
  ok(method == NULL, "method not ready");
  PARSE("GET ", 0, -2, "incomplete 2");
  ok(strrcmp(method, method_len, "GET"), "method ready");
  PARSE("GET /", 0, -2, "incomplete 3");
  ok(path == NULL, "path not ready");
  PARSE("GET / ", 0, -2, "incomplete 4");
  ok(strrcmp(path, path_len, "/"), "path ready");
  PARSE("GET / H", 0, -2, "incomplete 5");
  PARSE("GET / HTTP/1.", 0, -2, "incomplete 6");
  PARSE("GET / HTTP/1.0", 0, -2, "incomplete 7");
  ok(minor_version == -1, "version not ready");
  PARSE("GET / HTTP/1.0\r", 0, -2, "incomplete 8");
  ok(minor_version == 0, "version is ready");
  
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
  ok(num_headers == 1, "# of headers");
  ok(strrcmp(method, method_len, "GET"), "method");
  ok(strrcmp(path, path_len, "/\xa0"), "path");
  ok(minor_version == 0, "minor version");
  ok(strrcmp(headers[0].name, headers[0].name_len, "h"), "header #1 name");
  ok(strrcmp(headers[0].value, headers[0].value_len, "c\xa2y"), "header #1 value");

#undef PARSE
  
  return 0;
}
