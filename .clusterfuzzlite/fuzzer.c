#include <stdint.h>
#include "picohttpparser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 10) {
        return 0;
    }

    const char *buf = (const char *)data;
    size_t buflen = size;

    int minor_version, status;
    const char *msg;
    size_t msg_len;
    struct phr_header headers[100];
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);
    size_t prevbuflen = 0;

    phr_parse_response(buf, buflen, &minor_version, &status, &msg, &msg_len, headers, &num_headers, prevbuflen);

    return 0;
}
