PicoHTTPParser
=============

Copyright (c) 2009-2014 [Kazuho Oku](https://github.com/kazuho), [Tokuhiro Matsuno](https://github.com/tokuhirom), [Daisuke Murase](https://github.com/typester), [Shigeo Mitsunari](https://github.com/herumi)

PicoHTTPParser is a tiny, primitive, fast HTTP request/response parser.

Unlike most parsers, it is stateless and does not allocate memory by itself.
All it does is accept pointer to buffer and the output structure, and setups the pointers in the latter to point at the necessary portions of the buffer.

The code is widely deployed within Perl applications through popular modules that use it, including [Plack](https://metacpan.org/pod/Plack), [Starman](https://metacpan.org/pod/Starman), [Starlet](https://metacpan.org/pod/Starlet), [Furl](https://metacpan.org/pod/Furl).  It is also the HTTP/1 parser of [H2O](https://github.com/h2o/h2o).

Check out [test.c] to find out how to use the parser.

The software is dual-licensed under the Perl License or the MIT License.

Usage
-----

The library exposes four functions: `phr_parse_request`, `phr_parse_response`, `phr_parse_headers`, `phr_decode_chunked`.

### phr_parse_request

The example below reads an HTTP request from socket `sock` using `read(2)`, parses it using `phr_parse_request`, and prints the details.

```c
char buf[4096], *method, *path;
int pret, minor_version;
struct phr_header headers[100];
size_t buflen = 0, prevbuflen = 0, method_len, path_len, num_headers;
ssize_t rret;

while (1) {
    /* read the request */
    while ((rret = read(sock, buf + buflen, sizeof(buf) - buflen)) == -1 && errno == EINTR)
        ;
    if (rret <= 0)
        return IOError;
    prevbuflen = buflen;
    buflen += rret;
    /* parse the request */
    num_headers = sizeof(headers) / sizeof(headers[0]);
    pret = phr_parse_request(buf, buflen, &method, &method_len, &path, &path_len,
                             &minor_version, headers, &num_headers, prevbuflen);
    if (pret > 0)
        break; /* successfully parsed the request */
    else if (pret == -1)
        return ParseError;
    /* request is incomplete, continue the loop */
    assert(pret == -2);
    if (buflen == sizeof(buf))
        return RequestIsTooLongError;
}

printf("request is %d bytes long\n", pret);
printf("method is %.*s\n", (int)method_len, method);
printf("path is %.*s\n", (int)path_len, path);
printf("HTTP version is 1.%d\n", minor_version);
printf("headers:\n");
for (i = 0; i != num_headers; ++i) {
    printf("%.*s: %.*s\n", (int)headers[i].name_len, headers[i].name,
           (int)headers[i].value_len, headers[i].value);
}
```

### Accessing header names and values

The method, path, header names, and header values are slices of the input buffer,
not NUL-terminated strings. Keep the buffer alive and unchanged while using them.
Use their corresponding lengths when printing or copying; `%s`, `strlen`, and
`strcmp` cannot be used directly on these slices.

Header names are case-insensitive under RFC 9110, which hard-codes ASCII. Do not
use locale-dependent functions such as `strcasecmp` / `strncasecmp`. Match the
full name length as well as its contents so that, for example, `Host-Extra` does
not match `Host`. After a successful parse, this example prints each `Host`
value with an ASCII-only comparison:

```c
#include <stdio.h>

static int ascii_hdr_eq(const char *name, size_t name_len,
                        const char *literal /* lowercase ASCII */)
{
    size_t i, lit_len;
    for (lit_len = 0; literal[lit_len] != '\0'; ++lit_len)
        ;
    if (name == NULL || name_len != lit_len)
        return 0;
    for (i = 0; i != name_len; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c - 'A' + 'a');
        if (c != (unsigned char)literal[i])
            return 0;
    }
    return 1;
}

for (size_t i = 0; i != num_headers; ++i) {
    /* RFC 9112 Section 5.2: unfold obs-fold lines, or reject the section.
     * This example unfolds into the preceding header's printed value. */
    if (headers[i].name == NULL) {
        continue; /* handled when the preceding header was printed */
    }
    if (!ascii_hdr_eq(headers[i].name, headers[i].name_len, "host"))
        continue;
    fwrite(headers[i].value, 1, headers[i].value_len, stdout);
    for (size_t j = i + 1; j != num_headers && headers[j].name == NULL; ++j) {
        /* The parser preserves leading SP / HTAB on continuation lines. */
        size_t start = 0;
        while (start != headers[j].value_len &&
               (headers[j].value[start] == ' ' || headers[j].value[start] == '\t'))
            ++start;
        fputc(' ', stdout);
        fwrite(headers[j].value + start, 1, headers[j].value_len - start, stdout);
    }
    fputc('\n', stdout);
}
```

A header continuation has `name == NULL`; its value belongs to the preceding
header. Silently skipping those entries truncates the field. Applications must
either unfold the value (as above) or reject the header section, per RFC 9112
Section 5.2. The example does not enforce rules about repeated headers;
applications must apply those for the headers they consume.

### phr_parse_response, phr_parse_headers

`phr_parse_response` and `phr_parse_headers` provide similar interfaces as `phr_parse_request`.  `phr_parse_response` parses an HTTP response, and `phr_parse_headers` parses the headers only.

### phr_decode_chunked

The example below decodes incoming data in chunked-encoding.  The data is decoded in-place.

```c
struct phr_chunked_decoder decoder = {}; /* zero-clear */
char *buf = malloc(4096);
size_t size = 0, capacity = 4096, rsize;
ssize_t rret, pret;

/* set consume_trailer to 1 to discard the trailing header, or the application
 * should call phr_parse_headers to parse the trailing header */
decoder.consume_trailer = 1;

do {
    /* expand the buffer if necessary */
    if (size == capacity) {
        capacity *= 2;
        buf = realloc(buf, capacity);
        assert(buf != NULL);
    }
    /* read */
    while ((rret = read(sock, buf + size, capacity - size)) == -1 && errno == EINTR)
        ;
    if (rret <= 0)
        return IOError;
    /* decode */
    rsize = rret;
    pret = phr_decode_chunked(&decoder, buf + size, &rsize);
    if (pret == -1)
        return ParseError;
    size += rsize;
} while (pret == -2);

/* successfully decoded the chunked data */
assert(pret >= 0);
printf("decoded data is at %p (%zu bytes)\n", buf, size);
```

Benchmark
---------

![benchmark results](http://i.gyazo.com/a85c18d3162dfb46b485bb41e0ad443a.png)

The benchmark code is from [fukamachi/fast-http@6b91103](https://github.com/fukamachi/fast-http/tree/6b9110347c7a3407310c08979aefd65078518478).

The internals of picohttpparser has been described to some extent in [my blog entry]( http://blog.kazuhooku.com/2014/11/the-internals-h2o-or-how-to-write-fast.html).
