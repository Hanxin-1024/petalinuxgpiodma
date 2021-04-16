
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define MSG(args...) printf(args) 
#define BRAM_MAX_SIZE    2048
#define DATA_LEN    1024

int GpioInit();
// int BramInit();


