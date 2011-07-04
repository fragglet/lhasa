
bin_PROGRAMS=lha

lha_SOURCES =                             \
	main.c                            \
	list.c        list.h              \
	extract.c     extract.h

lha_CFLAGS=$(MAIN_CFLAGS) -I$(top_builddir)/lib
lha_LDADD=$(top_builddir)/lib/liblhasa.la

