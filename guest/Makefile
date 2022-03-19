guest.bin: guest.S
	gcc -nostdinc -c guest.S -o guest.o
	ld -Ttext=0x00 -nostdlib -static guest.o -o guest.elf
	objcopy -O binary guest.elf guest.bin
	xxd -i guest.bin > ../module/guest.h

.PHONY: clean

clean:
	rm -rf guest.o guest.elf guest.bin
