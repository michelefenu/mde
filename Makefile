CC      ?= cc
CFLAGS   = -Wall -Wextra -std=gnu99 -O2 -D_XOPEN_SOURCE_EXTENDED
LDFLAGS  = $(shell pkg-config --libs ncursesw 2>/dev/null || echo "-lncurses")
CFLAGS  += $(shell pkg-config --cflags ncursesw 2>/dev/null)

SRCDIR   = src
BUILDDIR = build
TARGET   = mde

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/help_md.h: docs/help.md | $(BUILDDIR)
	xxd -i $< > $@

$(BUILDDIR)/editor.o: $(BUILDDIR)/help_md.h
$(BUILDDIR)/help.o: $(BUILDDIR)/help_md.h

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(BUILDDIR) -MMD -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

TEST_OBJS = $(BUILDDIR)/utf8.o $(BUILDDIR)/buffer.o $(BUILDDIR)/undo.o \
            $(BUILDDIR)/render.o $(BUILDDIR)/render_table.o

test: $(BUILDDIR)/test_runner
	./$(BUILDDIR)/test_runner

$(BUILDDIR)/test_runner: tests/test_main.c $(TEST_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) tests/test_main.c $(TEST_OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(wildcard $(BUILDDIR)/*.d)
