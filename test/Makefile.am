
AM_CFLAGS=$(TEST_CFLAGS) -I../lib -g
LDADD=$(top_builddir)/lib/liblhasatest.a libtestframework.a

COMPILED_TESTS=                       \
	test-crc16                    \
	test-decoder                  \
	test-decompress

UNCOMPILED_TESTS=                     \
	test-list-output              \
	test-crc-output

TESTS=$(COMPILED_TESTS) $(UNCOMPILED_TESTS)
EXTRA_DIST=archives compressed output $(UNCOMPILED_TESTS)

check_PROGRAMS=$(COMPILED_TESTS)
check_LIBRARIES=libtestframework.a

libtestframework_a_SOURCES=     \
	crc32.c crc32.h

clean-local:
	rm -f *.gcno *.gcda *.c.gcov

if BUILD_COVERAGE

# Delete .gcda files before running tests.

check-am: check-prerun

check-prerun:
	@rm -f $(top_builddir)/lib/*.gcda  \
	       $(top_builddir)/src/*.gcda

# Display coverage summary after running tests.

check: check-postrun

check-postrun:
	@cd $(top_builddir); ./gencov lib/*.c src/*.c

endif

