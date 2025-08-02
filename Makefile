SUBDIRS := $(wildcard */)

EXCLUDE := venv/

SUBDIRS := $(filter-out $(EXCLUDE), $(SUBDIRS))

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
