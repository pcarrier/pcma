EXTRA_DIST = pcma.5.asciidoc pcmac.1.asciidoc pcmad.1.asciidoc
manpages = pcma.5 pcmac.1 pcmad.1
CLEANFILES = $(manpages)
man_MANS = $(manpages)

pcma.5: pcma.5.asciidoc
	a2x -Lf manpage pcma.5.asciidoc

pcmac.1: pcmac.1.asciidoc
	a2x -Lf manpage pcmac.1.asciidoc

pcmad.1: pcmad.1.asciidoc
	a2x -Lf manpage pcmad.1.asciidoc
