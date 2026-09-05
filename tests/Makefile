# Unit tests for MiuiserPeruser.
#
#   make              build + run every test binary
#   make COVERAGE=1   same, instrumented for gcov (see ./run_tests.sh --coverage)
#   make clean
#
# SQLITE=system links libsqlite3 instead of the bundled amalgamation, which is
# a lot faster to build; the amalgamation is what the production gaveld links.

ROOT      := $(abspath $(CURDIR)/..)
BUILD     := $(CURDIR)/build
BIN       := $(BUILD)/bin
OBJ       := $(BUILD)/obj
TEST_ROOT := $(BUILD)/root
GAVELD    := $(ROOT)/src/gaveld

CC      ?= cc
CFLAGS  ?= -std=c11 -g -O0 -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -D_GNU_SOURCE -DBASE_DIR='"$(TEST_ROOT)"'
CFLAGS  += -I$(CURDIR) -I$(GAVELD)
LDLIBS  += -lpthread -lm

SQLITE ?= bundled
ifeq ($(SQLITE),system)
  LDLIBS += -lsqlite3
  SQLITE_OBJ :=
else
  CFLAGS += -I$(ROOT)/src/core/sqlite
  SQLITE_OBJ := $(BUILD)/sqlite3.o
endif

COVERAGE ?= 0
ifeq ($(COVERAGE),1)
  CFLAGS  += --coverage
  LDFLAGS += --coverage
endif

# test name -> gaveld sources linked into it
# (test_ingest #includes ingest.c directly to reach its static parser)
SRC_test_weights   := weights.c
SRC_test_mitre_map := mitre_map.c
SRC_test_log       := log.c
SRC_test_tier      := tier.c log.c
SRC_test_ingest    := log.c
SRC_test_db        := db.c log.c
SRC_test_cases     := cases.c db.c log.c
SRC_test_scorer    := scorer.c db.c log.c tier.c weights.c

TESTS := test_weights test_mitre_map test_log test_tier test_ingest \
         test_db test_cases test_scorer

BINS := $(addprefix $(BIN)/,$(TESTS))

.PHONY: all build run clean
all: run

build: $(BINS)

run: build
	@fail=0; \
	for t in $(TESTS); do \
	    echo "== $$t"; \
	    $(BIN)/$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; fi; \
	echo "ALL TESTS PASSED"

$(BUILD)/sqlite3.o:
	@mkdir -p $(BUILD)
	$(CC) -std=c11 -O1 -w -DSQLITE_THREADSAFE=1 \
	      -I$(ROOT)/src/core/sqlite -c $(ROOT)/src/core/sqlite/sqlite3.c -o $@

# Per-test object directory keeps the gcov data files separated by binary.
define TEST_RULES
$(OBJ)/$(1)/%.o: $(GAVELD)/%.c
	@mkdir -p $$(dir $$@) $(TEST_ROOT)
	$(CC) $(CFLAGS) -c $$< -o $$@

$(OBJ)/$(1)/main.o: $(CURDIR)/gaveld/$(1).c
	@mkdir -p $$(dir $$@) $(TEST_ROOT)
	$(CC) $(CFLAGS) -c $$< -o $$@

$(BIN)/$(1): $(OBJ)/$(1)/main.o \
             $(addprefix $(OBJ)/$(1)/,$(SRC_$(1):.c=.o)) $(SQLITE_OBJ)
	@mkdir -p $(BIN)
	$(CC) $(LDFLAGS) -o $$@ $$^ $(LDLIBS)
endef

$(foreach t,$(TESTS),$(eval $(call TEST_RULES,$(t))))

clean:
	rm -rf $(BUILD)
