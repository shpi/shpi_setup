#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>

#define PWM_CHANNEL   1
#define PWM_GPIO      19 /* GPIO pin for P1-35 */
#define PWM_FIFO_DEPTH 16
#define PWM_DIVISOR   4
#define PWM_RANGE     32
#define PATTERN_MAX_WORDS 18

/* Minimal register definitions from bcm2835 library */
#define BLOCK_SIZE            (4*1024)

static uint32_t peri_base = 0x20000000; /* updated when mapping succeeds */

#define GPIO_BASE_OFFSET      0x200000
#define PWM_BASE_OFFSET       0x20C000
#define CLK_BASE_OFFSET       0x101000

#define GPFSEL0               0x0000
#define GPSET0                0x001c
#define GPCLR0                0x0028
#define GPPUD                 0x0094
#define GPPUDCLK0             0x0098

#define PWM_CONTROL           0
#define PWM_STATUS            1
#define PWM_DMAC              2
#define PWM0_RANGE            4
#define PWM0_DATA             5
#define PWM_FIF1              6
#define PWM1_RANGE            8
#define PWM1_DATA             9

#define PWMCLK_CNTL           40
#define PWMCLK_DIV            41
#define PWM_PASSWRD           (0x5A << 24)

#define PWM1_MS_MODE  0x8000
#define PWM1_USEFIFO  0x2000
#define PWM1_REVPOLAR 0x1000
#define PWM1_OFFSTATE 0x0800
#define PWM1_REPEATFF 0x0400
#define PWM1_SERIAL   0x0200
#define PWM1_ENABLE   0x0100
#define CLEAR_FIFO    0x0040

#define GPIO_FSEL_MASK 0x07
#define GPIO_FSEL_INPT 0x00
#define GPIO_FSEL_OUTP 0x01
#define GPIO_FSEL_ALT5 0x02

#define GPIO_PUD_OFF   0x00
#define GPIO_PUD_DOWN  0x01
#define GPIO_PUD_UP    0x02

static volatile uint32_t *gpio;
static volatile uint32_t *pwm;
static volatile uint32_t *clk;

static int map_peripherals(void)
{
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("open /dev/mem");
        return 0;
    }

    uint32_t bases[] = {0x20000000, 0x3F000000, 0xFE000000};
    size_t i;
    for (i = 0; i < sizeof(bases)/sizeof(bases[0]); i++) {
        peri_base = bases[i];
        gpio = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mem_fd, peri_base + GPIO_BASE_OFFSET);
        pwm  = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mem_fd, peri_base + PWM_BASE_OFFSET);
        clk  = mmap(NULL, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                    mem_fd, peri_base + CLK_BASE_OFFSET);
        if (gpio != MAP_FAILED && pwm != MAP_FAILED && clk != MAP_FAILED)
            break;

        if (gpio != MAP_FAILED) munmap((void *)gpio, BLOCK_SIZE);
        if (pwm  != MAP_FAILED) munmap((void *)pwm,  BLOCK_SIZE);
        if (clk  != MAP_FAILED) munmap((void *)clk,  BLOCK_SIZE);
    }
    close(mem_fd);

    if (i == sizeof(bases)/sizeof(bases[0])) {
        perror("mmap");
        return 0;
    }
    return 1;
}

static void unmap_peripherals(void)
{
    if (gpio && gpio != MAP_FAILED) munmap((void*)gpio, BLOCK_SIZE);
    if (pwm && pwm != MAP_FAILED) munmap((void*)pwm, BLOCK_SIZE);
    if (clk && clk != MAP_FAILED) munmap((void*)clk, BLOCK_SIZE);
}

