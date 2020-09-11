all: prober.c
	$(CC) -O2 -o prober $^
asm: prober.c
	$(CC) -S -masm=intel -O2 -o prober.S $^
debug: prober.c
	$(CC) -g -o prober $^
clean: prober.c
	rm prober

