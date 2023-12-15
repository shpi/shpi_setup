/*
This software needs wiringPi to compile!

compiling:
   gcc yaccf.c -o yaccf -lwiringPi

Mai 2019 ... Kianusch Sayah Karadji <kianusch@gmail.com>

*** Based on: CCLoader/Arduino.c ...

Copyright (c) 2012-2014 RedBearLab

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*** and flash_cc2531/cc_write.c

Copyright (c) 2019 Jean Michault.

modified for SHPI


*/

// INCLUDES
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>

#include <getopt.h>

#include <wiringPi.h>

// DEFINES
#define RESET 14
#define DC 12
#define DD 2

#define DELAY 2000

// Addresses
#define ADDR_BUF0 0x0000       // Buffer
#define ADDR_DMA_DESC_0 0x0200 // DMA descriptors (8 bytes)
#define ADDR_DMA_DESC_1 (ADDR_DMA_DESC_0 + 8)

// DMA channels
#define CH_DBG_TO_BUF0 0x01   // Channel 0
#define CH_BUF0_TO_FLASH 0x02 // Channel 1

// Commands
#define CMD_CHIP_ERASE 0x10
#define CMD_WR_CONFIG 0x19
#define CMD_RD_CONFIG 0x24
#define CMD_READ_STATUS 0x30
#define CMD_RESUME 0x4C
#define CMD_DEBUG_INSTR_1B (0x54 | 1)
#define CMD_DEBUG_INSTR_2B (0x54 | 2)
#define CMD_DEBUG_INSTR_3B (0x54 | 3)
#define CMD_BURST_WRITE 0x80
#define CMD_GET_CHIP_ID 0x68

// Status bitmasks
#define STATUS_CHIP_ERASE_BUSY_BM 0x80 // New debug interface
#define STATUS_PCON_IDLE_BM 0x40
#define STATUS_CPU_HALTED_BM 0x20
#define STATUS_PM_ACTIVE_BM 0x10
#define STATUS_HALT_STATUS_BM 0x08
#define STATUS_DEBUG_LOCKED_BM 0x04
#define STATUS_OSC_STABLE_BM 0x02
#define STATUS_STACK_OVERFLOW_BM 0x01

// Registers
#define CC_DBGDATA 0x6260   // Debug interface data buffer
#define CC_FCTL 0x6270      // Flash controller
#define CC_FADDRL 0x6271    // Flash controller addr
#define CC_FADDRH 0x6272    // Flash controller addr
#define CC_FWDATA 0x6273    // Clash controller data buffer
#define CC_CLKCONSTA 0x709E // Sys clock status
#define CC_CLKCONCMD 0x70C6 // Sys clock configuration
#define CC_MEMCTR 0x70C7    // Flash bank xdata mapping
#define CC_DMA1CFGL 0x70D2  // Low byte, DMA config ch. 1
#define CC_DMA1CFGH 0x70D3  // Hi byte , DMA config ch. 1
#define CC_DMA0CFGL 0x70D4  // Low byte, DMA config ch. 0
#define CC_DMA0CFGH 0x70D5  // Low byte, DMA config ch. 0
#define CC_DMAARM 0x70D6    // DMA arming register

// Macros
#define LOBYTE(w) (w & 0xFF)
#define HIBYTE(w) ((w >> 8) & 0xFF)

// Return Codes
#define OK 0x00
#define ERRO 0xFF

// Misc
#define R_BUFSIZE 32768
#define W_BUFSIZE 512
#define PAGESIZE 2048
#define MAXPAGES 128

const uint8_t dma_desc_0[8] =
    {
        // Debug Interface -> Buffer
        HIBYTE(CC_DBGDATA), // src[15:8]
        LOBYTE(CC_DBGDATA), // src[7:0]
        HIBYTE(ADDR_BUF0),   // dest[15:8]
        LOBYTE(ADDR_BUF0),   // dest[7:0]
        0,                   // len[12:8] - filled in later
        0,                   // len[7:0]
        31,                  // trigger: DBG_BW
        0x11                 // increment destination
};

