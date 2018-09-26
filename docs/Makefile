

INFO = ez8mon.info flashutil.info crcgen.info
PDF = ez8mon.pdf flashutil.pdf crcgen.pdf

TEXILOGFILES = *.aux *.cp *.cps *.fn *.ky *.pg *.toc *.tp *.vr *.log

%.pdf: %.texi
	texi2pdf $<

all: $(INFO) $(PDF)

clean:
	$(RM) *.pdf *.info $(TEXILOGFILES)

almostclean:
	$(RM) $(TEXILOGFILES)

