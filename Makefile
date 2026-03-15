CC      ?= cc
CFLAGS   = -Wall -Wextra -std=gnu99 -O2 -D_XOPEN_SOURCE_EXTENDED
LDFLAGS  =

SRCDIR   = src
BUILDDIR = build
TARGET   = mde

SUBDIRS  = core render features utils term
SRCS     = $(foreach d,$(SUBDIRS),$(wildcard $(SRCDIR)/$(d)/*.c))
OBJS     = $(addprefix $(BUILDDIR)/,$(notdir $(SRCS:.c=.o)))
INCLUDES = -I$(SRCDIR)/core -I$(SRCDIR)/render -I$(SRCDIR)/features \
           -I$(SRCDIR)/utils -I$(SRCDIR)/term -I$(BUILDDIR)

vpath %.c $(foreach d,$(SUBDIRS),$(SRCDIR)/$(d))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/help_md.h: docs/help.md | $(BUILDDIR)
	xxd -i $< > $@

$(BUILDDIR)/editor.o: $(BUILDDIR)/help_md.h
$(BUILDDIR)/help.o: $(BUILDDIR)/help_md.h

$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

TEST_OBJS = $(BUILDDIR)/utf8.o $(BUILDDIR)/buffer.o $(BUILDDIR)/undo.o \
            $(BUILDDIR)/render.o $(BUILDDIR)/render_table.o \
            $(BUILDDIR)/render_olist.o $(BUILDDIR)/render_ulist.o \
            $(BUILDDIR)/render_todo.o $(BUILDDIR)/render_hrule.o \
            $(BUILDDIR)/render_frontmatter.o \
            $(BUILDDIR)/render_heading.o \
            $(BUILDDIR)/links.o $(BUILDDIR)/term.o

TEST_SRCS    = $(wildcard tests/*.c)
TEST_RUNNERS = $(patsubst tests/%.c,$(BUILDDIR)/%_runner,$(TEST_SRCS))

test: $(TEST_RUNNERS)
	@for t in $(TEST_RUNNERS); do ./$$t; done

$(BUILDDIR)/%_runner: tests/%.c $(TEST_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(TEST_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(wildcard $(BUILDDIR)/*.d)
