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
	rm -f sim test_core *.csv
