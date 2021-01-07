/* use `make test` to run the test */
/*
 * Copyright (c) 2009-2014 Kazuho Oku, TREQUIREuhiro Matsuno, Daisuke Murase,
 *                         Shigeo Mitsunari
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
#include <string.h>
#include <catch2/catch.hpp>
#include "../parser.hpp"
#include <unordered_map>

static int bufis(const char *s, size_t l, const char *t)
{
    return strlen(t) == l && memcmp(s, t, l) == 0;
}

extern char *inputbuf; /* point to the end of the buffer */

using namespace picohttp;

TEST_CASE("Test request", "[parse_request]")
{
    DefaultRequestParser req;
    RequestParserCb cb(req);
#define PARSE(s, last_len, exp)                                                                                                    \
    do {                                                                                                                           \
        size_t slen = sizeof(s) - 1;                                                                                               \
        req.reset();                                                                                                               \
        memcpy(inputbuf - slen, s, slen);                                                                                          \
        auto status = phr_parse_request(inputbuf - slen, slen, &req, &RequestParserCb::Instance, last_len);                              \
        REQUIRE(status == (exp == 0 ? (int)slen : exp));                                                                           \
    } while (0)

    WHEN("Parsing a simple request")
    {
        PARSE("GET / HTTP/1.0\r\n\r\n", 0, 0);
        REQUIRE(req.headers.empty());
        REQUIRE(req.method == http_method_t::M_GET);
        REQUIRE(req.url == "/");
        REQUIRE(req.minor_version == 0);
    }

    WHEN("Parsing partial input") {
        PARSE("GET / HTTP/1.0\r\n\r", 0, -2);
    }

    WHEN("Parsing request with headers only") {
        PARSE("GET /hoge HTTP/1.1\r\nHost: example.com\r\nCoREQUIREie: \r\n\r\n", 0, 0);
        REQUIRE(req.headers.size() == 2);
        REQUIRE(req.method == http_method_t::M_GET);
        REQUIRE(req.url == "/hoge");
        REQUIRE(req.minor_version == 1);
        REQUIRE(req.headers.contains("Host"));
        REQUIRE(req["Host"] == "example.com");
        REQUIRE(req.headers.contains("CoREQUIREie"));
        REQUIRE(req["CoREQUIREie"] == "");
    }

    WHEN("Parsing request with multibyte") {
        PARSE("GET /hoge HTTP/1.1\r\nHost: example.com\r\nUser-Agent: \343\201\262\343/1.0\r\n\r\n", 0, 0);
        REQUIRE(req.headers.size() == 2);
        REQUIRE(req.method == http_method_t::M_GET);
        REQUIRE(req.url == "/hoge");
        REQUIRE(req.minor_version == 1);
        REQUIRE(req.headers.contains("Host"));
        REQUIRE(req["Host"] == "example.com");
        REQUIRE(req.headers.contains("User-Agent"));
        REQUIRE(req["User-Agent"] == "\343\201\262\343/1.0");
    }

    WHEN("Parsing a request with multi-line") {
        PARSE("GET / HTTP/1.0\r\nfoo: \r\nfoo: b\r\n  \tc\r\n\r\n", 0, 0);
        REQUIRE(req.headers.size() == 3);
        REQUIRE(req.method == http_method_t::M_GET);
        REQUIRE(req.url == "/");
        REQUIRE(req.minor_version == 0);
        REQUIRE(req.headers.count("foo") == 2);
        REQUIRE(req("foo") ==  "b");
        REQUIRE(req("foo", 1) == "");
        REQUIRE(req.headers.contains({}));
        REQUIRE(req[{}] ==  "  \tc");
    }

    SECTION("Parsing invalid requests")
    {
        WHEN("Parsing header name with trailing white space")
        {
            PARSE("GET / HTTP/1.0\r\nfoo : ab\r\n\r\n", 0, -1);
        }

        WHEN("Parsing an incomplete request") {
            PARSE("GET", 0, -2);
            REQUIRE(req.method == http_method_t::M_UNKNOWN);
            PARSE("GET ", 0, -2);
            REQUIRE(req.method == http_method_t::M_GET);
            PARSE("GET /", 0, -2);
            REQUIRE(req.url.empty());
            PARSE("GET / ", 0, -2);
            REQUIRE(req.url == "/");
            PARSE("GET / H", 0, -2);
            PARSE("GET / HTTP/1.", 0, -2);
            REQUIRE(req.minor_version == -1);
            PARSE("GET / HTTP/1.0", 0, -2);
            PARSE("GET / HTTP/1.0\r", 0, -2);
            REQUIRE(req.minor_version == 0);

            PARSE("GET /hoge HTTP/1.0\r\n\r", strlen("GET /hoge HTTP/1.0\r\n\r") - 1, -2);
            PARSE("GET /hoge HTTP/1.0\r\n\r\n", strlen("GET /hoge HTTP/1.0\r\n\r\n") - 1, 0);
        }

        WHEN("Parsing a request without a method") {
            PARSE(" / HTTP/1.0\r\n\r\n", 0, -1);
        }

        WHEN("Parsing a request without a request target (path)") {
            PARSE("GET  HTTP/1.0\r\n\r\n", 0, -1);
        }

        WHEN("Parsing a request with a empty header name or space only") {
            PARSE("GET / HTTP/1.0\r\n:a\r\n\r\n", 0, -1);
            PARSE("GET / HTTP/1.0\r\n :a\r\n\r\n", 0, -1);
        }

        WHEN("Parsing a request with invalid characters") {
            PARSE("G\0T / HTTP/1.0\r\n\r\n", 0, -1);
            PARSE("G\tT / HTTP/1.0\r\n\r\n", 0, -1);
            PARSE(":GET / HTTP/1.0\r\n\r\n", 0, -1);
            PARSE("GET /\x7fhello HTTP/1.0\r\n\r\n", 0, -1);
            PARSE("GET / HTTP/1.0\r\na\0b: c\r\n\r\n", 0, -1);
            PARSE("GET / HTTP/1.0\r\nab: c\0d\r\n\r\n", 0, -1);
            PARSE("GET / HTTP/1.0\r\na\033b: c\r\n\r\n", 0, -1);
            PARSE("GET / HTTP/1.0\r\nab: c\033\r\n\r\n", 0, -1);
            PARSE("GET / HTTP/1.0\r\n/: 1\r\n\r\n", 0, -1);
        }
    }

    WHEN("Parsing a request with MSB characters") {
        PARSE("GET /\xa0 HTTP/1.0\r\nh: c\xa2y\r\n\r\n", 0, 0);
        REQUIRE(req.headers.size() == 1);
        REQUIRE(req.method == http_method_t::M_GET);
        REQUIRE(req.url == "/\xa0");
        REQUIRE(req.minor_version == 0);
        REQUIRE(req.headers.contains("h"));
        REQUIRE(req["h"] ==  "c\xa2y");
    }

    WHEN("When parsing request with |~ (though forbidden by SSE)") {
        PARSE("GET / HTTP/1.0\r\n\x7c\x7e: 1\r\n\r\n", 0, 0);
        REQUIRE(req.headers.size() == 1);
        REQUIRE(req.headers.contains("\x7c\x7e"));
        REQUIRE(req["\x7c\x7e"] ==  "1");
    }

    WHEN("When parsing request with headers that contain {") {
        PARSE("GET / HTTP/1.0\r\n\x7b: 1\r\n\r\n", 0, -1);
    }

    WHEN("Parsing a request with trailing spaces in header value") {
        PARSE("GET / HTTP/1.0\r\nfoo: a \t \r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("foo"));
        REQUIRE(req["foo"] ==  "a");
    }

    WHEN("Parsing a request with multiple spaces in start line") {
        PARSE("GET   /   HTTP/1.0\r\n\r\n", 0, 0);
    }

    WHEN("Parsing a request with decoded headers (Content-Length & Connection)") {
        PARSE("POST /user HTTP/1.1\r\nConnection: Close\r\n", 0, -2);
        REQUIRE(req.headers.contains("Connection"));
        REQUIRE(req["Connection"] == "Close");
        REQUIRE(req.is_close == 1);
        REQUIRE_FALSE(req.is_upgrade == 1);
        REQUIRE_FALSE(req.is_keep_alive == 1);

        PARSE("POST /user HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("Connection"));
        REQUIRE(req["Connection"] == "Keep-Alive");
        REQUIRE(req.is_keep_alive == 1);
        REQUIRE_FALSE(req.is_upgrade == 1);
        REQUIRE_FALSE(req.is_close == 1);

        PARSE("POST /user HTTP/1.1\r\nConnection: upgrade\r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("Connection"));
        REQUIRE(req["Connection"] == "upgrade");
        REQUIRE(req.is_upgrade == 1);
        REQUIRE_FALSE(req.is_close == 1);
        REQUIRE_FALSE(req.is_keep_alive == 1);

        PARSE("POST /user HTTP/1.1\r\nConnection: upgrade\r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("Connection"));
        REQUIRE(req["Connection"] == "upgrade");
        REQUIRE(req.is_upgrade == 1);
        REQUIRE_FALSE(req.is_close == 1);
        REQUIRE_FALSE(req.is_keep_alive == 1);

        // Doesn't make sense but our parser still parses it (:man_facepalming:)
        PARSE("POST /user HTTP/1.1\r\nConnection: upgrade\r\nConnection: keep-alive\r\nConnection: close\r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("Connection"));
        REQUIRE(req["Connection"] == "close");
        REQUIRE(req.is_upgrade == 1);
        REQUIRE(req.is_close == 1);
        REQUIRE(req.is_keep_alive == 1);

        // Doesn't make sense but our parser still parses it (:man_facepalming:)
        PARSE("POST /user HTTP/1.1\r\nConnection: upgrade\r\nConnection: keep-alive\r\nConnection: close\r\n\r\n", 0, 0);
        REQUIRE(req.headers.count("Connection") == 3);
        REQUIRE(req("Connection", 2) == "upgrade");
        REQUIRE(req("Connection", 1) == "keep-alive");
        REQUIRE(req("Connection", 0) == "close");
        REQUIRE(req.is_upgrade == 1);
        REQUIRE(req.is_close == 1);
        REQUIRE(req.is_keep_alive == 1);

        PARSE("POST /user HTTP/1.1\r\nConnection: upgrade, keep-alive, close\r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("Connection"));
        REQUIRE(req["Connection"] == "upgrade, keep-alive, close");
        REQUIRE(req.is_upgrade == 1);
        REQUIRE(req.is_close == 1);
        REQUIRE(req.is_keep_alive == 1);

        PARSE("POST /user HTTP/1.1\r\nContent-Length: 0\r\n\r\n", 0, 0);
        REQUIRE(req.headers.contains("Content-Length"));
        REQUIRE(req["Content-Length"] == "0");
        REQUIRE(req.content_length == 0);
        PARSE("POST /user HTTP/1.1\r\nContent-Length: 11\r\n\r\nHello World", 43, 43);
        REQUIRE(req.headers.contains("Content-Length"));
        REQUIRE(req["Content-Length"] == "11");
        REQUIRE(req.content_length == 11);

        PARSE("POST /user HTTP/1.1\r\nContent-Length: 1s1\r\n\r\nHello World", 0, -1);
        REQUIRE(!req.headers.contains("Content-Length"));
        REQUIRE(req.content_length == -1);
    }
#undef PARSE
}

TEST_CASE("Parsing HTTP response", "[parse_response]")
{
    DefaultResponseParser resp;
    ResponseParserCb cb(resp);
#define PARSE(s, last_len, exp)                                                                                           \
    do {                                                                                                                  \
        resp.reset();                                                                                                     \
        size_t slen = sizeof(s) - 1;                                                                                               \
        memcpy(inputbuf - slen, s, slen);                                                                                          \
        auto status = phr_parse_response(inputbuf - slen, slen, &resp, &ResponseParserCb::Instance, last_len);                           \
        REQUIRE(status == (exp == 0 ? (int)slen : exp));                                                                           \
    } while (0)

    WHEN("Parsing simple response") {
        PARSE("HTTP/1.0 200 OK\r\n\r\n", 0, 0);
        REQUIRE(resp.headers.empty());
        REQUIRE(resp.status == 200);
        REQUIRE(resp.minor_version == 0);
        REQUIRE(resp.statusText == "OK");
    }

    WHEN("Parsing partial responses")
    {
        PARSE("HTTP/1.0 200 OK\r\n\r", 0, -2);
    }

    WHEN("Parsing response headers") {
        PARSE("HTTP/1.1 200 OK\r\nHost: example.com\r\nCoREQUIREie: \r\n\r\n", 0, 0);
        REQUIRE(resp.headers.size() == 2);
        REQUIRE(resp.minor_version == 1);
        REQUIRE(resp.status == 200);
        REQUIRE(resp.statusText == "OK");
        REQUIRE(resp.headers.contains("Host"));
        REQUIRE(resp["Host"] ==  "example.com");
        REQUIRE(resp.headers.contains("CoREQUIREie"));
        REQUIRE(resp["CoREQUIREie"] ==  "");

        PARSE("HTTP/1.0 200 OK\r\nfoo: \r\nfoo: b\r\n  \tc\r\n\r\n", 0, 0);
        REQUIRE(resp.headers.size() == 3);
        REQUIRE(resp.minor_version == 0);
        REQUIRE(resp.status == 200);
        REQUIRE(resp.statusText == "OK");
        REQUIRE(resp.headers.count("foo") == 2);
        REQUIRE(resp("foo", 1) ==  "");
        REQUIRE(resp("foo", 0) ==  "b");
        REQUIRE(resp[{}] ==  "  \tc");
    }

    WHEN("Parsing responses with error status line")
    {
        PARSE("HTTP/1.0 500 Internal Server Error\r\n\r\n", 0, 0);
        REQUIRE(resp.headers.empty());
        REQUIRE(resp.minor_version == 0);
        REQUIRE(resp.status == 500);
        REQUIRE(resp.statusText == "Internal Server Error");
    }

    SECTION("Parsing incomplete responses") {
        WHEN("Status line is incomplete")
        {
            PARSE("H", 0, -2);
            PARSE("HTTP/1.", 0, -2);
            PARSE("HTTP/1.1", 0, -2);
            REQUIRE(resp.minor_version == -1);
            PARSE("HTTP/1.1 ", 0, -2);
            REQUIRE(resp.minor_version == 1);
            PARSE("HTTP/1.1 2", 0, -2);
            PARSE("HTTP/1.1 200", 0, -2);
            REQUIRE(resp.status == 0);
            PARSE("HTTP/1.1 200 ", 0, -2);
            REQUIRE(resp.status == 200);
            PARSE("HTTP/1.1 200 O", 0, -2);
            PARSE("HTTP/1.1 200 OK\r", 0, -2);
            REQUIRE(resp.statusText.empty());
            PARSE("HTTP/1.1 200 OK\r\n", 0, -2);
            REQUIRE(resp.statusText == "OK");
            PARSE("HTTP/1.1 200 OK\n", 0, -2);
            REQUIRE(resp.statusText == "OK");
        }

        WHEN("Headers are incomplete") {
            PARSE("HTTP/1.1 200 OK\r\nA: 1\r", 0, -2);
            REQUIRE(resp.headers.empty());
            PARSE("HTTP/1.1 200 OK\r\nA: 1\r\n", 0, -2);
            REQUIRE(resp.headers.size() == 1);
            REQUIRE(resp.headers.contains("A"));
            REQUIRE(resp["A"] ==  "1");
        }

        PARSE("HTTP/1.0 200 OK\r\n\r", strlen("HTTP/1.0 200 OK\r\n\r") - 1, -2);
        PARSE("HTTP/1.0 200 OK\r\n\r\n", strlen("HTTP/1.0 200 OK\r\n\r\n") - 1, 0);
    }

    WHEN("HTTP version is invalid") {
        PARSE("HTTP/1. 200 OK\r\n\r\n", 0, -1);
        PARSE("HTTP/1.2z 200 OK\r\n\r\n", 0, -1);
    }

    WHEN("HTTP status line is missing status code") {
        PARSE("HTTP/1.1  OK\r\n\r\n", 0, -1);
    }

    WHEN("HTTP is status line does not include message (accept)") {
        PARSE("HTTP/1.1 200\r\n\r\n", 0, 0);
        REQUIRE(resp.statusText == "");
    }

    WHEN("There is garbage after status line") {
        PARSE("HTTP/1.1 200X\r\n\r\n", 0, -1);
        PARSE("HTTP/1.1 200X \r\n\r\n", 0, -1);
        PARSE("HTTP/1.1 200X OK\r\n\r\n", 0, -1);
    }

    WHEN("There is leading and trailing spaces in header value (+)") {
        PARSE("HTTP/1.1 200 OK\r\nbar: \t b\t \t\r\n\r\n", 0, 0);
        REQUIRE(resp["bar"] == "b");
    }

    WHEN("Theres is multiple spaces between status line tokens (+)") {
        PARSE("HTTP/1.1   200   OK\r\n\r\n", 0, 0);
    }

#undef PARSE
}

TEST_CASE("Isolated header parsings")
{
    /* only test the interface; the core parser is tested by the tests above */
    using HeaderParser = IsolatedHeaderParser<std::string_view, std::string_view>;
    HeaderParser hp(std::string_view{});

#define PARSE(s, last_len, exp)                                                                                           \
    do {                                                                                                                  \
        auto status = phr_parse_headers(s, strlen(s), &hp, &HeaderParser::cbOnHeader, last_len);                          \
        REQUIRE(status ==(exp == 0 ? (int)strlen(s) : exp));                                                              \
    } while (0)

    WHEN("Parsing simple headers") {
        PARSE("Host: example.com\r\nCoREQUIREie: \r\n\r\n", 0, 0 );
        REQUIRE(hp.headers.size() == 2);
        REQUIRE(hp.headers.contains("Host"));
        REQUIRE(hp["Host"] ==  "example.com");
        REQUIRE(hp.headers.contains("CoREQUIREie"));
        REQUIRE(hp["CoREQUIREie"] ==  "");
    }

    WHEN("Parsing headers and response is complete") {
        PARSE("Host: example.com\r\nCoREQUIREie: Candy\r\n\r\n", 1, 0);
        REQUIRE(hp.headers.size() == 2);
        REQUIRE(hp.headers.contains("Host"));
        REQUIRE(hp["Host"] ==  "example.com");
        REQUIRE(hp.headers.contains("CoREQUIREie"));
        REQUIRE(hp["CoREQUIREie"] ==  "Candy");
    }

    WHEN("Parsing partial incomplete data") {
        PARSE("Host: example.com\r\nCoREQUIREie: \r\n\r", 0, -2);
        PARSE("Host: e\7fample.com\r\nCoREQUIREie: \r\n\r", 0, -1);
    }
#undef PARSE
}

static void test_chunked_at_once(
    int line,
    int consume_trailer,
    const char *encoded,
    const char *decoded,
    ssize_t expected)
{
    struct phr_chunked_decoder dec = {0};
    char *buf;
    size_t bufsz;
    ssize_t ret;

    dec.consume_trailer = consume_trailer;

    INFO("testing at-once, source at line" << line);

    buf = strdup(encoded);
    bufsz = strlen(buf);

    ret = phr_decode_chunked(&dec, buf, &bufsz);

    REQUIRE(ret == expected);
    REQUIRE(bufsz == strlen(decoded));
    REQUIRE(bufis(buf, bufsz, decoded));
    if (expected >= 0) {
        if (ret == expected)
            REQUIRE(bufis(buf + bufsz, ret, encoded + strlen(encoded) - ret));
        else
            REQUIRE(0);
    }

    free(buf);
}

static void test_chunked_per_byte(
    int line,
    int consume_trailer,
    const char *encoded,
    const char *decoded,
    ssize_t expected)
{
    struct phr_chunked_decoder dec = {0};
    auto buf = (char *) malloc(strlen(encoded) + 1);
    size_t bytes_to_consume = strlen(encoded) - (expected >= 0 ? expected : 0), bytes_ready = 0, bufsz, i;
    ssize_t ret;

    dec.consume_trailer = consume_trailer;

    INFO("testing per-byte, source at line" << line);

    for (i = 0; i < bytes_to_consume - 1; ++i) {
        buf[bytes_ready] = encoded[i];
        bufsz = 1;
        ret = phr_decode_chunked(&dec, buf + bytes_ready, &bufsz);
        if (ret != -2) {
            REQUIRE(0);
            goto cleanup;
        }
        bytes_ready += bufsz;
    }
    strcpy(buf + bytes_ready, encoded + bytes_to_consume - 1);
    bufsz = strlen(buf + bytes_ready);
    ret = phr_decode_chunked(&dec, buf + bytes_ready, &bufsz);
    REQUIRE(ret == expected);
    bytes_ready += bufsz;
    REQUIRE(bytes_ready == strlen(decoded));
    REQUIRE(bufis(buf, bytes_ready, decoded));
    if (expected >= 0) {
        if (ret == expected)
            REQUIRE(bufis(buf + bytes_ready, expected, encoded + bytes_to_consume));
        else
            REQUIRE(0);
    }

cleanup:
    free(buf);
}

static void test_chunked_failure(int line, const char *encoded, ssize_t expected)
{
    struct phr_chunked_decoder dec = {0};
    char *buf = strdup(encoded);
    size_t bufsz, i;
    ssize_t ret;

    INFO("testing failure at-once, source at line" << line);
    bufsz = strlen(buf);
    ret = phr_decode_chunked(&dec, buf, &bufsz);
    REQUIRE(ret == expected);

    INFO("testing failure per-byte, source at line" << line);
    memset(&dec, 0, sizeof(dec));
    for (i = 0; encoded[i] != '\0'; ++i) {
        buf[0] = encoded[i];
        bufsz = 1;
        ret = phr_decode_chunked(&dec, buf, &bufsz);
        if (ret == -1) {
            REQUIRE(ret == expected);
            goto cleanup;
        } else if (ret == -2) {
            /* continue */
        } else {
            REQUIRE(0);
            goto cleanup;
        }
    }
    REQUIRE(ret == expected);

cleanup:
    free(buf);
}

static void (*chunked_test_runners[])(int, int, const char *, const char *, ssize_t) =
        {test_chunked_at_once, test_chunked_per_byte, NULL};

TEST_CASE("Test chunked requests")
{
    WHEN("Decoding valid chunked requests")
    {
        size_t i;
        for (i = 0; chunked_test_runners[i] != NULL; ++i) {
            chunked_test_runners[i](__LINE__, 0, "b\r\nhello world\r\n0\r\n", "hello world", 0);
            chunked_test_runners[i](__LINE__, 0, "6\r\nhello \r\n5\r\nworld\r\n0\r\n", "hello world", 0);
            chunked_test_runners[i](__LINE__, 0, "6;comment=hi\r\nhello \r\n5\r\nworld\r\n0\r\n", "hello world", 0);
            chunked_test_runners[i](__LINE__, 0, "6\r\nhello \r\n5\r\nworld\r\n0\r\na: b\r\nc: d\r\n\r\n", "hello world",
                                    sizeof("a: b\r\nc: d\r\n\r\n") - 1);
            chunked_test_runners[i](__LINE__, 0, "b\r\nhello world\r\n0\r\n", "hello world", 0);
        }
    }

    WHEN("Decoding invalid chunked requests")
    {
        INFO("Test chunked failures");
        test_chunked_failure(__LINE__, "z\r\nabcdefg", -1);
        if (sizeof(size_t) == 8) {
            test_chunked_failure(__LINE__, "6\r\nhello \r\nffffffffffffffff\r\nabcdefg", -2);
            test_chunked_failure(__LINE__, "6\r\nhello \r\nfffffffffffffffff\r\nabcdefg", -1);
        }
    }

    SECTION("Parsing chunked requests must consume trailing \r\n")
    {
        size_t i;

        for (i = 0; chunked_test_runners[i] != NULL; ++i) {
            chunked_test_runners[i](__LINE__, 1, "b\r\nhello world\r\n0\r\n", "hello world", -2);
            chunked_test_runners[i](__LINE__, 1, "6\r\nhello \r\n5\r\nworld\r\n0\r\n", "hello world", -2);
            chunked_test_runners[i](__LINE__, 1, "6;comment=hi\r\nhello \r\n5\r\nworld\r\n0\r\n", "hello world", -2);
            chunked_test_runners[i](__LINE__, 1, "b\r\nhello world\r\n0\r\n\r\n", "hello world", 0);
            chunked_test_runners[i](__LINE__, 1, "b\nhello world\n0\n\n", "hello world", 0);
            chunked_test_runners[i](__LINE__, 1, "6\r\nhello \r\n5\r\nworld\r\n0\r\na: b\r\nc: d\r\n\r\n", "hello world", 0);
        }
    }
}
