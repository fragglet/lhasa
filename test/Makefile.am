
AM_CFLAGS=$(TEST_CFLAGS) -I../lib -g
LDADD=$(top_builddir)/lib/liblhasatest.a libtestframework.a

COMPILED_TESTS=                       \
	test-decompress

UNCOMPILED_TESTS=                     \
	test-list-output              \
	test-crc-output

TESTS=$(COMPILED_TESTS) $(UNCOMPILED_TESTS)
EXTRA_DIST=archives output $(UNCOMPILED_TESTS)

check_PROGRAMS=$(COMPILED_TESTS)
check_LIBRARIES=libtestframework.a

libtestframework_a_SOURCES=     \
	crc32.c crc32.h

