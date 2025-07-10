#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PWM_CHANNEL   1
#define PWM_GPIO      RPI_V2_GPIO_P1_35
#define PWM_FIFO_DEPTH 16
#define PWM_DIVISOR   4
#define PWM_RANGE     32
#define PATTERN_MAX_WORDS 18

// Kompakte Init-Sequenz
void al3050_sw_init() {
    bcm2835_gpio_fsel(PWM_GPIO, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_clr(PWM_GPIO);
    bcm2835_delayMicroseconds(4000);  // Shutdown
    bcm2835_gpio_set(PWM_GPIO);
    bcm2835_delayMicroseconds(250);   // Startup
    bcm2835_gpio_clr(PWM_GPIO);
    bcm2835_delayMicroseconds(600);   // Detection
    bcm2835_gpio_set(PWM_GPIO);
    bcm2835_delayMicroseconds(1);
}

void clear_pwm_fifo() {
    uint32_t ctl = (1 << 6);  // CLRF1 bit
    bcm2835_peri_write(bcm2835_pwm + BCM2835_PWM_CONTROL, ctl);
    bcm2835_peri_write(bcm2835_pwm + BCM2835_PWM_CONTROL, 0);
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
    
    if (!bcm2835_init()) {
        fprintf(stderr, "bcm2835_init failed\n");
        return 1;
    }
    
    al3050_sw_init();
    
    uint32_t pattern[PATTERN_MAX_WORDS];
    int words = build_pattern(pattern, level);
    
    // PWM konfigurieren
    bcm2835_pwm_set_clock(PWM_DIVISOR);
    bcm2835_pwm_set_range(PWM_CHANNEL, PWM_RANGE);
    
    // FIFO vor Verwendung leeren
    clear_pwm_fifo();
    
    uint32_t ctl = BCM2835_PWM1_SERIAL | BCM2835_PWM1_USEFIFO | BCM2835_PWM1_OFFSTATE;
    bcm2835_peri_write(bcm2835_pwm + BCM2835_PWM_CONTROL, ctl);
    
    // Initial FIFO füllen
    int sent = 0;
    int init_words = (words > PWM_FIFO_DEPTH) ? PWM_FIFO_DEPTH : words;
    for (int i = 0; i < init_words; i++) {
        bcm2835_peri_write(bcm2835_pwm + BCM2835_PWM_FIF1, pattern[i]);
    }
    sent = init_words;
    
    // GPIO auf PWM umschalten und aktivieren
    bcm2835_gpio_fsel(PWM_GPIO, BCM2835_GPIO_FSEL_ALT5);
    ctl |= BCM2835_PWM1_ENABLE;
    bcm2835_peri_write(bcm2835_pwm + BCM2835_PWM_CONTROL, ctl);
    
    // Restliche Daten nachladen
    while (sent < words) {
        while (bcm2835_peri_read(bcm2835_pwm + BCM2835_PWM_STATUS) & 0x01);  // Warten bis FIFO nicht voll
        bcm2835_peri_write(bcm2835_pwm + BCM2835_PWM_FIF1, pattern[sent++]);
    }
    
    // Warten bis FIFO leer
    while (!(bcm2835_peri_read(bcm2835_pwm + BCM2835_PWM_STATUS) & 0x02));
    
    bcm2835_gpio_set_pud(PWM_GPIO, BCM2835_GPIO_PUD_UP); 
    bcm2835_gpio_fsel(PWM_GPIO, BCM2835_GPIO_FSEL_INPT);
    bcm2835_close();
    
    printf("AL3050-Wert %d gesendet (Pattern-Länge: %d Words)\n", level, words);
    return 0;
}
