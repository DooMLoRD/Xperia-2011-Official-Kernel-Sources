
AM_MAKEFLAGS = --no-print-directory

parser_sources =  parser/parser.h parser/parser.c \
					parser/lmp.c \
					parser/hci.c \
					parser/l2cap.c \
					parser/att.c \
					parser/sdp.h parser/sdp.c \
					parser/rfcomm.h parser/rfcomm.c \
					parser/bnep.c \
					parser/cmtp.c \
					parser/hidp.c \
					parser/hcrp.c \
					parser/avdtp.c \
					parser/avctp.c \
					parser/obex.c \
					parser/capi.c \
					parser/ppp.c \
					parser/tcpip.c \
					parser/ericsson.c \
					parser/csr.c \
					parser/bpa.c


sbin_PROGRAMS = src/hcidump

src_hcidump_SOURCES = src/hcidump.c $(parser_sources)
src_hcidump_LDADD = @BLUEZ_LIBS@


noinst_PROGRAMS = src/bpasniff src/csrsniff

src_bpasniff_SOURCES = src/bpasniff.c $(parser_sources)
src_bpasniff_LDADD = @BLUEZ_LIBS@

src_csrsniff_SOURCES = src/csrsniff.c $(parser_sources)
src_csrsniff_LDADD = @BLUEZ_LIBS@


AM_CFLAGS = @BLUEZ_CFLAGS@

dist_man_MANS = src/hcidump.8

EXTRA_DIST = src/magic.btsnoop

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 configure config.h.in \
	depcomp missing install-sh mkinstalldirs
