// Access from ARM Running Linux
#define BCM2708_PERI_BASE        0x3F000000  //Base address for peripherals
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) //base address for gpio pins


#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

int  shutdown = 0;
int  mem_fd;//File descriptor to dev/mem

volatile unsigned *mem_map;//peripherals virtual address pointer

//base-7e000000 gives the offset , offset + mem_map gives the required location
#define ACCESS(base) *(volatile int*)((int)mem_map+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit //sets the specified bit in the specified location
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)//clears the specified bit in the specified location

#define CM_GP0CTL (0x7e101070) //virtual address for gpio clock control
#define GPFSEL0 (0x7E200000) //virtual address for gpio pins
#define CM_GP0DIV (0x7e101074) //virtual address for clock divisor register

struct GPCTL {
    char SRC         : 4; //clock source(which clock source to select)
    char ENAB        : 1; //enable clock
    char KILL        : 1; //stops the clock
    char             : 1;
    char BUSY        : 1; //indicates that clock generator is running
    char FLIP        : 1; //inverts the clock generator output
    char MASH        : 2; // ..
    unsigned int     : 13;
    char PASSWD      : 8; //0x5a
};

//selects pin4 to clock mode and starts the clock
void setup_fm(int state)
{
    //open returns the file descriptor of the path given
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit (-1);
    }

    //change later
    //returns the array pointer to virtual address of 3F000000 in file discriptor mem_fd and the following locations(of size len)
    mem_map = (unsigned *)mmap(
                  NULL,       // if NULL is specified the array returned is at some random location
                  0x01000000,  //len
                  PROT_READ|PROT_WRITE, //mode of the .....
                  MAP_SHARED,
                  mem_fd,
                  0x3F000000  //base address for peripherals
              );
    //printf("mem_map: %d\n",(int)mem_map);
    if ((int)mem_map==-1) exit(-1);//if incorrect file discriptor or location

    // GPIO 4 selecting alt func 0 -> 1-0-0 bits to pin 14-13-12 : FSEL4 -> clock
    SETBIT(GPFSEL0 , 14); //sets the 14th bit of GPFSEL0
    CLRBIT(GPFSEL0 , 13); //clears the 13th bit of GPFSEL0
    CLRBIT(GPFSEL0 , 12); //clears the 12th bit of GPFSEL0



    // 6 -> PLLD clock 500MHz , ENABLE
    struct GPCTL setupword = {6, state, 0, 0, 0, state,0x5a};
    printf("\nsetupword(GPCTL) = %d\n", *((int*)&setupword));
    ACCESS(CM_GP0CTL) = *((int*)&setupword); //changes the according  bits of CM_GP0CTL given by setupword
}

//shutdowns after
void shutdown_fm()
{
    // printf("in shutdown mem_map = %d\n",(int)mem_map);
    if(!shutdown){
        shutdown=1;
        printf("\nShutting Down\n");
        setup_fm(0);// to shutdown the fm
        exit(0);
    }
}


void modulate(int m,int mod)
{
    ACCESS(CM_GP0DIV) = (0x5a << 24) + mod + m; //fm modulation where mod is  DIVI value and m is DIVF value

}

void playWav(char* filename,int mod,float bw)
{
    int fp;int intval;
    fp = open(filename, 'r');
    lseek(fp, 22, SEEK_SET); //Skip the header of wav file 44 bytes
    short* data = (short*)malloc(2048);


    while (read(fp, data, 2048))  { // reading data from .wav file 2048 bytes at a time
	    for (int j=0; j<2048/2; j++){ // divide by 2 because data is short(2 Bytes)
            	float dval = (float)(data[j])/65536.0*bw; //normalizing the data in wav file
            	intval = (int)(floor(dval));
            	for (int i=0; i<300; i++) {
              		modulate(intval,mod); //changes the frequency according to intval and mod
            	}
	    }
    }
    close(fp);
}


int main(int argc, char **argv)
{
        //stops the clock after transmission
    	signal(SIGTERM, &shutdown_fm);
    	signal(SIGINT, &shutdown_fm);
   	    atexit(&shutdown_fm);
    	setup_fm(1);//sets state

	// argv[1] - filename, argv[2]-carrier frequency,argv[3]-bandwidth
    	float freq = atof(argv[2]);
    	float bw;
    	int mod = (500/freq)*4096; //this is the divi value leftshifted by 12 bits(DIVI is CM_GP0DIV[23:12])
    	modulate(0,mod); // generates clock with carrier frequency
    	if (argc==3){
      		bw = 8.0;
     	    playWav(argv[1],mod,bw);// if bandwidth is not given by default bw=8
    	}else if (argc==4){
      		bw = atof(argv[3]);
      		playWav(argv[1],mod,bw);
    	}else{
		printf("Arguments not given");
	}
}
