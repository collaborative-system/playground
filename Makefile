CC := gcc
MOUNT := hello
CFLAGS := -lfuse3 -Wall -Wextra -pedantic 

all: fuse-ram

.PHONY: run
run: mount

.PHONY: mount
mount: fuse-ram
	./$< $(MOUNT)

.PHONY: debug
debug: fuse-ram
	./$< -d $(MOUNT)

.PHONY: umount
umount:
	umount $(MOUNT)

.PHONY: clean
clean: fuse-ram umount
	rm -f $^

fuse-ram:
	$(CC) $(CFLAGS) -o $@ main.c 
