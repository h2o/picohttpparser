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
  int minor_version;
  int status;
  const char *msg;
  size_t msg_len;
  struct phr_header headers[4];
  size_t num_headers;
  
  tests(34);
  
#define PARSE(s, last_len, exp, comment)				\
  num_headers = sizeof(headers) / sizeof(headers[0]);			\
  ok(phr_parse_response(s, strlen(s), &minor_version, &status,       	\
		       &msg, &msg_len, headers,		                \
		       &num_headers, last_len)				\
    == (exp == 0 ? strlen(s) : exp),					\
    comment)
  
  PARSE("HTTP/1.0 200 OK\r\n\r\n", 0, 0, "simple");
  ok(num_headers == 0, "# of headers");
  ok(status      == 200, "http status code");
  ok(minor_version = 1, "method");
  ok(strrcmp(msg, msg_len, "OK"), "msg");
  
  PARSE("HTTP/1.0 200 OK\r\n\r", 0, -2, "partial");

  PARSE("HTTP/1.1 200 OK\r\nHost: example.com\r\nCookie: \r\n\r\n", 0, 0,
	"parse headers");
  ok(num_headers == 2, "# of headers");
  ok(minor_version == 1, "minor_version");
  ok(status == 200, "status");
  ok(strrcmp(msg, msg_len, "OK"), "msg");
  ok(strrcmp(headers[0].name, headers[0].name_len, "Host"), "host");
  ok(strrcmp(headers[0].value, headers[0].value_len, "example.com"),
     "host value");
  ok(strrcmp(headers[1].name, headers[1].name_len, "Cookie"), "cookie");
  ok(strrcmp(headers[1].value, headers[1].value_len, ""), "cookie value");

  PARSE("HTTP/1.0 200 OK\r\nfoo: \r\nfoo: b\r\n  \tc\r\n\r\n", 0, 0,
	"parse multiline");
  ok(num_headers == 3, "# of headers");
  ok(minor_version == 0, "minor_version");
  ok(status == 200, "status");
  ok(strrcmp(msg, msg_len, "OK"), "msg");
  ok(strrcmp(headers[0].name, headers[0].name_len, "foo"), "header #1 name");
  ok(strrcmp(headers[0].value, headers[0].value_len, ""), "header #1 value");
  ok(strrcmp(headers[1].name, headers[1].name_len, "foo"), "header #2 name");
  ok(strrcmp(headers[1].value, headers[1].value_len, "b"), "header #2 value");
  ok(headers[2].name == NULL, "header #3");
  ok(strrcmp(headers[2].value, headers[2].value_len, "  \tc"),
     "header #3 value");

  PARSE("HTTP/1.0 500 Internal Server Error\r\n\r\n", 0, 0,
	"internal server error");
  ok(num_headers == 0, "# of headers");
  ok(minor_version == 0, "minor_version");
  ok(status == 500, "status");
  ok(strrcmp(msg, msg_len, "Internal Server Error"), "msg");
  ok(msg_len == sizeof("Internal Server Error")-1, "msg_len");

  PARSE("HTTP/1.0 200 OK\r\n\r", strlen("GET /hoge HTTP/1.0\r\n\r") - 1,
	-2, "slowloris (incomplete)");
  PARSE("HTTP/1.0 200 OK\r\n\r\n", strlen("HTTP/1.0 200 OK\r\n\r\n") - 1,
	0, "slowloris (complete)");
  
#undef PARSE
  
  return 0;
}
