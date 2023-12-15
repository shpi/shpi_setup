#include <bcm2835.h>
#include <stdio.h>
#include <time.h> 


#define CLK RPI_V2_GPIO_P1_03 // BCM 2
#define PCLK 1 // BMC0 dpi clock
#define MOSI RPI_V2_GPIO_P1_12 // reuse fan gpio
#define CS RPI_V2_GPIO_P1_35 // use backlight
#define DELAY 50 // clock pulse time in microseconds
#define WAIT 120 // wait time in milliseconds

#define PIN RPI_V2_GPIO_P1_35
#define PWM_CHANNEL 1




uint16_t commands2[] =  {0x0080, 0x01f8, 0x0246, 0x0305, 0x0b76, 0x0440, 0x0540, 0x0640, 0x0740, 0x0840, 0x0940, 0x00CA, 0x0A03};            // only for  A035VW01 


int32_t commands[] = {
    //0x0011, -1, 
    0x0001, -2,
    0x00c1, 0x01a8, 0x01b1, 0x0145, 0x0104,
    0x00b1, 0x0106, 0x0146, 0x0105,  //DE, Hblanking, Vblanking,  vsync negative hsync negative
    0x00c5, 0x0180, 0x016c, 
    0x00c6, 0x01bd,  0x0184, 
    0x00c7, 0x01bd, 0x0184, 
    0x00bd, 0x0102, 
    0x0011, -1,     
    0xF2,
    0x0100, 0x0100, 0x0182, 
    0x0026, 0x0108, 
    0x00e0, 0x0100, 0x0104, 0x0108, 0x010b, 0x010c, 0x0111, 0x010d, 0x010e, 0x0100, 0x0104, 0x0108, 0x0113, 0x0114, 0x012f, 0x0129, 0x0124, 
    0x00e1, 0x0100, 0x0104, 0x0108, 0x010b, 0x010c, 0x0111, 0x010d, 0x010e, 0x0100, 0x0104, 0x0108, 0x0113, 0x0114, 0x012f, 0x0129, 0x0124, 
    0x0026, 0x0108, 
    0x00fd, 0x0100, 0x0108,
    0x0029
};

/*
0x11, -1, //sleepout
0x01, -1,  //reset
0xC1, 0x1A8, 0x1B1, 0x145, 0x104, //vgh vgl
0xC5, 0x180, 0x16C, //vcomdc
0xC6, 0x1BD, 0x184, // gvdd gvss
0xC7, 0x1BD, 0x184, // ngvdd ngvss
0xBD, 0x102, // disable precharge
0x11, -1,  //sleepout
0xF2, 0x100, 0x100, 0x182, //gamma setting follow
0x26, 0x108, // gamma enable
0xE0, 0x100, 0x104, 0x108, 0x10B, 0x10C, 0x111,0x10D, 0x10E, 0x100, 0x104, 0x108, 0x113, 0x114, 0x12F, 0x129, 0x124, //positive gamma
0xE1, 0x100, 0x104, 0x108, 0x10B, 0x10C, 0x111, 0x10D, 0x10E, 0x100, 0x104, 0x108, 0x113, 0x114, 0x12F, 0x129, 0x124, //negative gamma
0x26, 0x108, //enable gamma
0xFD, 0x100, 0x108,  //enable 2 dot function
0x29 //display on


*/

void busy_wait(struct timespec start, long long wait_ns){
    struct timespec end;
    long long elapsed_ns;

    do {
        clock_gettime(CLOCK_MONOTONIC, &end);
        elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
    } while (elapsed_ns < wait_ns);
}




void setup_pins(void)
{
    bcm2835_gpio_fsel(8, BCM2835_GPIO_FSEL_ALT2);
    bcm2835_gpio_fsel(CLK, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(MOSI, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(CS, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(CS, HIGH);
}

void send_bits(uint16_t data, uint16_t count){
    struct timespec start_time;
    int x;
    int mask = 1 << (count-1);
    for(x = 0; x < count; x++){
        bcm2835_gpio_write(MOSI, (data & mask) > 0);
        data <<= 1;

        bcm2835_gpio_write(CLK, LOW);
        //bcm2835_delayMicroseconds(DELAY);
        
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        busy_wait(start_time,DELAY);

        bcm2835_gpio_write(CLK, HIGH);

        clock_gettime(CLOCK_MONOTONIC, &start_time);
        busy_wait(start_time, DELAY);
        //bcm2835_delayMicroseconds(DELAY);
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
    
    bcm2835_gpio_fsel(PCLK, BCM2835_GPIO_FSEL_OUTP);
    for (int x = 0; x < sizeof(commands2) / sizeof(commands2[0]); x++) { write(commands2[x], 16);  }
    

    for(x = 0; x < count; x++){ int32_t command = commands[x];    

      if(command == -1){bcm2835_delay(WAIT);         continue; } 
      if(command == -2){bcm2835_delay(30);         continue; } 


       write((uint16_t)command,9);    }

     bcm2835_gpio_fsel(CLK, BCM2835_GPIO_FSEL_ALT2);
     bcm2835_gpio_fsel(CS, BCM2835_GPIO_FSEL_ALT5);

     bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
     bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1);
     bcm2835_pwm_set_range(PWM_CHANNEL, 4095);
     bcm2835_pwm_set_data(PWM_CHANNEL, 4094);
     bcm2835_gpio_fsel(PCLK, BCM2835_GPIO_FSEL_ALT2);
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