static inline void delayMicroseconds(unsigned int micros)
{
    struct timespec ts;
    ts.tv_sec = micros / 1000000;
    ts.tv_nsec = (micros % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

static inline void peri_write(volatile uint32_t *addr, uint32_t val)
{
    *addr = val;
}

static inline uint32_t peri_read(volatile uint32_t *addr)
{
    return *addr;
}

static void gpio_fsel(uint8_t pin, uint8_t mode)
{
    volatile uint32_t *reg = gpio + (GPFSEL0/4) + pin/10;
    uint8_t shift = (pin % 10) * 3;
    uint32_t value = peri_read(reg);
    value &= ~(GPIO_FSEL_MASK << shift);
    value |= (mode << shift);
    peri_write(reg, value);
}

static inline void gpio_set(uint8_t pin)
{
    peri_write(gpio + GPSET0/4 + pin/32, 1 << (pin % 32));
}

static inline void gpio_clr(uint8_t pin)
{
    peri_write(gpio + GPCLR0/4 + pin/32, 1 << (pin % 32));
}

static void gpio_set_pud(uint8_t pin, uint8_t pud)
{
    peri_write(gpio + GPPUD/4, pud & 3);
    delayMicroseconds(5);
    peri_write(gpio + GPPUDCLK0/4 + pin/32, 1 << (pin % 32));
    delayMicroseconds(5);
    peri_write(gpio + GPPUD/4, 0);
    peri_write(gpio + GPPUDCLK0/4 + pin/32, 0);
}

static void pwm_set_clock(uint32_t divisor)
{
    divisor &= 0xfff;
    peri_write(clk + PWMCLK_CNTL, PWM_PASSWRD | 0x01);
    delayMicroseconds(110);
    while (peri_read(clk + PWMCLK_CNTL) & 0x80)
        delayMicroseconds(1);
    peri_write(clk + PWMCLK_DIV, PWM_PASSWRD | (divisor << 12));
    peri_write(clk + PWMCLK_CNTL, PWM_PASSWRD | 0x11);
}

static inline void pwm_set_range(uint8_t channel, uint32_t range)
{
    if (channel == 0)
        peri_write(pwm + PWM0_RANGE, range);
    else
        peri_write(pwm + PWM1_RANGE, range);
}

static void pwm_reset(void)
{
    /* Disable PWM and stop its clock */
    peri_write(pwm + PWM_CONTROL, 0);
    delayMicroseconds(10);
    peri_write(clk + PWMCLK_CNTL, PWM_PASSWRD | 0x01);
    delayMicroseconds(110);
    while (peri_read(clk + PWMCLK_CNTL) & 0x80)
        delayMicroseconds(1);

    /* Clear status flags and FIFO */
    peri_write(pwm + PWM_STATUS, 0x1FF);
    peri_write(pwm + PWM_DMAC, 0);
    peri_write(pwm + PWM_CONTROL, CLEAR_FIFO);
    peri_write(pwm + PWM_CONTROL, 0);
}

// Kompakte Init-Sequenz
void al3050_sw_init() {
    gpio_fsel(PWM_GPIO, GPIO_FSEL_OUTP);
    gpio_clr(PWM_GPIO);
    delayMicroseconds(4000);  // Shutdown
    gpio_set(PWM_GPIO);
    delayMicroseconds(250);   // Startup
    gpio_clr(PWM_GPIO);
    delayMicroseconds(600);   // Detection
    gpio_set(PWM_GPIO);
    delayMicroseconds(1);
}

void clear_pwm_fifo() {
    uint32_t ctl = (1 << 6);  // CLRF1 bit
    peri_write(pwm + PWM_CONTROL, ctl);
    peri_write(pwm + PWM_CONTROL, 0);
}

// Korrekte Pattern-Generierung
static int build_pattern(uint32_t *buf, uint8_t brightness) {
    int word_count = 0;
    uint8_t bytes[2] = {0x58, brightness & 0x1f};
    
    for (int b = 0; b < 2; b++) {
        for (int i = 7; i >= 0; i--) {
            buf[word_count] = 0;
            if (bytes[b] & (1 << i)) {
                // Bit 1: Low 9, High 23
                for (int j = 9; j < 32; j++) {
                    buf[word_count] |= (1u << (31 - j));
                }
            } else {
                // Bit 0: Low 23, High 9
                for (int j = 23; j < 32; j++) {
                    buf[word_count] |= (1u << (31 - j));
                }
            }
            word_count++;
        }
        buf[word_count++] = 0x0000FFFF;  // EOS + Start
    }
    return word_count;
}

int main(int argc, char **argv) {
    uint8_t level = (argc > 1) ? (uint8_t)atoi(argv[1]) : 31;
    if (level > 31) level = 31;

    if (!map_peripherals()) {
        fprintf(stderr, "unable to map peripherals\n");
        return 1;
    }

    pwm_reset();
    al3050_sw_init();
    
    uint32_t pattern[PATTERN_MAX_WORDS];
    int words = build_pattern(pattern, level);
    
    // PWM konfigurieren
    pwm_set_clock(PWM_DIVISOR);
    pwm_set_range(PWM_CHANNEL, PWM_RANGE);
    
    // FIFO vor Verwendung leeren
    clear_pwm_fifo();
    
    uint32_t ctl = PWM1_SERIAL | PWM1_USEFIFO | PWM1_OFFSTATE;
    peri_write(pwm + PWM_CONTROL, ctl);
    
    // Initial FIFO füllen
    int sent = 0;
    int init_words = (words > PWM_FIFO_DEPTH) ? PWM_FIFO_DEPTH : words;
    for (int i = 0; i < init_words; i++) {
        peri_write(pwm + PWM_FIF1, pattern[i]);
    }
    sent = init_words;
    
    // GPIO auf PWM umschalten und aktivieren
    gpio_fsel(PWM_GPIO, GPIO_FSEL_ALT5);
    ctl |= PWM1_ENABLE;
    peri_write(pwm + PWM_CONTROL, ctl);
    
    // Restliche Daten nachladen
    while (sent < words) {
        while (peri_read(pwm + PWM_STATUS) & 0x01);  // Warten bis FIFO nicht voll
        peri_write(pwm + PWM_FIF1, pattern[sent++]);
    }

    // Warten bis FIFO leer
    while (!(peri_read(pwm + PWM_STATUS) & 0x02));

    gpio_set_pud(PWM_GPIO, GPIO_PUD_UP);
    gpio_fsel(PWM_GPIO, GPIO_FSEL_INPT);
    unmap_peripherals();
    
    printf("AL3050-Wert %d gesendet (Pattern-Länge: %d Words)\n", level, words);
    return 0;
}
