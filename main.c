#include "UART_hal.h"
#include "GPIO_hal.h"
#include "time_hal.h"
#include "mini_stdio.h"
#include <stdint.h>

UART_HandleTypeDef huart0;

#define BIT(n)                  (1u << (n))

/* SMHC CMD bits */
#define SMHC_CMD_START          BIT(31)
#define SMHC_CMD_USE_HOLD       BIT(29)
#define SMHC_CMD_VOLT_SWITCH    BIT(28)
#define SMHC_CMD_UPCLK_ONLY     BIT(21)
#define SMHC_CMD_SEND_INIT_SEQ  BIT(15)
#define SMHC_CMD_WAIT_PRE       BIT(13)
#define SMHC_CMD_STOP_ABORT     BIT(12)
#define SMHC_CMD_DATA           BIT(9)
#define SMHC_CMD_CHK_CRC        BIT(8)
#define SMHC_CMD_LONG_RESP      BIT(7)
#define SMHC_CMD_RESP           BIT(6)

/* Raw interrupt bits */
#define RINT_RESP_ERR           BIT(1)
#define RINT_CC                 BIT(2)    /* command complete */
#define RINT_DTC                BIT(3)    /* data transfer complete */
#define RINT_TXDR               BIT(4)
#define RINT_RXDR               BIT(5)
#define RINT_RESP_CRC_ERR       BIT(6)
#define RINT_DATA_CRC_ERR       BIT(7)
#define RINT_RESP_TIMEOUT       BIT(8)
#define RINT_DATA_TIMEOUT       BIT(9)
#define RINT_FIFO_RUN_ERR       BIT(11)
#define RINT_HARD_WARE_LOCKED   BIT(12)
#define RINT_START_BIT_ERR      BIT(13)
#define RINT_END_BIT_ERR        BIT(15)

/* Good general error mask for command/data debug */
#define RINT_ERRORS             (RINT_RESP_ERR | RINT_RESP_CRC_ERR | RINT_DATA_CRC_ERR | \
                                 RINT_RESP_TIMEOUT | RINT_DATA_TIMEOUT | RINT_FIFO_RUN_ERR | \
                                 RINT_HARD_WARE_LOCKED | RINT_START_BIT_ERR | RINT_END_BIT_ERR)

/* SMHC CTRL bits */
#define CTRL_SOFT_RST           BIT(0)
#define CTRL_FIFO_RST           BIT(1)
#define CTRL_DMA_RST            BIT(2)
#define CTRL_FIFO_AHB           BIT(31)

/* SMHC CLKDIV bits */
#define CLKDIV_CCLK_ENB         BIT(16)

/* SMHC STATUS bits */
#define STATUS_FIFO_EMPTY       BIT(2)
#define STATUS_FIFO_FULL        BIT(3)
#define STATUS_CARD_BUSY        BIT(9)

static uint32_t rca = 1;
static uint32_t emmc_sector_mode = 1;
static uint8_t block[512];

static inline void uart_raw_putchar(uint8_t b)
{
    /*
     * Important:
     * Do NOT call uart_putchar() here.
     * uart_putchar() modifies '\n' into "\r\n".
     */
    HAL_UART_TransmitChar(&huart0, (char)b);
}

static void uart_raw_write(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        uart_raw_putchar(buf[i]);
}

static void dump_regs(const char *tag)
{
    uart_printf("%s CTRL=%x CLKDIV=%x RINT=%x STATUS=%x CTYPE=%x BYTCNT=%x\n",
                tag,
                SMHC2->SMHC_CTRL,
                SMHC2->SMHC_CLKDIV,
                SMHC2->SMHC_RINTSTS,
                SMHC2->SMHC_STATUS,
                SMHC2->SMHC_CTYPE,
                SMHC2->SMHC_BYTCNT);
}

static int smhc_wait_reset_clear(uint32_t mask)
{
    for (uint32_t t = 0; t < 1000000; t++) {
        if ((SMHC2->SMHC_CTRL & mask) == 0)
            return 0;
    }

    uart_printf("reset timeout CTRL=%x\n", SMHC2->SMHC_CTRL);
    return -1;
}

static int smhc_reset_fifo(void)
{
    SMHC2->SMHC_CTRL |= CTRL_FIFO_RST;
    return smhc_wait_reset_clear(CTRL_FIFO_RST);
}

static int smhc_reset_all(void)
{
    SMHC2->SMHC_CTRL |= CTRL_SOFT_RST | CTRL_FIFO_RST | CTRL_DMA_RST;
    return smhc_wait_reset_clear(CTRL_SOFT_RST | CTRL_FIFO_RST | CTRL_DMA_RST);
}

