PicoHTTPParser
=============

PicoHTTPParser is a tiny, primitive, fast HTTP parser used by major Perl web application servers including Starman and Starlet.

Unlike most parsers, it is stateless and does not allocate memory by itself.
All it does is accept pointer to buffer and the output structure, and setups the pointers in the latter to point at the necessary portions of the buffer.
