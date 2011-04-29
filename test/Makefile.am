
AM_CFLAGS=$(TEST_CFLAGS) -I../lib -g
LDADD=$(top_builddir)/lib/liblhasatest.a libtestframework.a

TESTS=                       \
	test-larc

EXTRA_DIST=larc_lz4.lzs  larc_lz5.lzs

check_PROGRAMS=$(TESTS)
check_LIBRARIES=libtestframework.a

libtestframework_a_SOURCES=     \
	crc32.c crc32.h

