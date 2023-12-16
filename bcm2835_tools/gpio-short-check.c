#include <bcm2835.h>
#include <stdio.h>
#include <string.h>

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))
#define BCM2835_PAD_SLEW_RATE      (0x00 << 2)






int is_in_array(uint8_t *arr, int size, int value) {
    for (int i = 0; i < size; i++) {
        if (value == arr[i]) {
            return 1;
        }
    }
    return 0;
}


void PrintArray(uint8_t *data_arr, uint8_t data_length)
{
    while (data_length--)
    {
        printf("%d ", (uint8_t) *data_arr);
        *data_arr++;
    }
    printf("\n");
}


static int bcm2835_get_fsel(unsigned int pin) {

  volatile uint32_t *reg = bcm2835_gpio + BCM2835_GPFSEL0 / 4 + (pin / 10);
  uint32_t lsb = (pin % 10) * 3;
  uint32_t gpfsel = bcm2835_peri_read(reg);

  switch ((gpfsel >> lsb) & 7) {
  case 0:
    return BCM2835_GPIO_FSEL_INPT;
    break;
  case 1:
    return BCM2835_GPIO_FSEL_OUTP;
    break;
  case 2:
    return BCM2835_GPIO_FSEL_ALT5;
    break;
  case 3:
    return BCM2835_GPIO_FSEL_ALT4;
    break;
  case 4:
    return BCM2835_GPIO_FSEL_ALT0;
    break;
  case 5:
    return BCM2835_GPIO_FSEL_ALT1;
    break;
  case 6:
    return BCM2835_GPIO_FSEL_ALT2;
    break;
  case 7:
    return BCM2835_GPIO_FSEL_ALT3;
    break;
  }
  return -1;
}