static int wait_cmd_done(void)
{
    for (uint32_t t = 0; t < 10000000; t++) {
        uint32_t s = SMHC2->SMHC_RINTSTS;

        if (s & RINT_ERRORS) {
            uart_printf("CMD ERR RINT=%x STATUS=%x RESP0=%x\n",
                        s, SMHC2->SMHC_STATUS, SMHC2->SMHC_RESP0);
            SMHC2->SMHC_RINTSTS = s;
            return -1;
        }

        if (s & RINT_CC) {
            SMHC2->SMHC_RINTSTS = RINT_CC;
            return 0;
        }
    }

    uart_printf("CMD timeout RINT=%x STATUS=%x RESP0=%x\n",
                SMHC2->SMHC_RINTSTS, SMHC2->SMHC_STATUS, SMHC2->SMHC_RESP0);
    return -1;
}

static int smhc_cmd(uint32_t cmd, uint32_t arg, uint32_t flags)
{
    SMHC2->SMHC_RINTSTS = 0xffffffff;
    SMHC2->SMHC_CMDARG = arg;

    SMHC2->SMHC_CMD = SMHC_CMD_START |
                      SMHC_CMD_USE_HOLD |
                      SMHC_CMD_WAIT_PRE |
                      flags |
                      cmd;

    return wait_cmd_done();
}

static int smhc_update_clock(void)
{
    SMHC2->SMHC_RINTSTS = 0xffffffff;
    SMHC2->SMHC_CMDARG = 0;

    SMHC2->SMHC_CMD = SMHC_CMD_START |
                      SMHC_CMD_USE_HOLD |
                      SMHC_CMD_WAIT_PRE |
                      SMHC_CMD_UPCLK_ONLY;

    return wait_cmd_done();
}

static void smhc2_gpio_init(void)
{
    /*
     * PC2 = SDC2_CLK
     * PC3 = SDC2_CMD
     * PC4 = SDC2_DAT2
     * PC5 = SDC2_DAT1
     * PC6 = SDC2_DAT0
     * PC7 = SDC2_DAT3
     *
     * Function 3 = SMHC2 on these pins.
     */

    /* 4 bits per pin in CFG[0]. PC2..PC7 are bits 8..31. */
    GPIOC->CFG[0] &= ~0xffffff00u;
    GPIOC->CFG[0] |=  0x33333300u;

    /*
     * 2 bits per pin in PULL[0].
     * PC2 CLK = no pull.
     * PC3 CMD + PC4..PC7 DAT = pull-up.
     */
    GPIOC->PULL[0] &= ~0x0000fff0u;
    GPIOC->PULL[0] |=  0x00005550u;

    /* Drive strength level 2 on PC2..PC7. */
    GPIOC->DRV[0] &= ~0x0000fff0u;
    GPIOC->DRV[0] |=  0x0000aaa0u;
}

static void smhc2_set_slow_clock(void)
{
    /*
     * Disable card clock, update, then enable slow clock.
     * If source is 24 MHz and divider is 30:
     * card clock roughly 24 MHz / (2 * 30) = 400 kHz.
     */

    SMHC2->SMHC_CLKDIV = 0;
    smhc_update_clock();

    SMHC2->SMHC_CLKDIV = CLKDIV_CCLK_ENB | 30;
    smhc_update_clock();
}

static void smhc2_init(void)
{
    smhc2_gpio_init();

    /*
     * Enable SMHC2 clock gate and release reset.
     * This matches your existing CCU usage:
     * bit 2  = SMHC2 gate
     * bit 18 = SMHC2 reset
     */
    CCU->SMHC_BGR_REG |= BIT(2) | BIT(18);

    /*
     * Enable SMHC2 module clock.
     * Keep your original 24 MHz-ish source/div setup.
     */
    CCU->SMHC2_CLK_REG = BIT(31) | (0 << 24) | 4;

    HAL_delayMs(10);

    smhc_reset_all();

    SMHC2->SMHC_CTRL = CTRL_FIFO_AHB;
    SMHC2->SMHC_TMOUT = 0xffffffff;
    SMHC2->SMHC_CTYPE = 0;          /* 1-bit mode first */
    SMHC2->SMHC_BLKSIZ = 512;
    SMHC2->SMHC_BYTCNT = 0;
    SMHC2->SMHC_INTMASK = 0;
    SMHC2->SMHC_RINTSTS = 0xffffffff;

    /*
     * FIFO threshold from your original code.
     * Fine for CPU polling.
     */
    SMHC2->SMHC_FIFOTH = 0x20070008;

    smhc2_set_slow_clock();

    dump_regs("after smhc2_init");
}

