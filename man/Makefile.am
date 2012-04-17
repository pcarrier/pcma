EXTRA_DIST = pcma.5.asciidoc pcmac.1.asciidoc pcmad.1.asciidoc
manpages = pcma.5 pcmac.1 pcmad.1
CLEANFILES = $(manpages)
man_MANS = $(manpages)

pcma.5: pcma.5.asciidoc
	@A2X@ -f manpage pcma.5.asciidoc

pcmac.1: pcmac.1.asciidoc
	@A2X@ -f manpage pcmac.1.asciidoc

pcmad.1: pcmad.1.asciidoc
	@A2X@ -f manpage pcmad.1.asciidoc
