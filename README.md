## Lhasa

Lhasa is a library for parsing LHA (.lzh) archives and a free
replacement for the Unix LHA tool.

It is possible to read from (i.e. decompress) archives; compression is
supported, but only for the -lh1- compression algorithm; future versions
may add support for other algorithms.  The aim is to be compatible with
as many different variants of the LHA file format as possible, including
LArc (.lzs) and PMarc (.pma).  A suite of archives generated from
different tools is included for regression testing. Type 'make check' to
run the tests.

The command line tool aims to be interface-compatible with the
non-free Unix LHA tool (command line syntax and output), for backwards
compatibility with tools that expect particular output.

Lhasa is licensed under the ISC license, which is a simplified version
of the MIT/X11 license that is functionally identical.
