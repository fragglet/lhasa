
AM_CFLAGS=$(TEST_CFLAGS) -I../lib -g
LDADD=$(top_builddir)/lib/liblhasatest.a libtestframework.a

ARCHIVES=                                  \
  archives/larc333/lz4.lzs                 \
  archives/larc333/lz5.lzs                 \
  archives/larc333/sfx.com                 \
  archives/lharc113/lh0.lzh                \
  archives/lharc113/lh1.lzh                \
  archives/lharc113/sfx.com                \
  archives/lha213/lh0.lzh                  \
  archives/lha213/lh5.lzh                  \
  archives/lha213/sfx.exe                  \
  archives/lha255e/lh0.lzh                 \
  archives/lha255e/lh5.lzh                 \
  archives/lha255e/sfx.exe                 \
  archives/lha_unix114i/lh0.lzh            \
  archives/lha_unix114i/lh5.lzh            \
  archives/lha_unix114i/lh6.lzh            \
  archives/lha_unix114i/lh7.lzh

EXTRA_DIST=$(ARCHIVES)

TESTS=                       \
	test-larc

check_PROGRAMS=$(TESTS)
check_LIBRARIES=libtestframework.a

libtestframework_a_SOURCES=     \
	crc32.c crc32.h

