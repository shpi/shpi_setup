#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
 
#define MODE_READ 0
#define MODE_WRITE 1
#define MAX_LEN 32
char wbuf[MAX_LEN];
 
typedef enum {
    NO_ACTION,
    I2C_BEGIN,
    I2C_END
} i2c_init;
 
uint8_t  init = NO_ACTION;
uint16_t clk_div = BCM2835_I2C_CLOCK_DIVIDER_2500; //BCM2835_I2C_CLOCK_DIVIDER_148;
uint8_t slave_address = 0x00;
uint32_t len = 0;
uint8_t  mode = MODE_READ;
 
 
int comparse(int argc, char **argv) {
    int argnum, i, xmitnum;
        
    if (argc < 2) {  // must have at least program name and len arguments
                     // or -ie (I2C_END) or -ib (I2C_BEGIN)
        fprintf(stderr, "Insufficient command line arguments\n");
        return EXIT_FAILURE;
    }
    
    argnum = 1;
    while (argnum < argc && argv[argnum][0] == '-') {
 
        switch (argv[argnum][1]) {
 
            case 'i':  // I2C init
                switch (argv[argnum][2]) {
                    case 'b': init = I2C_BEGIN; break;
                    case 'e': init = I2C_END; break;
                    default:
                        fprintf(stderr, "%c is not a valid init option\n", argv[argnum][2]);
                        return EXIT_FAILURE;
                }
                break;
 
            case 'd':  // Read/Write Mode
                switch (argv[argnum][2]) {
                    case 'r': mode = MODE_READ; break;
                    case 'w': mode = MODE_WRITE; break;
                    default:
                        fprintf(stderr, "%c is not a valid init option\n", argv[argnum][2]);
                        return EXIT_FAILURE;
                }
                break;
 

            case 's':  // Slave address
               slave_address = (uint8_t)strtol(argv[argnum]+2, NULL, 16);
               printf("I2C: 0x%02X, ", slave_address);
               break;
 
            default:
                fprintf(stderr, "%c is not a valid option\n", argv[argnum][1]);
                return EXIT_FAILURE;
        }
 
        argnum++;   // advance the argument number
 
    }
 
    // If command is used for I2C_END or I2C_BEGIN only
    if (argnum == argc && init != NO_ACTION) // no further arguments are needed
        return EXIT_SUCCESS;
        
    // Get len
    if (strspn(argv[argnum], "0123456789") != strlen(argv[argnum])) {
        fprintf(stderr, "Invalid number of bytes specified\n");
        return EXIT_FAILURE;
    }
    
    len = atoi(argv[argnum]);
 
    if (len > MAX_LEN) {
        fprintf(stderr, "Invalid number of bytes specified\n");
        return EXIT_FAILURE;
    }
    
    argnum++;   // advance the argument number
 
    xmitnum = argc - argnum;    // number of xmit bytes
 
    memset(wbuf, 0, sizeof(wbuf));
 
    for (i = 0; i < xmitnum; i++) {
        if (strspn(argv[argnum + i], "0123456789abcdefABCDEFxX") != strlen(argv[argnum + i])) {
            fprintf(stderr, "Invalid data: ");
            fprintf(stderr, "%d \n", xmitnum);
            return EXIT_FAILURE;
        }
        wbuf[i] = (char)strtoul(argv[argnum + i], NULL, 0);
    }
 
    return EXIT_SUCCESS;
}
 
//*******************************************************************************
//  showusage: Print the usage statement and return errcode.
//*******************************************************************************
int showusage(int errcode) {
    printf("i2c error\n");
    return errcode;
}
 
char buf[MAX_LEN];
int i;
uint8_t data;
 
int main(int argc, char **argv) {
 
    
    // parse the command line
    if (comparse(argc, argv) == EXIT_FAILURE) return showusage (EXIT_FAILURE);
 
    if (!bcm2835_init())
    {
      printf("bcm2835_init failed. Are you running as root??\n");
      return 1;
    }
      
    // I2C begin if specified    
    if (init == I2C_BEGIN)
    {
      if (!bcm2835_i2c_begin())
      {
        printf("bcm2835_i2c_begin failed. Are you running as root??\n");
        return 1;
      }
    }
          
 
    // If len is 0, no need to continue, but do I2C end if specified
    if (len == 0) {
         if (init == I2C_END) bcm2835_i2c_end();
         return EXIT_SUCCESS;
    }
 
    bcm2835_i2c_setSlaveAddress(slave_address);
    bcm2835_i2c_setClockDivider(clk_div);
    //fprintf(stderr, "Clock divider set to: %d\n", clk_div);
    //fprintf(stderr, "len set to: %d\n", len);
    //fprintf(stderr, "Slave address set to: %d\n", slave_address);   
    
    if (mode == MODE_READ) {
        for (i=0; i<MAX_LEN; i++) buf[i] = 'n';
        data = bcm2835_i2c_read(buf, len);
        if (data == 0) printf("ok");
        else printf(" not found (%d)", data);   
        for (i=0; i<MAX_LEN; i++) {
                if(buf[i] != 'n') printf(" [%d] = %x ", i, buf[i]);
        }    
    }
    if (mode == MODE_WRITE) {
        data = bcm2835_i2c_write(wbuf, len);
        printf(" %d\n", data);
    }   
 
    // This I2C end is done after a transfer if specified
    if (init == I2C_END) bcm2835_i2c_end();   
    printf("\n");

    bcm2835_close();
    return data;
}