const uint8_t dma_desc_1[8] =
    {
        // Buffer -> Flash controller
        HIBYTE(ADDR_BUF0),  // src[15:8]
        LOBYTE(ADDR_BUF0),  // src[7:0]
        HIBYTE(CC_FWDATA), // dest[15:8]
        LOBYTE(CC_FWDATA), // dest[7:0]
        0,                  // len[12:8] - filled in later
        0,                  // len[7:0]
        18,                 // trigger: FLASH
        0x42,               // increment source
};


uint8_t ddIsOutput = 0;

uint8_t wiringPi()
{
        if (wiringPiSetup() == -1)
        {
                printf("no wiring pi detected\n");
                return (ERRO);
        }
        return (OK);
}

void cc_delay(uint8_t d)
{
        volatile uint8_t i = 3000 * d;
        while (i--)
                ;
}

void setDirection(uint8_t direction)
{

        if (direction == ddIsOutput)
                return;
        ddIsOutput = direction;

        if (ddIsOutput)
        {
                digitalWrite(DD, LOW);
                pinMode(DD, OUTPUT);
                digitalWrite(DD, LOW);
        }
        else
        {
                digitalWrite(DD, LOW);
                pinMode(DD, INPUT);
                digitalWrite(DD, LOW);
        }
}

void switchWrite()
{
        setDirection(OUTPUT);
}

uint8_t switchRead(uint8_t maxWaitCycles)
{
        uint8_t cnt;
        uint8_t didWait = 0;

        setDirection(INPUT);
        cc_delay(2);
        while (digitalRead(DD) == HIGH)
        {
                for (cnt = 8; cnt; cnt--)
                {
                        digitalWrite(DC, HIGH);
                        cc_delay(2);
                        digitalWrite(DC, LOW);
                        cc_delay(2);
                }
                didWait = 1;

                if (!--maxWaitCycles)
                {
                        return (ERRO);
                }
        }

        if (didWait)
                cc_delay(2);

        return 0;
}

void reset()
{
        pinMode(DC, INPUT);
        pinMode(DD, INPUT);
        pinMode(RESET, OUTPUT);
        
        digitalWrite(RESET, LOW);
        cc_delay(250);
        delayMicroseconds(DELAY);
        digitalWrite(RESET, HIGH);
        cc_delay(250);
        delayMicroseconds(DELAY);
}

static void gpio_init()
{

        pinMode(DC, OUTPUT);
        pinMode(DD, OUTPUT);
        pinMode(RESET, OUTPUT);
        digitalWrite(DC, LOW);
        digitalWrite(DD, LOW);
        digitalWrite(RESET, LOW);
        delayMicroseconds(DELAY);
        setDirection(INPUT);
}

// Write byte on the debug interface.
#pragma inline
void write_debug_byte(uint8_t data)
{
        setDirection(OUTPUT);

        for (uint8_t i = 0; i < 8; i++)
        {
                if (data & 0x80)
                {
                        digitalWrite(DD, HIGH);
                }
                else
                {
                        digitalWrite(DD, LOW);
                }
                digitalWrite(DC, HIGH);
                data <<= 1;
                cc_delay(2);
                digitalWrite(DC, LOW); // set clock low (DUP capture flank)
                cc_delay(2);
        }
}

// Read byte from the debug interface.
#pragma inline
uint8_t read_debug_byte(void)
{
        uint8_t data = 0x00;

        setDirection(INPUT);

        for (uint8_t i = 0; i < 8; i++)
        {
                digitalWrite(DC, HIGH);
                cc_delay(2);
                data <<= 1;
                if (HIGH == digitalRead(DD))
                {
                        data |= 0x01;
                }
                digitalWrite(DC, LOW); // DC low
                cc_delay(2);
        }
        return data;
}

// Waits for CC to be ready.
#pragma inline
uint8_t wait_cc_ready(void) {
        uint8_t count = 0;
        while ((HIGH == digitalRead(DD)) && count < 16)
        {
                read_debug_byte();
                count++;
        }
        return (count == 16) ? 0 : 1;
}

