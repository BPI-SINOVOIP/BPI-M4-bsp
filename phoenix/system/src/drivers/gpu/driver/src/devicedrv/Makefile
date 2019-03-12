.PHONY: all install build clean

all: install

install: build

build:
	$(MAKE) -C mali \
		BUILD=release USING_PROFILING=0 USING_DT=1 USING_DVFS=1 MALI_DMA_BUF_MAP_ON_ATTACH=0 \
		USING_DEVFREQ=1 MALI_PLATFORM=hercules

clean:
	$(MAKE) -C mali clean

