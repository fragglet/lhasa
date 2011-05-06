
AM_CFLAGS=$(TEST_CFLAGS) -I../lib -g
LDADD=$(top_builddir)/lib/liblhasatest.a libtestframework.a

EXTRA_DIST=archives output

COMPILED_TESTS=                       \
	test-larc

TESTS=$(COMPILED_TESTS)               \
	test-list-output

check_PROGRAMS=$(COMPILED_TESTS)
check_LIBRARIES=libtestframework.a

libtestframework_a_SOURCES=     \
	crc32.c crc32.h