// Send command to debug interface.
uint8_t debug_command(uint8_t cmd, uint8_t *cmd_bytes, uint16_t num_cmd_bytes)
{
        uint8_t output = 0;
        pinMode(DD, OUTPUT);
        write_debug_byte(cmd);
        for (uint16_t i = 0; i < num_cmd_bytes; i++)
        {
                write_debug_byte(cmd_bytes[i]);
        }
        pinMode(DD, INPUT);
        digitalWrite(DD, HIGH);
        wait_cc_ready();
        output = read_debug_byte();
        pinMode(DD, OUTPUT);

        return output;
}

uint8_t debug_exit(void)
{
        uint8_t ret;

        write_debug_byte(0x48);
        switchRead(250);
        ret = read_debug_byte();
        switchWrite();
        return 0;
}

// Enter debug mode.
void debug_enter(void)
{
        digitalWrite(DD, LOW);
        digitalWrite(DC, LOW);
        digitalWrite(RESET, LOW);
        cc_delay(250);
        delayMicroseconds(DELAY);
          digitalWrite(DC, HIGH);
          cc_delay(2);
          digitalWrite(DC, LOW);
          cc_delay(2);
          digitalWrite(DC, HIGH);
          cc_delay(2);
          digitalWrite(DC, LOW);
          cc_delay(3);
        digitalWrite(RESET, HIGH);
        cc_delay(250);
        delayMicroseconds(DELAY);
}

// Fetch Chip-ID
uint8_t read_chip_id(uint8_t verbose)
{
        uint8_t id = 0;
        uint8_t r = 0;

        pinMode(DD, OUTPUT);
        cc_delay(1);
        write_debug_byte(CMD_GET_CHIP_ID);
        pinMode(DD, INPUT);
        digitalWrite(DD, HIGH);
        cc_delay(1);
        if (wait_cc_ready() == 1)
        {
                id = read_debug_byte(); // ID
                r = read_debug_byte();  // Revision (discard)
        }
        // Set DD as output
        pinMode(DD, OUTPUT);

        if (verbose)
                printf("CHIPID: 0x%02x%02x\n", id, r);

        return id;
}

// Burst-send data block via debug interface.
void burst_write_block(uint8_t *src, uint16_t num_bytes)
{
        pinMode(DD, OUTPUT);

        write_debug_byte(CMD_BURST_WRITE | HIBYTE(num_bytes));
        write_debug_byte(LOBYTE(num_bytes));
        for (uint16_t i = 0; i < num_bytes; i++)
        {
                write_debug_byte(src[i]);
        }

        // Set DD as input
        switchRead(255);
        //pinMode(DD, INPUT);
        //digitalWrite(DD, HIGH);
        // Wait for DUP to be ready
        //wait_cc_ready();
        read_debug_byte();
        // Set DD as output
        // pinMode(DD, OUTPUT);
}

// Erase flash.
void chip_erase(void)
{
        volatile uint8_t status;
        debug_command(CMD_CHIP_ERASE, 0, 0);

        do
        {
                status = debug_command(CMD_READ_STATUS, 0, 0);
        } while ((status & STATUS_CHIP_ERASE_BUSY_BM));
}

// Write data block to XDATA buffer.
void write_xdata_memory_block(uint16_t address, const uint8_t *values, uint16_t num_bytes)
{
        uint8_t instr[3];

        // MOV DPTR, address
        instr[0] = 0x90;
        instr[1] = HIBYTE(address);
        instr[2] = LOBYTE(address);
        debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

        for (uint16_t i = 0; i < num_bytes; i++)
        {
                // MOV A, values[i]
                instr[0] = 0x74;
                instr[1] = values[i];
                debug_command(CMD_DEBUG_INSTR_2B, instr, 2);

                // MOV @DPTR, A
                instr[0] = 0xF0;
                debug_command(CMD_DEBUG_INSTR_1B, instr, 1);

                // INC DPTR
                instr[0] = 0xA3;
                debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
        }
}

