#ifndef picohttpparser_h
#define picohttpparser_h

/* $Id$ */

#ifdef __cplusplus
extern "C" {
#endif

/* contains name and value of a header (name == NULL if is a continuing line
 * of a multiline header */
struct phr_header {
  const char* name;
  size_t name_len;
  const char* value;
  size_t value_len;
};

/* returns number of bytes cosumed if successful, -2 if request is partial,
 * -1 if failed */
int phr_parse_request(const char* buf, size_t len, const char** method,
		      size_t* method_len, const char** path,
		      size_t* path_len, int* minor_version,
		      struct phr_header* headers, size_t* num_headers,
		      size_t last_len);

/* ditto */
int phr_parse_response(const char* _buf, size_t len, int *minor_version,
              int *status, const char **msg, size_t *msg_len,
              struct phr_header* headers, size_t* num_headers,
              size_t last_len);

#ifdef __cplusplus
}
#endif

#endif
