ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libarib25.la
libarib25_la_SOURCES = src/arib_std_b25.c src/b_cas_card.c src/multi2.c \
        src/portable.h src/ts_section_parser.c src/arib_std_b25_error_code.h \
        src/b_cas_card_error_code.h src/multi2_error_code.h src/td.c \
        src/ts_section_parser_error_code.h src/arib_std_b25.h src/b_cas_card.h \
        src/multi2.h src/ts_common_types.h src/ts_section_parser.h
libarib25_la_LIBADD = $(AM_LIBS) $(pcsclite_CFLAGS)
libarib25_la_CFLAGS = $(AM_CFLAGS) -Wall -Werror $(pcsclite_CFLAGS)

pkginclude_HEADERS = src/arib_std_b25_error_code.h src/b_cas_card_error_code.h \
        src/multi2_error_code.h src/portable.h src/ts_section_parser_error_code.h \
        src/arib_std_b25.h src/b_cas_card.h src/multi2.h src/ts_common_types.h \
        src/ts_section_parser.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = src/arib25.pc

dist_doc_DATA = README.txt