// Write byte to XDATA address
void write_xdata_byte(uint16_t address, uint8_t value)
{
        uint8_t instr[3];

        // MOV DPTR, address
        instr[0] = 0x90;
        instr[1] = HIBYTE(address);
        instr[2] = LOBYTE(address);
        debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

        // MOV A, values[i]
        instr[0] = 0x74;
        instr[1] = value;
        debug_command(CMD_DEBUG_INSTR_2B, instr, 2);

        // MOV @DPTR, A
        instr[0] = 0xF0;
        debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
}

// Read byte from XDATA address
uint8_t read_xdata_byte(uint16_t address)
{
        uint8_t instr[3];

        // MOV DPTR, address
        instr[0] = 0x90;
        instr[1] = HIBYTE(address);
        instr[2] = LOBYTE(address);
        debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

        // MOVX A, @DPTR
        instr[0] = 0xE0;
        return debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
}

// Read block from flash (1-32767 bytes)
void read_flash_memory_block(uint32_t addr, uint16_t num_bytes, uint8_t *values)
{
        uint8_t instr[3];
        uint16_t xdata_addr;
        uint8_t bank;

        bank = addr >> 15;
        xdata_addr = (0x8000 + (((addr / 4) % (512 * 16)) * 4));

        write_xdata_byte(CC_MEMCTR, bank);

        instr[0] = 0x90;
        instr[1] = HIBYTE(xdata_addr);
        instr[2] = LOBYTE(xdata_addr);
        debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

        for (uint16_t i = 0; i < num_bytes; i++)
        {
                instr[0] = 0xE0;
                values[i] = debug_command(CMD_DEBUG_INSTR_1B, instr, 1);

                instr[0] = 0xA3;
                debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
        }
}

// Write block to flash (4-2048 bytes / multiple of 4)
void write_flash_memory_block(uint8_t *src, uint32_t start_addr, uint16_t num_bytes)
{
        write_xdata_memory_block(ADDR_DMA_DESC_0, dma_desc_0, 8);
        write_xdata_memory_block(ADDR_DMA_DESC_1, dma_desc_1, 8);

        uint8_t len[2] = {HIBYTE(num_bytes), LOBYTE(num_bytes)};
        write_xdata_memory_block((ADDR_DMA_DESC_0 + 4), len, 2); // LEN, DBG => ram
        write_xdata_memory_block((ADDR_DMA_DESC_1 + 4), len, 2); // LEN, ram => flash

        write_xdata_byte(CC_DMA0CFGH, HIBYTE(ADDR_DMA_DESC_0));
        write_xdata_byte(CC_DMA0CFGL, LOBYTE(ADDR_DMA_DESC_0));
        write_xdata_byte(CC_DMA1CFGH, HIBYTE(ADDR_DMA_DESC_1));
        write_xdata_byte(CC_DMA1CFGL, LOBYTE(ADDR_DMA_DESC_1));

        write_xdata_byte(CC_FADDRH, HIBYTE(start_addr >> 2));
        write_xdata_byte(CC_FADDRL, LOBYTE(start_addr >> 2));

        write_xdata_byte(CC_DMAARM, CH_DBG_TO_BUF0);
        burst_write_block(src, num_bytes);

        while (read_xdata_byte(CC_FCTL) & 0x80)
                ;

        write_xdata_byte(CC_DMAARM, CH_BUF0_TO_FLASH);
        write_xdata_byte(CC_FCTL, 0x06); //0x0A

        while (read_xdata_byte(CC_FCTL) & 0x80)
                ;
}

void ProgrammerInit(void)
{
        pinMode(DD, OUTPUT);
        pinMode(DC, OUTPUT);
        pinMode(RESET, OUTPUT);
        //pinMode(LED, OUTPUT);
        digitalWrite(DD, LOW);
        digitalWrite(DC, LOW);
        digitalWrite(RESET, HIGH);
        //digitalWrite(LED, LOW);
        cc_delay(250);
        delayMicroseconds(DELAY);
}

