# Builds the reference pack validator against the runtime loader that lives in source/pack/.
CC ?= cc
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wno-format-extra-args -I../../source/pack
all: test_pack
test_pack: test_pack.c ../../source/pack/dqiv_pack.c ../../source/pack/dqiv_pack.h
	$(CC) $(CFLAGS) -o $@ test_pack.c ../../source/pack/dqiv_pack.c
test: test_pack
	./test_pack $(PACK)
clean:
	rm -f test_pack
