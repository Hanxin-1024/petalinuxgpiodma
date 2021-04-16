#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "gpioapp.h"

#define DATA_LEN    1024

int gpio_fd;
int gpio_fd1;
int gpio_fd2;
int gpio_fd3;
int gpio_fd4;
int gpio_fd5;
int gpio_fd6;
int gpio_fd7;


static int gpio_export(int pin);
static int gpio_unexport(int pin);
static int gpio_direction(int pin, int dir);
static int gpio_write(int pin, int value);
static int gpio_read(int pin);
static int gpio_edge(int pin, int edge);




void XBram_Out32(unsigned int * Addr, unsigned int Value)
{
	volatile unsigned int *LocalAddr = (volatile unsigned int *)Addr;
	*LocalAddr = Value;
}
 unsigned int * XBram_In32(unsigned int * Addr)
{
	return *(volatile unsigned int *) Addr;
}
static int gpio_export(int pin)  
{  
    char buffer[64];  
    int len;  
    int fd;  
  
    fd = open("/sys/class/gpio/export", O_WRONLY);  
    if (fd < 0) 
    {  
        MSG("Failed to open export for writing!\n");  
        return(-1);  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin);  
    printf("%s,%d,%d\n",buffer,sizeof(buffer),len);
    if (write(fd, buffer, len) < 0) 
    {  
        MSG("Failed to export gpio!");  
        return -1;  
    }  
     
    close(fd);  
    return 0;  
}  
static int gpio_unexport(int pin)  
{  
    char buffer[64];  
    int len;  
    int fd;  
  
    fd = open("/sys/class/gpio/unexport", O_WRONLY);  
    if (fd < 0) 
    {  
        MSG("Failed to open unexport for writing!\n");  
        return -1;  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin);  
    if (write(fd, buffer, len) < 0) 
    {  
        MSG("Failed to unexport gpio!");  
        return -1;  
    }  
     
    close(fd);  
    return 0;  
} 
//dir: 0输入, 1输出
static int gpio_direction(int pin, int dir)  
{  
    static const char dir_str[] = "in\0out";  
    char path[64];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);  
    fd = open(path, O_WRONLY);  
    if (fd < 0) 
    {  
        MSG("Failed to open gpio direction for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0) 
    {  
        MSG("Failed to set direction!\n");  
        return -1;  
    }  
  
    close(fd);  
    return 0;  
}  
//value: 0-->LOW, 1-->HIGH
static int gpio_write(int pin, int value)  
{  
    static const char values_str[] = "01";  
    char path[64];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);  
    fd = open(path, O_WRONLY);  
    if (fd < 0) 
    {  
        MSG("Failed to open gpio value for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &values_str[value == 0 ? 0 : 1], 1) < 0) 
    {  
        MSG("Failed to write value!\n");  
        return -1;  
    }  
  
    close(fd);  
    return 0;  
}
static int gpio_read(int pin)  
{  
    char path[64];  
    char value_str[3];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);  
    fd = open(path, O_RDONLY);  
    if (fd < 0) 
    {  
        MSG("Failed to open gpio value for reading!\n");  
        return -1;  
    }  
  
    if (read(fd, value_str, 3) < 0)
    {  
        MSG("Failed to read value!\n");  
        return -1;  
    }  
  
    close(fd);  
    return (atoi(value_str));
}  
// none表示引脚为输入，不是中断引脚
// rising表示引脚为中断输入，上升沿触发
// falling表示引脚为中断输入，下降沿触发
// both表示引脚为中断输入，边沿触发
// 0-->none, 1-->rising, 2-->falling, 3-->both
static int gpio_edge(int pin, int edge)
{
const char dir_str[] = "none\0rising\0falling\0both"; 
char ptr;
char path[64];  
    int fd; 
switch(edge)
{
    case 0:
        ptr = 0;
        break;
    case 1:
        ptr = 5;
        break;
    case 2:
        ptr = 12;
        break;
    case 3:
        ptr = 20;
        break;
    default:
        ptr = 0;
} 
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", pin);  
    fd = open(path, O_WRONLY);  
    if (fd < 0) 
    {  
        MSG("Failed to open gpio edge for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &dir_str[ptr], strlen(&dir_str[ptr])) < 0) 
    {  
        MSG("Failed to set edge!\n");  
        return -1;  
    }  
  
    close(fd);  
    return 0;  
}
int GpioInit()
{

    // gpio_unexport(958);
    // gpio_unexport(959);
    gpio_unexport(960);
    gpio_unexport(961);
    gpio_unexport(962);
    gpio_unexport(963);
    gpio_unexport(964);
    gpio_unexport(965);
    gpio_unexport(966);
    gpio_unexport(967);

    /*********************rx handle**************/ 

    gpio_export(960);
    gpio_direction(960, 0);//
    gpio_edge(960,1);
 
    gpio_fd = open("/sys/class/gpio/gpio960/value",O_RDONLY);
    if(gpio_fd < 0)
    {
        MSG("Failed to open value2!\n");  
        return -1;  
    }
    else
    printf("success open960\r\n");

    gpio_export(961);
    gpio_direction(961, 0);//
    gpio_edge(961,1);
 
    gpio_fd1 = open("/sys/class/gpio/gpio961/value",O_RDONLY);
    if(gpio_fd1 < 0)
    {
        MSG("Failed to open value3!\n");  
        return -1;  
    }
    else
    printf("success open961\r\n");

    gpio_export(962);
    gpio_direction(962, 0);//
    gpio_edge(962,1);
 
    gpio_fd2 = open("/sys/class/gpio/gpio962/value",O_RDONLY);
    if(gpio_fd2 < 0)
    {
        MSG("Failed to open value4!\n");  
        return -1;  
    }
    else
    printf("success open962\r\n");

    gpio_export(963);
    gpio_direction(963, 0);//
    gpio_edge(963,1);
 
    gpio_fd3 = open("/sys/class/gpio/gpio963/value",O_RDONLY);
    if(gpio_fd3 < 0)
    {
        MSG("Failed to open value5!\n");  
        return -1;  
    }
    else
    printf("success open963\r\n");

    gpio_export(964);
    gpio_direction(964, 0);//
    gpio_edge(964,1);
 
    gpio_fd4 = open("/sys/class/gpio/gpio964/value",O_RDONLY);
    if(gpio_fd4 < 0)
    {
        MSG("Failed to open value6!\n");  
        return -1;  
    }
    else
    printf("success open964\r\n");

    gpio_export(965);
    gpio_direction(965, 0);//
    gpio_edge(965,1);
 
    gpio_fd5 = open("/sys/class/gpio/gpio965/value",O_RDONLY);
    if(gpio_fd5 < 0)
    {
        MSG("Failed to open value7!\n");  
        return -1;  
    }
    else
    printf("success open965\r\n");

    gpio_export(966);
    gpio_direction(966, 0);//
    gpio_edge(966,1);
 
    gpio_fd6 = open("/sys/class/gpio/gpio966/value",O_RDONLY);
    if(gpio_fd6 < 0)
    {
        MSG("Failed to open value6!\n");  
        return -1;  
    }
    else
    printf("success open966\r\n");

    gpio_export(967);
    gpio_direction(967, 0);//
    gpio_edge(967,1);
 
    gpio_fd7 = open("/sys/class/gpio/gpio967/value",O_RDONLY);
    if(gpio_fd7 < 0)
    {
        MSG("Failed to open value7!\n");  
        return -1;  
    }
    else
    printf("success open967\r\n");

    
}