uint8_t flash(uint8_t *fw, uint8_t pages, uint8_t Verify, uint8_t Verbose)
{
        uint8_t chip_id = 0;
        uint8_t debug_config = 0;

        debug_enter();
        chip_id = read_chip_id(Verbose);
        if (chip_id == 0 || chip_id == 0xFF)
        {
                return (ERRO);
        }

        chip_erase();

        write_xdata_byte(CC_CLKCONCMD, 0x80);
        while (read_xdata_byte(CC_CLKCONSTA) != 0x80)
                ; //0x80)

        debug_config = 0x22;
        debug_command(CMD_WR_CONFIG, &debug_config, 1);

        uint8_t *txPtr;
        uint16_t iterations;
        uint32_t addr = 0;

        // PAGESIZE = 2048
        // PAGES=128
        // BANKS=PAGESIZE x 16

        iterations = pages * PAGESIZE / W_BUFSIZE;

        uint32_t *dWordPtr;

        for (uint16_t page = 0; page < iterations; page++)
        {
                addr = page * W_BUFSIZE;
                txPtr = fw + addr;
                dWordPtr=(uint32_t *)txPtr;

                // check if block needs to be written to flash   
                for (uint16_t i = 0; i < W_BUFSIZE/4; i++)
                {
                        if (dWordPtr[i] != 0xFFFFFFFF)
                        {
                                printf("flashing block %d of %d.\r", (page + 1), iterations);
                                fflush(stdout);
                                write_flash_memory_block(txPtr, addr, W_BUFSIZE); // src, address, count
                                break;
                        }
                }
                fflush(stdout);
        }
        printf("\n");

        printf("\rVerify %d", Verify);
        if (Verify)
        {
                uint32_t *ptr1, *ptr2;
                // uint32_t mempos = 0x0;
                uint8_t read_data[R_BUFSIZE];

        	iterations = pages * PAGESIZE / R_BUFSIZE;

                for (uint16_t page = 0; page < iterations; page++)
                {
                        printf("\rverifying block %d of %d", (page + 1), iterations);
                        fflush(stdout);
                        addr = page * R_BUFSIZE;
                        read_flash_memory_block(addr, R_BUFSIZE, read_data); // addr, count, dest
                        if (memcmp(read_data,fw+addr,W_BUFSIZE)) {
                            printf(" - FAILED.\n");
                            fflush(stdout);
                            return (ERRO);
                        }

                }
                printf(" - OK.");
        }
        printf("\n");
        return (OK);
}

