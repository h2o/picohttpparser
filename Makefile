#
# Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase,
#                         Shigeo Mitsunari
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
TEST_CFLAGS=$(CFLAGS) -Wall -fsanitize=address,undefined
TEST_ENV="UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"

all:

test: test-bin
	env $(TEST_ENV) $(PROVE) -v ./test-bin

test-bin: picohttpparser.c picotest/picotest.c test.c
	$(CC) $(TEST_CFLAGS) $(LDFLAGS) -o $@ $^

bench: bench.c picohttpparser.c

clean:
	rm -f bench test-bin

.PHONY: all clean test
