DIRS=Crypto Logger Utils Sort Cps

all:
	for i in $(DIRS); do \
		$(MAKE) -C $$i || exit 1; \
	done

clean:
	for i in $(DIRS); do \
		$(MAKE) -C $$i clean || exit 1; \
	done
	rm -rf ./release


