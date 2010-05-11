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
  
  tests(61);
  
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
  
  PARSE("H", 0, -2, "incomplete 1");
  PARSE("HTTP/1.", 0, -2, "incomplete 2");
  PARSE("HTTP/1.1", 0, -2, "incomplete 3");
  ok(minor_version == -1, "minor_version not ready");
  PARSE("HTTP/1.1 ", 0, -2, "incomplete 4");
  ok(minor_version == 1, "minor_version ready");
  PARSE("HTTP/1.1 2", 0, -2, "incomplete 5");
  PARSE("HTTP/1.1 200", 0, -2, "incomplete 6");
  ok(status == 0, "status not ready");
  PARSE("HTTP/1.1 200 ", 0, -2, "incomplete 7");
  ok(status == 200, "status ready");
  PARSE("HTTP/1.1 200 O", 0, -2, "incomplete 8");
  PARSE("HTTP/1.1 200 OK\r", 0, -2, "incomplete 9");
  ok(msg == NULL, "message not ready");
  PARSE("HTTP/1.1 200 OK\r\n", 0, -2, "incomplete 10");
  ok(strrcmp(msg, msg_len, "OK"), "message ready");
  PARSE("HTTP/1.1 200 OK\n", 0, -2, "incomplete 11");
  ok(strrcmp(msg, msg_len, "OK"), "message ready 2");

  PARSE("HTTP/1.1 200 OK\r\nA: 1\r", 0, -2, "incomplete 11");
  ok(num_headers == 0, "header not ready");
  PARSE("HTTP/1.1 200 OK\r\nA: 1\r\n", 0, -2, "incomplete 12");
  ok(num_headers == 1, "header ready");
  ok(strrcmp(headers[0].name, headers[0].name_len, "A"), "header #1 name");
  ok(strrcmp(headers[0].value, headers[0].value_len, "1"), "header #1 value");
  
  PARSE("HTTP/1.0 200 OK\r\n\r", strlen("GET /hoge HTTP/1.0\r\n\r") - 1,
	-2, "slowloris (incomplete)");
  PARSE("HTTP/1.0 200 OK\r\n\r\n", strlen("HTTP/1.0 200 OK\r\n\r\n") - 1,
	0, "slowloris (complete)");
  
  PARSE("HTTP/1. 200 OK\r\n\r\n", 0, -1, "invalid http version");
  PARSE("HTTP/1.2z 200 OK\r\n\r\n", 0, -1, "invalid http version 2");
  PARSE("HTTP/1.1  OK\r\n\r\n", 0, -1, "no status code");
  
#undef PARSE
  
  return 0;
}
