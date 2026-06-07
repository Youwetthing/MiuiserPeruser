# MiuiserPeruser Daemon Suite — Makefile
# Drop this in ~/MiuiserPeruser/src/daemon/

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE
CFLAGS  += -Wmissing-braces -Wmissing-declarations -Wstrict-prototypes
CFLAGS  += -I. -I../include -I../../include -I../gaveld
LDFLAGS := -lpthread -lrt

SRCDIR  := .
BINDIR  := ../../bin

DAEMONS := rahzerd granitord shredderd ratkingd ram_slammer kernel_sanders stalkerslayer

.PHONY: all clean check dirs $(DAEMONS)

dirs:
	mkdir -p $(BINDIR)

all: dirs $(DAEMONS)

check:
	@echo "=== Syntax-only check for all daemons ==="
	@for f in $(SRCDIR)/*.c; do \
		if [ -f "$$f" ]; then \
			echo "Checking $$f..."; \
			$(CC) $(CFLAGS) -fsyntax-only $$f || exit 1; \
		fi \
	done
	@echo "All clean."

clean:
	@rm -f $(BINDIR)/rahzerd $(BINDIR)/granitord $(BINDIR)/shredderd \
	      $(BINDIR)/ratkingd $(BINDIR)/ram_slammer $(BINDIR)/kernel_sanders \
	      $(BINDIR)/stalkerslayer

$(BINDIR)/rahzerd: $(SRCDIR)/rahzerd.c $(SRCDIR)/rahzerd.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/granitord: $(SRCDIR)/granitord.c $(SRCDIR)/granitord.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/shredderd: $(SRCDIR)/shredderd.c $(SRCDIR)/shredderd.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/ratkingd: $(SRCDIR)/ratkingd.c $(SRCDIR)/ratkingd.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/ram_slammer: $(SRCDIR)/ram_slammer.c $(SRCDIR)/ram_slammer.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/kernel_sanders: $(SRCDIR)/kernel_sanders.c $(SRCDIR)/kernel_sanders.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/stalkerslayer: $(SRCDIR)/stalkerslayer.c $(SRCDIR)/stalkerslayer.h | dirs
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
