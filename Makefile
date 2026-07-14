CROSS = riscv64-unknown-elf

CFLAGS = -march=rv64imac -mabi=lp64 -mcmodel=medany \
         -ffreestanding -nostartfiles -O0 -Wall -nostdlib

LDFLAGS = -T linker.ld

PROJECT = emmc

SRCS = startup.s main.c 
 
all: $(PROJECT).bin

$(PROJECT).elf: $(SRCS) linker.ld
	$(CROSS)-gcc $(CFLAGS) $(SRCS) $(LDFLAGS) -o $(PROJECT).elf

$(PROJECT).bin: $(PROJECT).elf
	$(CROSS)-objcopy -O binary $(PROJECT).elf $(PROJECT).bin

clean:
	rm -f $(PROJECT).elf $(PROJECT).bin