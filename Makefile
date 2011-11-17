CC?=gcc
PROVE?=prove

test: build_request build_response
	$(PROVE) -e '/bin/sh -c' ./test_request
	$(PROVE) -e '/bin/sh -c' ./test_response

test_request: build_request
	$(PROVE) -e '/bin/sh -c' ./test_request

test_response: build_request
	$(PROVE) -e '/bin/sh -c'./test_response

build_request: picohttpparser.o test.o
	$(CC) $(LDFLAGS) -o test_request $^

build_response: picohttpparser.o test_response.o
	$(CC) $(LDFLAGS) -o test_response $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o
	rm -f test_request
	rm -f test_response