uint8_t *readFile(char *fn, uint8_t *size, bool verbose)
{
        uint8_t *buf;
        uint8_t buffer[601];
        uint8_t data[260];

        buf = malloc(PAGESIZE * MAXPAGES);

        FILE *ficin = fopen(fn, "r");
        if (!ficin)
        {
                fprintf(stderr, " Can't open file %s.\n", fn);
                exit(1);
        }
        for (uint16_t page = 0; page < MAXPAGES; page++)
        {
                memset(buf + (page * PAGESIZE), 0xff, PAGESIZE);
        }

        uint16_t ela = 0; // extended linear address
        uint32_t sla = 0; // start linear address
        // read hex file
        uint16_t line = 0;
        uint16_t maxpage = 0;
        while (fgets(buffer, 600, ficin))
        {
                uint16_t sum = 0, cksum, type;
                uint32_t addr, len;
                line++;
                if (line % 10 == 0 && verbose)
                {
                        printf("\r  reading line %d.");
                        fflush(stdout);
                }
                if (buffer[0] != ':')
                {
                        fprintf(stderr, "incorrect hex file ( : missing)\n");
                        exit(1);
                }
                if (strlen(buffer) < 3)
                {
                        fprintf(stderr, "incorrect hex file ( incomplete line)\n");
                        exit(1);
                }
                if (!sscanf(buffer + 1, "%02x", &len))
                {
                        fprintf(stderr, "incorrect hex file (incorrect length\n");
                        exit(1);
                }
                if (strlen(buffer) < (11 + (len * 2)))
                {
                        fprintf(stderr, "incorrect hex file ( incomplete line)\n");
                        exit(1);
                }
                if (!sscanf(buffer + 3, "%04x", &addr))
                {
                        fprintf(stderr, "incorrect hex file (incorrect addr)\n");
                        exit(1);
                }
                if (!sscanf(buffer + 7, "%02x", &type))
                {
                        fprintf(stderr, "incorrect hex file (incorrect record type\n");
                        exit(1);
                }
                if (type == 4)
                {
                        if (!sscanf(buffer + 9, "%04x", &ela))
                        {
                                fprintf(stderr, "incorrect hex file (incorrect extended addr)\n");
                                exit(1);
                        }
                        sla = ela << 16;
                        continue;
                }
                if (type == 5)
                {
                        if (!sscanf(buffer + 9, "%08x", &sla))
                        {
                                fprintf(stderr, "incorrect hex file (incorrect extended addr)\n");
                                exit(1);
                        }
                        ela = sla >> 16;
                        continue;
                }
                if (type == 1) // EOF
                {
                        break;
                }
                if (type)
                {
                        fprintf(stderr, "incorrect hex file (record type %d not implemented\n", type);
                        exit(1);
                }
                sum = (len & 255) + ((addr >> 8) & 255) + (addr & 255) + (type & 255);
                uint16_t i;
                for (i = 0; i < len; i++)
                {
                        if (!sscanf(buffer + 9 + 2 * i, "%02x", &data[i]))
                        {
                                fprintf(stderr, "incorrect hex file (incorrect data)\n");
                                exit(1);
                        }
                        sum += data[i];
                }
                if (!sscanf(buffer + 9 + 2 * i, "%02x", &cksum))
                {
                        fprintf(stderr, "incorrect hex file line %d (incorrect checksum)\n", line);
                        exit(1);
                }
                if (((sum & 255) + (cksum & 255)) & 255)
                {
                        fprintf(stderr, "incorrect hex file line %d (bad checksum) %x %x\n", line, (-sum) & 255, cksum);
                        exit(1);
                }
                // stock datas
                uint16_t page = (sla + addr) >> 11;
                if (page > maxpage)
                        maxpage = page;
                uint16_t start = (sla + addr) & 0x7ff;
                if (start + len > 2048) // some datas are for next page
                {                       //copy end of datas to next page
                        if (page + 1 > maxpage)
                                maxpage = page + 1;
                        memcpy(buf + ((page + 1) * PAGESIZE), data + PAGESIZE - start, (start + len - PAGESIZE));
                        len = 2048 - start;
                }
                memcpy(buf + (page * PAGESIZE) + start, data, len);
        }
        if (verbose)
                printf("\n  file loaded (%d lines / %d pages read).\n", line, maxpage + 1);

        *size = (maxpage + 1);

        return buf;
}

static void usage(void)
{
        printf("Usage: yaccf [options]\n"
               "\t-r:            reset device\n"
               "\t-V:            verify after flashing\n"
               "\t-f <firmware>: flash <firmware> file\n");
        exit(-1);
}

int main(int ac, char **av)
{
        int opt;
        uint8_t *firmware = NULL;
        uint8_t do_reset = 0;
        uint8_t verify = 0;
        uint8_t verbose = 1;
        struct stat buf;

        uint8_t *fw;
        uint16_t i, j, k;
        uint16_t pos = 0;
        uint8_t len;

        while ((opt = getopt(ac, av, "f:Vr")) > 0)
        {
                switch (opt)
                {
                case 'f':
                        firmware = optarg;
                        break;
                case 'r':
                        do_reset = 1;
                        break;
                case 'V':
                        verify = 1;
                        break;
                default:
                        break;
                }
        }
        ac -= optind;
        ac += optind;

        if (ac < 2)
                usage();

        wiringPi();

        if (firmware != NULL)
        {
                if (stat(firmware, &buf) < 0)
                {
                        perror("stat");
                        exit(255);
                }

                if (!S_ISREG(buf.st_mode))
                {
                        fprintf(stderr, "%s is not a regular file\n", firmware);
                        exit(255);
                }

                printf("Reading file: %s\n", firmware);
                fw = readFile(firmware, &len, false);
                gpio_init();
                flash(fw, len, verify, verbose);
                debug_exit();
                sleep(1);
        }

        if (do_reset)
        {
                if (verbose)
                        printf("resetting...\n");
                reset();
        }

        fflush(stdout);
}
