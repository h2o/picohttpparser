#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "picohttpparser.h"

#define REQ "GET /cookies HTTP/1.1\r\nHost: 127.0.0.1:8090\r\nConnection: keep-alive\r\nCache-Control: max-age=0\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.17 (KHTML, like Gecko) Chrome/24.0.1312.56 Safari/537.17\r\nAccept-Encoding: gzip,deflate,sdch\r\nAccept-Language: en-US,en;q=0.8\r\nAccept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\r\nCookie: name=wookie\r\n\r\n"

int main(void)
{
  const char* method;
  size_t method_len;
  const char* path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[32];
  size_t num_headers;
  int i, ret;
  float start, end;

  start = (float)clock()/CLOCKS_PER_SEC;
  for (i = 0; i < 100000; i++) {
    num_headers = sizeof(headers) / sizeof(headers[0]);
    ret = phr_parse_request(REQ, sizeof(REQ) - 1, &method, &method_len, &path,
                &path_len, &minor_version, headers, &num_headers,
                0);
    assert(ret == sizeof(REQ) - 1);
  }
  end = (float)clock()/CLOCKS_PER_SEC;

  printf("Elapsed %f seconds.\n", (end - start));

  return 0;
}
