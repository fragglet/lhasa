lib_LTLIBRARIES=liblhasa.la
check_LIBRARIES=liblhasatest.a

HEADER_FILES =                     \
	lha_file_header.h          \
	lha_input_stream.h         \
	lha_reader.h

SRC =                                                   \
	crc16.c                 crc16.h                 \
	endian.c                endian.h                \
	ext_common.c                                    \
	ext_unix.c                                      \
	ext_header.c            ext_header.h            \
	lha_decoder.c           lha_decoder.h           \
	lha_file_header.c                               \
	lha_input_stream.c                              \
	lha_null_decoder.c      lha_null_decoder.h      \
	lha_lzhuf_decoder.c                             \
	lha_lzss_decoder.c      lha_lzss_decoder.h      \
	lha_reader.c

liblhasatest_a_CFLAGS=$(TEST_CFLAGS) -DALLOC_TESTING -I../test -g
liblhasatest_a_SOURCES=$(SRC) $(HEADER_FILES)

liblhasa_la_CFLAGS=$(MAIN_CFLAGS)
liblhasa_la_SOURCES=$(SRC) $(HEADER_FILES)

headerfilesdir=$(includedir)/liblhasa-1.0
headerfiles_HEADERS=$(HEADER_FILES)

