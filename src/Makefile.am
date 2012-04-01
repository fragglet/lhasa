
bin_PROGRAMS=lha
check_PROGRAMS=test-lha

SOURCES =                                 \
	main.c                            \
	              options.h           \
	filter.c      filter.h            \
	list.c        list.h              \
	extract.c     extract.h

lha_SOURCES=$(SOURCES)
lha_CFLAGS=$(MAIN_CFLAGS) -I$(top_builddir)/lib
lha_LDADD=$(top_builddir)/lib/liblhasa.la

test_lha_SOURCES=$(SOURCES)
test_lha_CFLAGS=$(TEST_CFLAGS) -I$(top_builddir)/lib
test_lha_LDADD=$(top_builddir)/lib/liblhasatest.a

clean-local:
	rm -f *.gcno *.gcda *.c.gcov

