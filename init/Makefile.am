EXTRA_DIST = pcmad.lsb.in pcmad.service.in pcmad.upstart.in

edit = \
	$(SED) -r 's,@bindir\@,$(bindir),g' < $< > $@ || rm $@

noinst_DATA = pcmad.lsb pcmad.upstart

pcmad.lsb: pcmad.lsb.in
	$(edit)

pcmad.upstart: pcmad.upstart.in
	$(edit)

if HAVE_SYSTEMD
systemdsystemunit_DATA = pcmad.service

pcmad.service: pcmad.service.in
	$(edit)
endif
