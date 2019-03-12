TOP ?= ..

DIRS :=
DIRS += gpu
ifneq ($(ENABLE_VMX_HARDENING), YES)
DIRS += gator
endif
DIRS += wifi
DIRS += bluetooth

.PHONY: all clean $(DIRS)

all: $(DIRS)

$(DIRS):
	@if test -d $@; then cd $@; $(MAKE) all; fi

clean:
	@for dir in $(DIRS); do \
		if test -d $$dir; then \
			if (cd $$dir; $(MAKE) $@); then true; else exit 1; fi; \
		fi \
	done;

install:
	@test -e ../bin || mkdir ../bin
	@for dir in $(DIRS); do \
		if test -d $$dir; then \
			if (cd $$dir; $(MAKE) $@); then true; else exit 1; fi; \
		fi \
	done;
