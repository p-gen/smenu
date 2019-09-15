bin_PROGRAMS = smenu
smenu_SOURCES = smenu.c smenu.h list.c list.h xmalloc.c xmalloc.h \
                index.c index.h utf8.c utf8.h fgetc.c fgetc.h     \
		utils.c utils.h usage.c usage.h ctxopt.h ctxopt.c
dist_man_MANS = smenu.1
EXTRA_DIST = smenu.spec.in smenu.spec ChangeLog build.sh version  \
	COPYRIGHT LICENSE.rst README.rst examples build-aux tests \
	FAQ

dist-hook:
	@chmod u+rw $(distdir)/tests;              \
	cd $(distdir)/tests ;                      \
	find * -name 'data*' -o                    \
	       -name '*.in'  -o                    \
	       -name '*.tst' -o                    \
	       -name '*.bl'  -o                    \
	       -name '*.good'                      \
	| cpio -o > tests.cpio 2>/dev/null
	@gzip -f9 $(distdir)/tests/tests.cpio
	@find $(distdir)/tests/* -type d           \
                                 -exec rm -rf {} +
