#include <stddef.h>
#include "picohttpparser.h"

/* $Id$ */

#if __GNUC__ >= 3
# define likely(x)	__builtin_expect(!!(x), 1)
# define unlikely(x)	__builtin_expect(!!(x), 0)
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#define CHECK_EOF()	\
  if (buf == buf_end) {	\
    *ret = -2;		\
    return NULL;	\
  }

#define EXPECT_CHAR(ch) \
  CHECK_EOF();		\
  if (*buf++ != ch) {	\
    *ret = -1;		\
    return NULL;	\
  }

#define ADVANCE_TOKEN(tok, toklen) do {		       \
    const char* tok_start = buf; 		       \
    for (; ; ++buf) {				       \
      CHECK_EOF();				       \
      if (*buf == ' ') {			       \
	break;					       \
      } else if (*buf == '\015' || *buf == '\012') {   \
	*ret = -1;				       \
	return NULL;				       \
      }						       \
    }						       \
    tok = tok_start;				       \
    toklen = buf - tok_start;			       \
  } while (0)

static const char* get_token_to_eol(const char* buf, const char* buf_end,
				    const char** token, size_t* token_len,
				    int* ret)
{
  const char* token_start = buf;
  
  while (1) {
    if (likely(buf_end - buf >= 16)) {
      unsigned i;
      for (i = 0; i < 16; i++, ++buf) {
	if (unlikely((unsigned char)*buf <= '\015')
	    && (*buf == '\015' || *buf == '\012')) {
	  goto EOL_FOUND;
	}
      }
    } else {
      for (; ; ++buf) {
	CHECK_EOF();
	if (unlikely((unsigned char)*buf <= '\015')
	    && (*buf == '\015' || *buf == '\012')) {
	  goto EOL_FOUND;
	}
      }
    }
  }
 EOL_FOUND:
  if (*buf == '\015') {
    ++buf;
    EXPECT_CHAR('\012');
    *token_len = buf - 2 - token_start;
  } else { /* should be: *buf == '\012' */
    *token_len = buf - token_start;
    ++buf;
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
    if (*num_headers == 0 || ! (*buf == ' ' || *buf == '\t')) {
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
  
  return buf - buf_start;
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
  
  return buf - buf_start;
}

#undef CHECK_EOF
#undef EXPECT_CHAR
#undef ADVANCE_TOKEN
