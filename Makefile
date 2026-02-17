CC      ?= cc
CFLAGS   = -Wall -Wextra -std=gnu99 -O2 -D_XOPEN_SOURCE_EXTENDED
LDFLAGS  = $(shell pkg-config --libs ncursesw 2>/dev/null || echo "-lncurses")
CFLAGS  += $(shell pkg-config --cflags ncursesw 2>/dev/null)

SRCDIR   = src
BUILDDIR = build
TARGET   = tmde

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(wildcard $(BUILDDIR)/*.d)
