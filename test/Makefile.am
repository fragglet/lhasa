
AM_CFLAGS=$(TEST_CFLAGS) -I../lib -g
LDADD=$(top_builddir)/lib/liblhasatest.a

TESTS=                       \
	test-larc

EXTRA_DIST=larc_lz4.lzs  larc_lz5.lzs

check_PROGRAMS=$(TESTS)

