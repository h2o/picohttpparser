#
# Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase
#
# The software is licensed under either the MIT License (below) or the Perl
# license.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

CC?=gcc
PROVE?=prove

all:

test: build_request build_response
	$(PROVE) -e '/bin/sh -c' ./test_request
	$(PROVE) -e '/bin/sh -c' ./test_response

test_request: build_request
	$(PROVE) -e '/bin/sh -c' ./test_request

test_response: build_request
	$(PROVE) -e '/bin/sh -c' ./test_response

build_request: picohttpparser.c test.c
	$(CC) -Wall $(LDFLAGS) -o test_request $^

build_response: picohttpparser.c test_response.c
	$(CC) -Wall $(LDFLAGS) -o test_response $^

clean:
	rm -f test_request
	rm -f test_response

.PHONY: test
