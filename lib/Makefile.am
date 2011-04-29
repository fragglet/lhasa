lib_LTLIBRARIES=liblhasa.la
check_LIBRARIES=liblhasatest.a

SRC= \
	lha_decoder.h              \
	lha_file_header.h          \
	lha_input_stream.h         \
	lha_lzss_decoder.h         \
	lha_reader.h

HEADER_FILES= \
	lha_decoder.c              \
	lha_file_header.c          \
	lha_input_stream.c         \
	lha_lzss_decoder.c         \
	lha_reader.c

liblhasatest_a_CFLAGS=$(TEST_CFLAGS) -DALLOC_TESTING -I../test -g
liblhasatest_a_SOURCES=$(SRC) $(HEADER_FILES)

liblhasa_la_CFLAGS=$(MAIN_CFLAGS)
liblhasa_la_SOURCES=$(SRC) $(HEADER_FILES)

headerfilesdir=$(includedir)/liblhasa-1.0
headerfiles_HEADERS=$(HEADER_FILES)

