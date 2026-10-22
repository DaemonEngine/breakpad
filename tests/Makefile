.DEFAULT_GOAL := all

CXX ?= g++
CXXFLAGS += -Wall -Wextra -Werror -O0 -g
LDLIBS ?= -lm

SUFFIX ?= unknown
BASE_TARGETS := test-math
TARGETS := $(addsuffix _$(SUFFIX),$(BASE_TARGETS))

.PHONY: all clean

%_${SUFFIX}: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)
	rm -rf ${@}.dSYM

all: $(TARGETS)

clean:
	rm -fv $(TARGETS)

fullclean:
	@for target in $(BASE_TARGETS); do \
		rm -fv "$$target"_*; \
	done
