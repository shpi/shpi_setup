
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <errno.h>

#define WS2812_SYMBOL_LENGTH			 4
#define LED_COLOURS                              3
#define LED_RESET_US                             60
#define LED_BIT_COUNT 		                 ((NUM_LEDS * LED_COLOURS * 8 * WS2812_SYMBOL_LENGTH) + ((LED_RESET_US * \
                                                  (WS2812_FREQ * WS2812_SYMBOL_LENGTH)) / 1000000))

#define LED_RESET_WAIT_TIME                      300
#define SPI_BYTE_COUNT		               ((((LED_BIT_COUNT >> 3) & ~0x7) + 4) + 4)
#define SYMBOL_HIGH                              0b1100 // 0.6us high 0.6us low
#define SYMBOL_LOW                               0b1000 // 0.3us high, 0.9us low
#define WS2812_FREQ                             800000

/*  data transmission time (TH+TL=1.25µs±600ns) */

#define NUM_LEDS                                29




struct ws2812_led {
        //struct led_classdev     cdev;
        //struct device           *master;
        uint8_t                  actual_led;
        uint8_t leds[NUM_LEDS][3];
	volatile uint8_t *rawstream;
	uint8_t spi_fd;

};



void  ws2812_render(struct ws2812_led* led)
{
    int bitpos;
    int i, k, l;
    unsigned j;
    bitpos =  7;
    int bytepos = 0;    // SPI
    for (i = 0; i < NUM_LEDS; i++)                // Led
        {
            for (j = 0; j < 3; j++)               // Color
            {
                for (k = 7; k >= 0; k--)                   // Bit
                {
                    uint8_t symbol = SYMBOL_LOW;
                    if (led->leds[i][j] & (1 << k))
                        symbol = SYMBOL_HIGH;

                    for (l = WS2812_SYMBOL_LENGTH; l > 0; l--)               // Symbol
                    {
                        volatile uint8_t  *byteptr = &led->rawstream[bytepos];    // SPI

                        *byteptr &= ~(1 << bitpos);
                        if (symbol & (1 << l))
                                *byteptr |= (1 << bitpos);
                        bitpos--;
                        if (bitpos < 0)
                        {
                                bytepos++;
                                bitpos = 7;
                            }
                        }
                    }
                }
    }

}


uint8_t ws2812_init(struct ws2812_led* led){

	uint8_t mode;
	uint8_t spi_fd;
	uint8_t bits = 8;
	uint32_t speed =  WS2812_FREQ * WS2812_SYMBOL_LENGTH;
	mode |= SPI_NO_CS;

	spi_fd = open("/dev/spidev0.0", O_RDWR);

   	if (spi_fd < 0) {
        fprintf(stderr, "Cannot open /dev/spidev0.0. spi_bcm2835 module not loaded?\n");
            }

    // SPI mode
   if   (
//(ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0)            ||
//         (ioctl(spi_fd, SPI_IOC_RD_MODE, &mode) < 0)            ||
//         (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)       ||
//         (ioctl(spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0)       ||
         (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ,  &speed) < 0)   ||
        (ioctl(spi_fd, SPI_IOC_RD_MAX_SPEED_HZ,  &speed) < 0))
    {
        fprintf(stderr, "spi_bcm2835 error\n");
    }

return spi_fd;

}

int main(int argc, char *argv[]) {

struct ws2812_led led;

led.spi_fd = ws2812_init(&led);


led.leds[0][0] = 0x00;
led.leds[0][1] = 0xff;
led.leds[0][2] = 0x00;

led.rawstream =  malloc(SPI_BYTE_COUNT);


ws2812_render(&led);


///sys/kernel/debug/clk/fw-clk-core/clk_rate

    struct spi_ioc_transfer tr = {

    //memset(&tr, 0, sizeof(struct spi_ioc_transfer));
    .tx_buf = (unsigned long)led.rawstream,
    .rx_buf = 0,
    .len = SPI_BYTE_COUNT,
    };
    ioctl(led.spi_fd, SPI_IOC_MESSAGE(1), &tr);
  }


