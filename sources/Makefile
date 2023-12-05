SUBDIRS=./libs/mpg123-1.32.3 ./libs/libre ./libs/ezxml ./libs/multipart ./whisperd

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do \
	    echo "cleaning: $$dir"; \
	    $(MAKE) -C $$dir clean; \
	done

install:
	echo "installing: whisperd"; \
	$(MAKE) -C ./whisperd install; \

.PHONY: all $(SUBDIRS)