static int emmc_init(void)
{
    uint32_t ocr = 0;

    uart_printf("CMD0 GO_IDLE\n");

    /*
     * SEND_INIT_SEQ is important at startup.
     */
    if (smhc_cmd(0, 0, SMHC_CMD_SEND_INIT_SEQ) < 0)
        return -1;

    HAL_delayMs(5);

    uart_printf("CMD1 SEND_OP_COND loop\n");

    /*
     * CMD1 for eMMC, not ACMD41.
     * 0x40ff8080 asks for high-capacity/sector mode + voltage window.
     */
    for (uint32_t i = 0; i < 1000; i++) {
        if (smhc_cmd(1, 0x40ff8080, SMHC_CMD_RESP) == 0) {
            ocr = SMHC2->SMHC_RESP0;
            uart_printf("OCR=%x\n", ocr);

            if (ocr & BIT(31))
                break;
        }

        HAL_delayMs(10);
    }

    if (!(ocr & BIT(31))) {
        uart_printf("eMMC not ready\n");
        return -1;
    }

    emmc_sector_mode = (ocr & BIT(30)) ? 1 : 0;
    uart_printf("sector_mode=%x\n", emmc_sector_mode);

    uart_printf("CMD2 ALL_SEND_CID\n");
    if (smhc_cmd(2, 0,
                 SMHC_CMD_RESP | SMHC_CMD_LONG_RESP | SMHC_CMD_CHK_CRC) < 0)
        return -1;

    uart_printf("CID: %x %x %x %x\n",
                SMHC2->SMHC_RESP3,
                SMHC2->SMHC_RESP2,
                SMHC2->SMHC_RESP1,
                SMHC2->SMHC_RESP0);

    uart_printf("CMD3 SET_RCA\n");
    rca = 1;
    if (smhc_cmd(3, rca << 16,
                 SMHC_CMD_RESP | SMHC_CMD_CHK_CRC) < 0)
        return -1;

    uart_printf("CMD7 SELECT_CARD\n");
    if (smhc_cmd(7, rca << 16,
                 SMHC_CMD_RESP | SMHC_CMD_CHK_CRC) < 0)
        return -1;

    uart_printf("CMD16 SET_BLOCKLEN 512\n");
    if (smhc_cmd(16, 512,
                 SMHC_CMD_RESP | SMHC_CMD_CHK_CRC) < 0)
        return -1;

    uart_printf("eMMC selected OK\n");
    return 0;
}

static int emmc_read_block(uint32_t lba, uint8_t *buf)
{
    uint32_t arg = emmc_sector_mode ? lba : (lba * 512u);

    uart_printf("CMD17 read LBA=%x ARG=%x\n", lba, arg);

    SMHC2->SMHC_RINTSTS = 0xffffffff;
    smhc_reset_fifo();

    SMHC2->SMHC_BLKSIZ = 512;
    SMHC2->SMHC_BYTCNT = 512;

    SMHC2->SMHC_CMDARG = arg;
    SMHC2->SMHC_CMD = SMHC_CMD_START |
                      SMHC_CMD_USE_HOLD |
                      SMHC_CMD_WAIT_PRE |
                      SMHC_CMD_DATA |
                      SMHC_CMD_RESP |
                      SMHC_CMD_CHK_CRC |
                      17;

    if (wait_cmd_done() < 0)
        return -1;

    uint32_t got = 0;

    while (got < 512) {
        uint32_t s = SMHC2->SMHC_RINTSTS;

        if (s & RINT_ERRORS) {
            uart_printf("READ ERR RINT=%x STATUS=%x got=%x\n",
                        s, SMHC2->SMHC_STATUS, got);
            SMHC2->SMHC_RINTSTS = s;
            return -1;
        }

        /*
         * STATUS bit 2 = FIFO empty on DW-MMC style controller.
         * If not empty, read one 32-bit word.
         */
        if (!(SMHC2->SMHC_STATUS & STATUS_FIFO_EMPTY)) {
            uint32_t w = SMHC2->SMHC_FIFO;

            buf[got++] = (uint8_t)((w >> 0)  & 0xff);
            buf[got++] = (uint8_t)((w >> 8)  & 0xff);
            buf[got++] = (uint8_t)((w >> 16) & 0xff);
            buf[got++] = (uint8_t)((w >> 24) & 0xff);

            if (SMHC2->SMHC_RINTSTS & RINT_RXDR)
                SMHC2->SMHC_RINTSTS = RINT_RXDR;
        }
    }

    for (uint32_t t = 0; t < 10000000; t++) {
        uint32_t s = SMHC2->SMHC_RINTSTS;

        if (s & RINT_ERRORS) {
            uart_printf("DTC ERR RINT=%x STATUS=%x\n",
                        s, SMHC2->SMHC_STATUS);
            SMHC2->SMHC_RINTSTS = s;
            return -1;
        }

        if (s & RINT_DTC) {
            SMHC2->SMHC_RINTSTS = 0xffffffff;
            uart_printf("read done\n");
            return 0;
        }
    }

    uart_printf("DTC timeout RINT=%x STATUS=%x TCBCNT=%x TBBCNT=%x\n",
                SMHC2->SMHC_RINTSTS,
                SMHC2->SMHC_STATUS,
                SMHC2->SMHC_TCBCNT,
                SMHC2->SMHC_TBBCNT);

    return -1;
}

