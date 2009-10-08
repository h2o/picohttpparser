#include <stddef.h>
#include "picohttpparser.h"

#define CHECK_EOF()	\
  if (buf == buf_end) { \
    return -2;		\
  }

#define EXPECT(ch)    \
  CHECK_EOF();	      \
  if (*buf++ != ch) { \
    return -1;	      \
  }

#define ADVANCE_TOKEN()			       \
  for (; ; ++buf) {			       \
    CHECK_EOF();			       \
    if (*buf == ' ') {			       \
      break;				       \
    } else if (*buf == '\r' || *buf == '\n') { \
      return -1;			       \
    }					       \
  }

static int is_complete(const char* buf, const char* buf_end, size_t last_len)
{
  int ret_cnt = 0;
  buf = last_len < 3 ? buf : buf + last_len - 3;
  
  while (1) {
    CHECK_EOF();
    if (*buf == '\r') {
      ++buf;
      CHECK_EOF();
      EXPECT('\n');
      ++ret_cnt;
    } else if (*buf == '\n') {
      ++buf;
      ++ret_cnt;
    } else {
      ++buf;
      ret_cnt = 0;
    }
    if (ret_cnt == 2) {
      return 0;
    }
  }
  
  return -2;
}

int phr_parse_request(const char* _buf, size_t len, const char** method,
		      size_t* method_len, const char** path, size_t* path_len,
		      int* minor_version, struct phr_header* headers,
		      size_t* num_headers, size_t last_len)
{
  const char * buf = _buf, * buf_end = buf + len;
  size_t max_headers;
  
  /* if last_len != 0, check if the request is complete (a fast countermeasure
     againt slowloris */
  if (last_len != 0) {
    int r = is_complete(buf, buf_end, last_len);
    if (r != 0) {
      return r;
    }
  }
  
  /* skip first empty line (some clients add CRLF after POST content) */
  CHECK_EOF();
  if (*buf == '\r') {
    ++buf;
    EXPECT('\n');
  } else if (*buf == '\n') {
    ++buf;
  }
  
  /* parse request line */
  *method = buf;
  ADVANCE_TOKEN();
  *method_len = buf - *method;
  ++buf;
  *path = buf;
  ADVANCE_TOKEN();
  *path_len = buf - *path;
  ++buf;
  EXPECT('H'); EXPECT('T'); EXPECT('T'); EXPECT('P'); EXPECT('/'); EXPECT('1');
  EXPECT('.');
  *minor_version = 0;
  for (; ; ++buf) {
    CHECK_EOF();
    if ('0' <= *buf && *buf <= '9') {
      *minor_version = *minor_version * 10 + *buf - '0';
    } else {
      break;
    }
  }
  if (*buf == '\r') {
    ++buf;
    EXPECT('\n');
  } else if (*buf == '\n') {
    ++buf;
  } else {
    return -1;
  }

  /* parse headers */
  max_headers = *num_headers;
  for (*num_headers = 0; ; ++*num_headers) {
    CHECK_EOF();
    if (*buf == '\r') {
      ++buf;
      EXPECT('\n');
      break;
    } else if (*buf == '\n') {
      ++buf;
      break;
    }
    if (*num_headers == max_headers) {
      return -1;
    }
    if (*num_headers == 0 || ! (*buf == ' ' || *buf == '\t')) {
      /* parsing name */
      headers[*num_headers].name = buf;
      for (; ; ++buf) {
	CHECK_EOF();
	if (*buf == ':') {
	  break;
	} else if (*buf < ' ') {
	  return -1;
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
    headers[*num_headers].value = buf;
    for (; ; ++buf) {
      CHECK_EOF();
      if (*buf == '\r') {
	headers[*num_headers].value_len = buf - headers[*num_headers].value;
	++buf;
	EXPECT('\n');
	break;
      } else if (*buf == '\n') {
	headers[*num_headers].value_len = buf - headers[*num_headers].value;
	++buf;
	break;
      }
    }
  }
  
  return buf - _buf;
}

#undef CHECK_EOF
#undef EXPECT
#undef ADVACE_TOKEN
