lib_LTLIBRARIES=liblhasa.la
check_LIBRARIES=liblhasatest.a

HEADER_FILES =                     \
	lha_file_header.h          \
	lha_input_stream.h         \
	lha_reader.h

EXTRA_DIST =                                            \
	gencov                                          \
	bit_stream_reader.c

SRC =                                                   \
	crc16.c                 crc16.h                 \
	endian.c                endian.h                \
	ext_common.c                                    \
	ext_unix.c                                      \
	ext_header.c            ext_header.h            \
	lha_decoder.c           lha_decoder.h           \
	lha_file_header.c                               \
	lha_input_stream.c                              \
	null_decoder.c          null_decoder.h          \
	lzhuf_decoder.c                                 \
	lzss_decoder.c          lzss_decoder.h          \
	lha_reader.c                                    \
	pma_decoder.c           pma_decoder.h

liblhasatest_a_CFLAGS=$(TEST_CFLAGS) -DALLOC_TESTING -I../test -g
liblhasatest_a_SOURCES=$(SRC) $(HEADER_FILES)

liblhasa_la_CFLAGS=$(MAIN_CFLAGS)
liblhasa_la_SOURCES=$(SRC) $(HEADER_FILES)

headerfilesdir=$(includedir)/liblhasa-1.0
headerfiles_HEADERS=$(HEADER_FILES)

clean-local:
	rm -f *.gcno *.gcda *.c.gcov

