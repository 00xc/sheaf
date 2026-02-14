ARCH ?= $(shell uname -i)
LLVM ?= 0
DEBUG ?= 0
ASAN ?= 0
V ?= 0

ifeq ($(V),0)
Q := @
else
Q :=
endif

ifeq ($(LLVM),0)
CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
LD = $(CROSS_COMPILE)ld
else
CC = clang
AR = llvm-ar
LD = ld.lld
endif

ifneq ($(wildcard Makefile.$(ARCH)),)
include Makefile.$(ARCH)
endif

ALL_CFLAGS := -Wall -Wextra -Wpedantic -Werror -O2 -std=c11 -Iinclude/ -fPIC
ALL_CFLAGS += $(CFLAGS_ARCH) $(CFLAGS)

ifneq ($(DEBUG),0)
ALL_CFLAGS += -ggdb -DDEBUG
ALL_LDFLAGS += -ggdb
endif

ifneq ($(ASAN),0)
ALL_CFLAGS += -fsanitize=address,undefined
ALL_LDFLAGS += -fsanitize=address,undefined -static-libasan
endif

TEST_CFLAGS := $(ALL_CFLAGS) -Itest/ $(TEST_CFLAGS)
TEST_LDFLAGS := -latomic

ifeq ($(LLVM),1)
include Makefile.clang
endif

SRCS           := $(wildcard src/*.c)
OBJS           := $(SRCS:.c=.o)
OBJS_DEPS      := $(OBJS:.o=.d)

TEST_SRCS      := $(wildcard tests/test_*.c)
TEST_OBJS      := $(TEST_SRCS:.c=.o)
TEST_OBJS_DEPS := $(TEST_OBJS:.o=.d)
TESTS          := $(TEST_OBJS:.o=)
RUN_TESTS      := $(addprefix run-,$(TESTS))

STATIC := libsheaf.a
SHARED := libsheaf.so

.PHONY: all clean fmt fmt-check tests run-tests

all: $(SHARED) $(STATIC)

$(STATIC): $(OBJS)
	$(info AR      $@)
	$(Q)$(AR) rcs $@ $^

$(SHARED): $(OBJS)
	$(info LD      $@)
	$(Q)$(LD) $(ALL_LDFLAGS) -shared -o $@ $^

%.o: %.c
	$(info CC      $@)
	$(Q)$(CC) $(ALL_CFLAGS) -MMD -MP -c -o $@ $<

-include $(OBJS_DEPS)

tests/test_%.o: tests/test_%.c
	$(info CC-TEST $@)
	$(Q)$(CC) $(TEST_CFLAGS) -MMD -MP -c -o $@ $<

-include $(TEST_OBJS_DEPS)

tests/test_%: tests/test_%.o $(STATIC)
	$(info LD-TEST $@)
	$(Q)$(CC) $(TEST_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

tests: $(TESTS)

run-tests/%: tests/%
	$(Q)./$< >/dev/null && \
		echo "TEST    $< OK" || \
		{ echo "TEST    $< FAIL"; exit 1; }

run-tests: $(RUN_TESTS)

fmt:
	$(Q)find src/ tests/ -name "*.c" | xargs -I{} clang-format -i {}
	$(Q)find include/ -name "*.h" | xargs -I{} clang-format -i {}

fmt-check:
	$(Q)find src/ tests/ -name "*.c" | xargs -I{} clang-format --dry-run --Werror {}
	$(Q)find include/ -name "*.h" | xargs -I{} clang-format --dry-run --Werror {}

clean:
	rm -f $(OBJS)
	rm -f $(OBJS_DEPS)
	rm -f $(TEST_OBJS)
	rm -f $(TEST_OBJS_DEPS)
	rm -f $(TESTS)
	rm -f $(STATIC) $(SHARED)
