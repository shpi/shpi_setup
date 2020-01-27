#include <bcm2835.h>
#include <stdio.h>

#define CLK RPI_V2_GPIO_P1_03 // BCM 2
#define MOSI RPI_V2_GPIO_P1_12 // reuse fan gpio
#define CS RPI_V2_GPIO_P1_35 // use backlight
#define DELAY 50 // clock pulse time in microseconds
#define WAIT 120 // wait time in milliseconds

#define PIN RPI_V2_GPIO_P1_35


uint16_t commands2[] =  {0x0080, 0x01F8,  0x0246, 0x0305, 0x0440,  0x0540, 0x0640, 0x0740, 0x0840,  0x0940, 0x0A03};            // only for  A035VW01 



int32_t commands[] = {
    -1,     0x0011, -1,     0x0001, -1,     0x00c1, 0x01a8, 0x01b1, 
    0x0145, 0x0104,0x00b1, 0x0106, 0x0146,0x0105, 0x00c5, 0x0180, 0x016c, 0x00c6, 0x01bd, 0x0184, 
    0x00c7, 0x01bd, 0x0184, 0x00bd, 0x0102, 0x0011, -1,     0x0100, 
    0x0100, 0x0182, 0x0026, 0x0108, 0x00e0, 0x0100, 0x0104, 0x0108, 
    0x010b, 0x010c, 0x010d, 0x010e, 0x0100, 0x0104, 0x0108, 0x0113, 
    0x0114, 0x012f, 0x0129, 0x0124, 0x00e1, 0x0100, 0x0104, 0x0108, 
    0x010b, 0x010c, 0x0111, 0x010d, 0x010e, 0x0100, 0x0104, 0x0108, 
    0x0113, 0x0114, 0x012f, 0x0129, 0x0124, 0x0026, 0x0108, 0x00fd, 
    0x0100, 0x0108, 0x0029
};

void setup_pins(void)
{
    bcm2835_gpio_fsel(CLK, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(MOSI, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(CS, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(CS, HIGH);
}

void send_bits(uint16_t data, uint16_t count){
    int x;
    int mask = 1 << (count-1);
    for(x = 0; x < count; x++){
        bcm2835_gpio_write(MOSI, (data & mask) > 0);
        data <<= 1;

        bcm2835_gpio_write(CLK, LOW);
        bcm2835_delayMicroseconds(DELAY);
        bcm2835_gpio_write(CLK, HIGH);
        bcm2835_delayMicroseconds(DELAY);
    }
    bcm2835_gpio_write(MOSI, LOW);
}



void write(uint16_t command, uint8_t count){
    bcm2835_gpio_fsel(CLK, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(CS, LOW);
    send_bits(command, count);
    bcm2835_gpio_write(CS, HIGH);
     bcm2835_gpio_fsel(CLK, BCM2835_GPIO_FSEL_ALT2);
}

void setup_lcd(void){
    int count = sizeof(commands) / sizeof(int32_t);
    int x;
    for(x = 0; x < count; x++){ int32_t command = commands[x];     if(command == -1){bcm2835_delay(WAIT);         continue; }        write((uint16_t)command,9);    }


for (int x = 0; x < 11; x++) {
    write(commands2[x], 16);
  }

}

int main(int argc, char **argv)
{
    if (!bcm2835_init())
      return 1;

    setup_pins();
    setup_lcd();
    
    bcm2835_close();
    
    return 0;

}


