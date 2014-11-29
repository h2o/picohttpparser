PicoHTTPParser
=============

Copyright (c) 2009-2014 [Kazuho Oku](https://github.com/kazuho), [Tokuhiro Matsuno](https://github.com/tokuhirom), [Daisuke Murase](https://github.com/typester), [Shigeo Mitsunari](https://github.com/herumi)

PicoHTTPParser is a tiny, primitive, fast HTTP request/response parser.

Unlike most parsers, it is stateless and does not allocate memory by itself.
All it does is accept pointer to buffer and the output structure, and setups the pointers in the latter to point at the necessary portions of the buffer.

The code is widely deployed within Perl applications through popular modules that use it, including [Plack](https://metacpan.org/pod/Plack), [Starman](https://metacpan.org/pod/Starman), [Starlet](https://metacpan.org/pod/Starlet), [Furl](https://metacpan.org/pod/Furl).  It is also the HTTP/1 parser of [H2O](https://github.com/h2o/h2o).

Check out [test.c] to find out how to use the parser.

The software is dual-licensed under the Perl License or the MIT License.

Benchmark
---------

![benchmark results](http://i.gyazo.com/a85c18d3162dfb46b485bb41e0ad443a.png)

The benchmark code is from [fukamachi/fast-http@6b91103](https://github.com/fukamachi/fast-http/tree/6b9110347c7a3407310c08979aefd65078518478).

The internals of picohttpparser has been described to some extent in [my blog entry]( http://blog.kazuhooku.com/2014/11/the-internals-h2o-or-how-to-write-fast.html).
