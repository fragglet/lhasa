include ../config.make

TOPLEVEL=../..
PANDOC_FLAGS = -s --template=default.html5 -H ../style.html

EXE_FILES=$(TOPLEVEL)/src/lha.exe

ZIP=$(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-win32.zip

$(ZIP): staging
	zip -j -r $@ staging/

staging: $(EXE_FILES) $(patsubst %,$(TOPLEVEL)/%,$(DOC_FILES))
	rm -rf staging
	mkdir staging
	cp $(EXE_FILES) staging/
	$(STRIP) staging/*.exe
	for f in $(DOC_FILES); do                      \
	    cp $(TOPLEVEL)/$$f staging/$$f.txt;        \
	    unix2dos staging/$$f.txt;                  \
	done
	pandoc $(PANDOC_FLAGS) -f man $(TOPLEVEL)/doc/lha.1 \
	       -o staging/lha.html

clean:
	rm -rf $(ZIP)
	rm -rf staging
