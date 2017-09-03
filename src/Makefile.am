bin_PROGRAMS = gmediarender

gmediarender_SOURCES = main.c git-version.h \
	upnp.c upnp_control.c upnp_connmgr.c  upnp_transport.c \
	upnp.h upnp_control.h upnp_connmgr.h  upnp_transport.h \
	variable-container.h variable-container.c \
	upnp_device.c upnp_device.h \
	upnp_renderer.h upnp_renderer.c \
	webserver.c webserver.h \
	output.c output.h \
	logging.h logging.c \
	xmldoc.c xmldoc.h \
	xmlescape.c xmlescape.h \
	output_silan.c output_silan.h swa_msg_api.h

main.c : git-version.h

git-version.h: .FORCE
	$(AM_V_GEN)(echo "#define GM_COMPILE_VERSION \"$(shell git log -n1 --date=short --format='%cd_%h' 2>/dev/null || echo -n '0.0.7-silan')\"" > $@-new; \
	cmp -s $@ $@-new || cp $@-new $@; \
	rm $@-new)

.FORCE:

AM_CPPFLAGS = $(LIBUPNP_CFLAGS) -DPKG_DATADIR=\"$(datadir)/gmediarender\"
gmediarender_LDADD = $(LIBUPNP_LIBS)
