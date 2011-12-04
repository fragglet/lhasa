lib_LTLIBRARIES=liblhasa.la
check_LIBRARIES=liblhasatest.a

HEADER_FILES =                     \
	lha_file_header.h          \
	lha_input_stream.h         \
	lha_reader.h

EXTRA_DIST =                                            \
	bit_stream_reader.c                             \
	lh_new_decoder.c                                \
	tree_decode.c

SRC =                                                   \
	crc16.c                 crc16.h                 \
	ext_common.c                                    \
	ext_unix.c                                      \
	ext_header.c            ext_header.h            \
	lha_decoder.c           lha_decoder.h           \
	lha_endian.c            lha_endian.h            \
	lha_file_header.c                               \
	lha_input_stream.c                              \
	lha_basic_reader.c      lha_basic_reader.h      \
	lha_reader.c                                    \
	null_decoder.c          null_decoder.h          \
	lh1_decoder.c           lh1_decoder.h           \
	lh5_decoder.c                                   \
	lz5_decoder.c           lz5_decoder.h           \
	lzs_decoder.c           lzs_decoder.h           \
	pm2_decoder.c           pm2_decoder.h

liblhasatest_a_CFLAGS=$(TEST_CFLAGS) -DALLOC_TESTING -I../test -g
liblhasatest_a_SOURCES=$(SRC) $(HEADER_FILES)

liblhasa_la_CFLAGS=$(MAIN_CFLAGS)
liblhasa_la_SOURCES=$(SRC) $(HEADER_FILES)

headerfilesdir=$(includedir)/liblhasa-1.0
headerfiles_HEADERS=$(HEADER_FILES)

clean-local:
	rm -f *.gcno *.gcda *.c.gcov

