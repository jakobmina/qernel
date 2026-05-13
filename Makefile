all:
	$(MAKE) -C qernel all

clean:
	$(MAKE) -C qernel clean

install:
	$(MAKE) -C qernel install

.PHONY: all clean install