static void dump_bytes(uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uart_printf("%x ", buf[i]);

        if ((i & 15) == 15)
            uart_printf("\n");
    }

    uart_printf("\n");
}

static void emmc_test(void)
{
    uart_printf("\nSMHC2 eMMC dump test\n");


    uart_printf("\n--- eMMC USER AREA BLOCK 0, first 100 bytes ---\n");
    if (emmc_read_block(0x0, block) == 0)
        dump_bytes(block, 100);
    else
        uart_printf("read block 0 failed\n");

    uart_printf("\n--- eMMC USER AREA BLOCK 0x10, first 100 bytes ---\n");
    if (emmc_read_block(0x10, block) == 0)
        dump_bytes(block, 100);
    else
        uart_printf("read block 0x10 failed\n");
}

static int wait_cmd_done_quiet(void)
{
    for (uint32_t t = 0; t < 10000000; t++) {
        uint32_t s = SMHC2->SMHC_RINTSTS;

        if (s & RINT_ERRORS) {
            SMHC2->SMHC_RINTSTS = s;
            return -1;
        }

        if (s & RINT_CC) {
            SMHC2->SMHC_RINTSTS = RINT_CC;
            return 0;
        }
    }

    return -1;
}

static int emmc_read_block_silent(uint32_t lba, uint8_t *buf)
{
    uint32_t arg = emmc_sector_mode ? lba : (lba * 512u);

    SMHC2->SMHC_RINTSTS = 0xffffffff;
    smhc_reset_fifo();

    SMHC2->SMHC_BLKSIZ = 512;
    SMHC2->SMHC_BYTCNT = 512;

    SMHC2->SMHC_CMDARG = arg;
    SMHC2->SMHC_CMD = SMHC_CMD_START |
                      SMHC_CMD_USE_HOLD |
                      SMHC_CMD_WAIT_PRE |
                      SMHC_CMD_DATA |
                      SMHC_CMD_RESP |
                      SMHC_CMD_CHK_CRC |
                      17;

    if (wait_cmd_done_quiet() < 0)
        return -1;

    uint32_t got = 0;

    while (got < 512) {
        uint32_t s = SMHC2->SMHC_RINTSTS;

        if (s & RINT_ERRORS) {
            SMHC2->SMHC_RINTSTS = s;
            return -1;
        }

        if (!(SMHC2->SMHC_STATUS & STATUS_FIFO_EMPTY)) {
            uint32_t w = SMHC2->SMHC_FIFO;

            buf[got++] = (uint8_t)((w >> 0)  & 0xff);
            buf[got++] = (uint8_t)((w >> 8)  & 0xff);
            buf[got++] = (uint8_t)((w >> 16) & 0xff);
            buf[got++] = (uint8_t)((w >> 24) & 0xff);

            if (SMHC2->SMHC_RINTSTS & RINT_RXDR)
                SMHC2->SMHC_RINTSTS = RINT_RXDR;
        }
    }

    for (uint32_t t = 0; t < 10000000; t++) {
        uint32_t s = SMHC2->SMHC_RINTSTS;

        if (s & RINT_ERRORS) {
            SMHC2->SMHC_RINTSTS = s;
            return -1;
        }

        if (s & RINT_DTC) {
            SMHC2->SMHC_RINTSTS = 0xffffffff;
            return 0;
        }
    }

    return -1;
}

static int emmc_dump_raw(uint32_t start_lba, uint32_t block_count)
{
    uint32_t total_bytes = block_count * 512u;

    /*
     * Text marker before raw binary starts.
     * PC script waits for this, then reads exactly total_bytes.
     */
    uart_printf("RAW %x\n", total_bytes);

    for (uint32_t i = 0; i < block_count; i++) {
        if (emmc_read_block_silent(start_lba + i, block) < 0)
            return -1;

        uart_raw_write(block, 512);
    }

    return 0;
}

int main(void)
{
    huart0.Instance = UART0;
    huart0.Init.BaudRate = 115200;
    huart0.Init.WordLength = UART_WORDLENGTH_8B;
    huart0.Init.StopBits = UART_STOPBITS_1;
    huart0.Init.Parity = UART_PARITY_NONE;

    HAL_UART_Init(&huart0);

    uart_printf("\nStarting program...\n");

    smhc2_init();

    while (emmc_init() < 0) {
        uart_printf("eMMC init failed, retrying...\n");
        dump_regs("fail");
        HAL_delayMs(1000);
    }

    emmc_dump_raw(0x00,0x20000);

    while (1) {
        HAL_delayMs(1000);
    }
}