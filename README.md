# Building and Running Bare-Metal Code on Allwinner F133

This guide shows how to build and run a bare-metal application on the Allwinner F133 using Ubuntu and FEL mode.

---

## 1. Install the Required Tools

Update your package list and install the RISC-V toolchain:

```bash
sudo apt update

sudo apt install \
    make \
    gcc-riscv64-unknown-elf \
    binutils-riscv64-unknown-elf \
    gdb-multiarch
```

Verify that the compiler is installed:

```bash
riscv64-unknown-elf-gcc --version
```

---

## 2. Recommended Compiler Flags

### General Purpose

Suitable for most RV64 targets:

```text
-march=rv64gc
-mabi=lp64d
-mcmodel=medany
-ffreestanding
-nostdlib
```

### Bare-Metal GPIO/UART Examples

For simple bare-metal applications, I recommend:

```text
-march=rv64imac
-mabi=lp64
-mcmodel=medany
-ffreestanding
-nostdlib
```

These options generate smaller code and avoid floating-point instructions, making them ideal for early bring-up and hardware testing.

---

## 3. Build the Project

Compile the firmware:

```bash
make
```

After a successful build, a binary such as `gpio.bin` will be generated.

---

## 4. Install FEL Utilities

Install the Allwinner FEL tools:

```bash
sudo apt install sunxi-tools
```

---

## 5. Enter FEL Mode

Reboot the board into **FEL (eFEL) mode**.

---

## 6. Verify the Connection

Make sure Ubuntu can communicate with the chip:

```bash
sudo sunxi-fel version
```

If the connection is successful, `sunxi-fel` will display information about the target device.

---

## 7. Upload the Firmware

Write the binary into the F133 SRAM:

```bash
sudo sunxi-fel write 0x00024000 gpio.bin
```

---

## 8. Execute the Firmware

Start execution from SRAM:

```bash
sudo sunxi-fel exec 0x00024000
```

Your bare-metal application is now running directly on the F133.
