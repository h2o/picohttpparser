#include <stddef.h>
#include "picohttpparser.h"

#define CHECK_EOF()	\
  if (buf == buf_end) { \
    return -2;		\
  }

#define EXPECT_CHAR(ch) \
  CHECK_EOF();		\
  if (*buf++ != ch) {	\
    return -1;		\
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

#define ADVANCE_EOL(tok, toklen) do {	      \
    tok = buf;				      \
    for (; ; ++buf) {			      \
      CHECK_EOF();			      \
      if (*buf == '\r' || *buf == '\n') {     \
	break;				      \
      }					      \
    }					      \
    toklen = buf - tok;			      \
    if (*buf == '\r') {			      \
      ++buf;				      \
      EXPECT_CHAR('\n');		      \
    } else { /* should be: *buf == '\n' */    \
      ++buf;				      \
    }					      \
  } while (0)
  
static int is_complete(const char* buf, const char* buf_end, size_t last_len)
{
  int ret_cnt = 0;
  buf = last_len < 3 ? buf : buf + last_len - 3;
  
  while (1) {
    CHECK_EOF();
    if (*buf == '\r') {
      ++buf;
      CHECK_EOF();
      EXPECT_CHAR('\n');
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

/* *_buf is always within [buf, buf_end) upon success */
static int parse_int(const char** _buf, const char* buf_end, int* value)
{
  const char* buf = *_buf;
  
  CHECK_EOF();
  if (! ('0' <= *buf && *buf <= '9')) {
    return -1;
  }
  *value = 0;
  for (; ; ++buf) {
    CHECK_EOF();
    if ('0' <= *buf && *buf <= '9') {
      *value = *value * 10 + *buf - '0';
    } else {
      break;
    }
  }
  
  *_buf = buf;
  return 0;
}

/* returned pointer is always within [buf, buf_end), or null */
static int parse_http_version(const char** _buf, const char* buf_end,
			      int* minor_version)
{
  const char* buf = *_buf;
  int r;
  
  EXPECT_CHAR('H'); EXPECT_CHAR('T'); EXPECT_CHAR('T'); EXPECT_CHAR('P');
  EXPECT_CHAR('/'); EXPECT_CHAR('1'); EXPECT_CHAR('.');
  
  if ((r = parse_int(&buf, buf_end, minor_version)) != 0) {
    return r;
  }
  
  *_buf = buf;
  return 0;
}

static int parse_headers(const char* buf, const char* _buf, const char* buf_end,
			 struct phr_header* headers, size_t* num_headers)
{
  size_t max_headers = *num_headers;
  for (*num_headers = 0; ; ++*num_headers) {
    CHECK_EOF();
    if (*buf == '\r') {
      ++buf;
      EXPECT_CHAR('\n');
      break;
    } else if (*buf == '\n') {
      ++buf;
      break;
    }
    if (*num_headers == max_headers) {
      return -1;
    }
    if (*num_headers == 0 || ! (*buf == ' ' || *buf == '\t')) {
      /* parsing name, but do not discard SP before colon, see
       * http://www.mozilla.org/security/announce/2006/mfsa2006-33.html */
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
    ADVANCE_EOL(headers[*num_headers].value, headers[*num_headers].value_len);
  }
  return buf - _buf;
}

int phr_parse_request(const char* _buf, size_t len, const char** method,
		      size_t* method_len, const char** path, size_t* path_len,
		      int* minor_version, struct phr_header* headers,
		      size_t* num_headers, size_t last_len)
{
  const char * buf = _buf, * buf_end = buf + len;
  int r;
  
  /* if last_len != 0, check if the request is complete (a fast countermeasure
     againt slowloris */
  if (last_len != 0) {
    if ((r = is_complete(buf, buf_end, last_len)) != 0) {
      return r;
    }
  }
  
  /* skip first empty line (some clients add CRLF after POST content) */
  CHECK_EOF();
  if (*buf == '\r') {
    ++buf;
    EXPECT_CHAR('\n');
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
  if ((r = parse_http_version(&buf, buf_end, minor_version)) != 0) {
    return r;
  }
  if (*buf == '\r') {
    ++buf;
    EXPECT_CHAR('\n');
  } else if (*buf == '\n') {
    ++buf;
  } else {
    return -1;
  }

  return parse_headers(buf, _buf, buf_end, headers, num_headers);
}

int phr_parse_response(const char* _buf, size_t len, int* minor_version,
		       int* status, const char** msg, size_t* msg_len,
		       struct phr_header* headers, size_t* num_headers,
		       size_t last_len)
{
  const char * buf = _buf, * buf_end = buf + len;
  int r;
  
  /* if last_len != 0, check if the response is complete (a fast countermeasure
     against slowloris */
  if (last_len != 0) {
    if ((r = is_complete(buf, buf_end, last_len)) != 0) {
      return r;
    }
  }
  
  /* parse "HTTP/1.x" */
  if ((r = parse_http_version(&buf, buf_end, minor_version)) != 0) {
    return r;
  }
  /* skip space */
  if (*buf++ != ' ') {
    return -1;
  }
  /* parse status code */
  if ((r = parse_int(&buf, buf_end, status)) != 0) {
    return r;
  }
  /* skip space */
  if (*buf++ != ' ') {
    return -1;
  }
  /* get message */
  ADVANCE_EOL(*msg, *msg_len);

  return parse_headers(buf, _buf, buf_end, headers, num_headers);
}

#undef CHECK_EOF
#undef EXPECT_CHAR
#undef ADVANCE_TOKEN
