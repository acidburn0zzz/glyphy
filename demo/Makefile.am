NULL =
SUBDIRS =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
BUILT_SOURCES =
noinst_PROGRAMS =

BUILT_SOURCES += default-text.h default-font.h
EXTRA_DIST += default-text.txt default-font.ttf
default-text.h: default-text.txt $(top_srcdir)/src/stringize
	$(AM_V_GEN) $(top_srcdir)/src/stringize "static const char default_text[]" $< > $@
default-font.h: default-font.ttf
	$(AM_V_GEN) { \
		echo "static const unsigned char default_font[] = {"; \
		hexdump -v -e '"x" 1/1 "%02X" " "' < $< | fmt | sed 's/ *x/\\x/g;s/^/"/;s/$$/"/'; \
		echo '};'; \
	} > $@

if HAVE_HARFBUZZ
if HAVE_GL
if HAVE_GLUT

if ANDROID
SUBDIRS += android
else
noinst_PROGRAMS += glyphy-demo
glyphy_demo_CPPFLAGS = \
	-I $(top_srcdir)/src \
	$(HARFBUZZ_CFLAGS) \
	$(GL_CFLAGS) \
	$(GLEW_CFLAGS) \
	$(GLUT_CFLAGS) \
	$(NULL)
glyphy_demo_LDADD = \
	$(top_builddir)/src/libglyphy.la \
	-lm \
	$(HARFBUZZ_LIBS) \
	$(GL_LIBS) \
	$(GLEW_LIBS) \
	$(GLUT_LIBS) \
	$(NULL)
glyphy_demo_SOURCES = \
	default-font.h \
	default-text.h \
	demo-atlas.h \
	demo-atlas.cc \
	demo-buffer.h \
	demo-buffer.cc \
	demo-common.h \
	demo-font.h \
	demo-font.cc \
	demo-glstate.h \
	demo-glstate.cc \
	demo-shader.h \
	demo-shader.cc \
	demo-view.h \
	demo-view.cc \
	glyphy-demo.cc \
	matrix4x4.h \
	matrix4x4.c \
	trackball.h \
	trackball.c \
	$(SHADERHEADERS) \
	$(NULL)
endif
SHADERS = \
	demo-atlas.glsl \
	demo-fshader.glsl \
	demo-vshader.glsl \
	$(NULL)
SHADERHEADERS = $(patsubst %.glsl,%-glsl.h, $(SHADERS))
BUILT_SOURCES += $(SHADERHEADERS)
EXTRA_DIST += $(SHADERS)

%-glsl.h: %.glsl $(top_srcdir)/src/stringize
	$(AM_V_GEN) $(top_srcdir)/src/stringize "static const char *`echo "$<" | \
	sed 's@.*/@@;s/[-.]/_/g'`" < "$<" > "$@.tmp" && \
	mv "$@.tmp" "$@" || ($(RM) "$@.tmp"; false)

endif
endif
endif


if HAVE_FREETYPE2

noinst_PROGRAMS += glyphy-validate
glyphy_validate_CPPFLAGS = \
	-I $(top_srcdir)/src \
	$(FREETYPE2_CFLAGS) \
	$(NULL)
glyphy_validate_LDADD = \
	$(top_builddir)/src/libglyphy.la \
	$(FREETYPE2_LIBS) \
	$(NULL)
glyphy_validate_SOURCES = \
	glyphy-validate.cc \
	$(NULL)

endif


-include $(top_srcdir)/git.mk
