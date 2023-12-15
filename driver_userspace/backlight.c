#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#define BLT RPI_V2_GPIO_P1_35 


signed int commands[] =  {0,1,0,1,1,0,0,0,-1,1,0,0};

void writebl(signed int a){
 bcm2835_gpio_fsel(BLT, BCM2835_GPIO_FSEL_OUTP);
 bcm2835_gpio_clr(BLT);
 bcm2835_delayMicroseconds(40);
 if (a == 0) {bcm2835_delayMicroseconds(50);    }
 bcm2835_gpio_fsel(BLT, BCM2835_GPIO_FSEL_INPT);
 bcm2835_gpio_set_pud(BLT, BCM2835_GPIO_PUD_UP);

 if (a == 1) {bcm2835_delayMicroseconds(50); }
 bcm2835_delayMicroseconds(40);
}

void initbl(){
bcm2835_gpio_fsel(BLT, BCM2835_GPIO_FSEL_OUTP);
bcm2835_gpio_clr(BLT);
bcm2835_delayMicroseconds(4000);
bcm2835_gpio_fsel(BLT, BCM2835_GPIO_FSEL_INPT);
bcm2835_gpio_set_pud(BLT, BCM2835_GPIO_PUD_UP);

bcm2835_delayMicroseconds(120);
bcm2835_gpio_fsel(BLT, BCM2835_GPIO_FSEL_OUTP);
bcm2835_gpio_clr(BLT);
bcm2835_delayMicroseconds(500);
bcm2835_gpio_fsel(BLT, BCM2835_GPIO_FSEL_INPT);
bcm2835_gpio_set_pud(BLT, BCM2835_GPIO_PUD_UP);

bcm2835_delayMicroseconds(5);
}


int main(int argc, char **argv)
{

if( argc == 2 ) {
     int n = atoi(argv[1]);
   
    if (!bcm2835_init())
      return 1;


int errors = 1;
while (errors > 0) {

    for(int x = 0; x < 12; x++){writebl(commands[x]); }

int k;

for (int i =4; i >= 0; i--)
 {
  k = n >> i;
  writebl((k & 1)); 
 }
writebl(-1);
bcm2835_delayMicroseconds(10);
if (bcm2835_gpio_lev(BLT)) {errors++;} else {errors = 0;}
bcm2835_delayMicroseconds(900);
if (errors > 3) {initbl();}
}

}
bcm2835_gpio_fsel(BLT,BCM2835_GPIO_FSEL_OUTP);
bcm2835_gpio_write(BLT, HIGH);

    bcm2835_close();
    return 0;
}



