CORE = ../stm32/lib/core
CFLAGS = -O2 -Wall -Wextra -I$(CORE)
all: sim test_core
sim: sim.c $(CORE)/vfd_core.c
	$(CC) $(CFLAGS) -o $@ $^ -lm
test_core: test_core.c $(CORE)/params.c $(CORE)/modbus.c $(CORE)/vfd_core.c $(CORE)/app.c $(CORE)/ui.c $(CORE)/lcd.c
	$(CC) $(CFLAGS) -o $@ $^ -lm
test: all
	./sim && ./test_core
clean:
	rm -f sim test_core dump_params *.csv glyphs.txt
dump_params: dump_params.c $(CORE)/params.c $(CORE)/app.c $(CORE)/ui.c $(CORE)/lcd.c $(CORE)/modbus.c $(CORE)/vfd_core.c hw_stub.c
	$(CC) $(CFLAGS) -o $@ $^ -lm
docs: dump_params
	./dump_params > ../../docs/parameters.md
fuzz: fuzz_modbus.c $(CORE)/modbus.c $(CORE)/params.c $(CORE)/vfd_core.c
	$(CC) -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -I$(CORE) -o $@ $^ -lm && ./fuzz