int main(int argc, char **argv) {
  int i;
  int a;
  int arr_pointer;
  uint8_t functions[28];
  uint8_t drive_state[28];
  uint8_t pulled_low[28];
  uint8_t pulled_high[28];
  uint8_t shorted_vcc[28];
  uint8_t shorted_gnd[28];
  uint8_t shorted_pin[28];

  memset(pulled_low, 255, 28);
  memset(pulled_high, 255, 28);
  memset(shorted_vcc, 255, 28);
  memset(shorted_gnd, 255, 28);
  memset(shorted_pin, 255, 28);

  uint8_t silent = 0;

    // Check for -v argument
    for (int arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-s") == 0) {
            silent = 1;
        }
    }

  bcm2835_init();



    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, BCM2835_PAD_DRIVE_2mA);
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_46_53, BCM2835_PAD_DRIVE_2mA);
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_28_45, BCM2835_PAD_SLEW_RATE);
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_46_53, BCM2835_PAD_SLEW_RATE);
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_28_45, BCM2835_PAD_DRIVE_2mA);
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, BCM2835_PAD_SLEW_RATE);





  for (i = 0; i < 28; i++) {
    // printf("GPIO %d", i);
    // printf(" function %d \n", bcm2835_get_fsel(i));
    functions[i] = bcm2835_get_fsel(i);
    drive_state[i] = bcm2835_gpio_lev(i);
    bcm2835_gpio_fsel(i, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(i, BCM2835_GPIO_PUD_UP);
    /*
       BCM2835_GPIO_PUD_OFF     = 0x00,
       BCM2835_GPIO_PUD_DOWN    = 0x01,
       BCM2835_GPIO_PUD_UP      = 0x02
    */
  }
  arr_pointer = 0;

  for (i = 0; i < 28; i++) {
    if (bcm2835_gpio_lev(i) == 0) {
      if (!silent) printf("GPIO %d pulled low\n", i);
      pulled_low[arr_pointer] = i;
      arr_pointer++;
    } 

    bcm2835_gpio_set_pud(i, BCM2835_GPIO_PUD_DOWN);
  }

  arr_pointer = 0;

  for (i = 0; i < 28; i++) {
    if (bcm2835_gpio_lev(i) == 1) {
      if (!silent) printf("GPIO %d pulled high\n", i);
      pulled_high[arr_pointer] = i;
      arr_pointer++;
    }
  }

  arr_pointer = 0;

  for (i = 0; i < 28; i++) {
    if (pulled_high[i] < 255) {
      bcm2835_gpio_fsel(pulled_high[i], BCM2835_GPIO_FSEL_OUTP);
      bcm2835_gpio_clr(pulled_high[i]);

      if (bcm2835_gpio_lev(pulled_high[i]) == 1) {
        bcm2835_gpio_fsel(pulled_high[i], BCM2835_GPIO_FSEL_INPT);
        printf("!WARNING!  GPIO %d shorted to VCC!\n", pulled_high[i]);
        shorted_vcc[arr_pointer] = pulled_high[i];
        arr_pointer++;
      }
      bcm2835_gpio_fsel(pulled_high[i], BCM2835_GPIO_FSEL_INPT);
    }
  }

  arr_pointer = 0;

  for (i = 0; i < 28; i++) {
    if (pulled_low[i] < 255) {
      bcm2835_gpio_fsel(pulled_low[i], BCM2835_GPIO_FSEL_OUTP);
      bcm2835_gpio_set(pulled_low[i]);

      if (bcm2835_gpio_lev(pulled_low[i]) == 0) {
        bcm2835_gpio_fsel(pulled_low[i], BCM2835_GPIO_FSEL_INPT);
        printf("GPIO %d shorted to GND!\n", pulled_low[i]);
        shorted_gnd[arr_pointer] = pulled_low[i];
        arr_pointer++;
      }
      bcm2835_gpio_fsel(pulled_low[i], BCM2835_GPIO_FSEL_INPT);
    }
  }

  arr_pointer = 0;

  for (i = 0; i < 28; i++) {

    if (!is_in_array(shorted_gnd,sizeof(shorted_gnd),i)) {
    if (!is_in_array(shorted_vcc,sizeof(shorted_vcc),i)) {
      bcm2835_gpio_fsel(i, BCM2835_GPIO_FSEL_OUTP);

      for (a = 0; a < 28; a++) {

        if (a != i) {
          bcm2835_gpio_set(i);
          bcm2835_gpio_fsel(a, BCM2835_GPIO_FSEL_INPT);
          bcm2835_gpio_set_pud(a, BCM2835_GPIO_PUD_DOWN);

          if (bcm2835_gpio_lev(a) == bcm2835_gpio_lev(i)) {

            bcm2835_gpio_fsel(i, BCM2835_GPIO_FSEL_OUTP);
            bcm2835_gpio_clr(i);
            bcm2835_gpio_fsel(a, BCM2835_GPIO_FSEL_INPT);
            bcm2835_gpio_set_pud(a, BCM2835_GPIO_PUD_UP);

            if (bcm2835_gpio_lev(a) == bcm2835_gpio_lev(i)) {
              shorted_pin[arr_pointer] = a;
              arr_pointer++;
              shorted_pin[arr_pointer] = i;
              arr_pointer++;
              printf("GPIO %d shorted to %d\n", i, a);
            }
          }
        }
      }
    }}

    bcm2835_gpio_fsel(i, BCM2835_GPIO_FSEL_INPT);
  }

  for (i = 0; i < 28; i++) {

    if (!is_in_array(shorted_gnd,sizeof(shorted_gnd),i) &&
        !is_in_array(shorted_vcc,sizeof(shorted_vcc),i) &&
        !is_in_array(shorted_pin,sizeof(shorted_pin),i)) {
    bcm2835_gpio_fsel(i, functions[i]);
    if (drive_state[i] == HIGH && functions[i] == BCM2835_GPIO_FSEL_INPT)  bcm2835_gpio_set_pud(i, BCM2835_GPIO_PUD_UP);  else  bcm2835_gpio_set_pud(i, BCM2835_GPIO_PUD_DOWN); 

    if (drive_state[i] == HIGH && functions[i] == BCM2835_GPIO_FSEL_OUTP) bcm2835_gpio_set(i); else bcm2835_gpio_clr(i);

   }
    else bcm2835_gpio_fsel(i, BCM2835_GPIO_FSEL_INPT);
  }

  bcm2835_close();

  return 0;
}
