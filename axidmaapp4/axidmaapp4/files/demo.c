/**
 * @file axidma_transfer.c
 * @date Sunday, April 1, 2021 at 12:23:43 PM EST
 * @author Brandon Perez (bmperez)
 * @author Jared Choi (jaewonch)
 * @author Xin Han (hicx)
 *
 * This program performs a simple AXI DMA transfer. It takes the input data,
 * loads it into memory, and then sends it out over the PL fabric. It then
 * receives the data back, and places it into the given output .
 *
 * By default it uses the lowest numbered channels for the transmit and receive,
 * unless overriden by the user. The amount of data transfered is automatically
 * determined from the file size. Unless specified, the output file size is
 * made to be 2 times the input size (to account for creating more data).
 *
 * This program also handles any additional channels that the pipeline
 * on the PL fabric might depend on. It starts up DMA transfers for these
 * pipeline stages, and discards their results.
 *
 * @bug No known bugs.
 **/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <fcntl.h>    
#include <sys/mman.h>          // Flags for open()
#include <sys/stat.h>           // Open() system call
#include <sys/types.h>          // Types for open()
#include <unistd.h>             // Close() system call
#include <string.h>             // Memory setting and copying
#include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes
#include <string.h>
#include <poll.h>
#include "util.h"               // Miscellaneous utilities
#include "conversion.h"         // Convert bytes to MiBs
#include "axidmaapp.h"          // Interface ot the AXI DMA library
#include <pthread.h>
#include "gpioapp.h"
#define MAXLENGTH 10240
#define TESTLENGTH    8192
static unsigned char rbuffer0[MAXLENGTH] = {0};
static unsigned char rbuffer1[MAXLENGTH] = {0};
static unsigned char rbuffer2[MAXLENGTH] = {0};
static unsigned char rbuffer3[MAXLENGTH] = {0};
static unsigned char sbuffer0[MAXLENGTH] = {0};
static unsigned char sbuffer1[MAXLENGTH] = {0};
static unsigned char sbuffer2[MAXLENGTH] = {0};
static unsigned char sbuffer3[MAXLENGTH] = {0};
static unsigned char tbuffer[MAXLENGTH] = {0};

extern gpio_fd;
extern gpio_fd1;
extern gpio_fd2;
extern gpio_fd3;
extern gpio_fd4;
extern gpio_fd5;
extern gpio_fd6;
extern gpio_fd7;
axidma_dev_t axidma_dev;
struct dma_transfer trans;
struct dma_transfer trans0;
struct dma_transfer trans1;
struct dma_transfer trans2;
struct dma_transfer trans3;
// Prints the usage for this program
static void print_usage(bool help)
{
    FILE* stream = (help) ? stdout : stderr;

    fprintf(stream, "Usage: axidma_transfer  "
            "[-t <DMA tx channel>] [-r <DMA rx channel>] [-s <Output file size>"
            " | -o <Output file size>].\n");
    if (!help) {
        return;
    }

    // fprintf(stream, "\t<input path>:\t\tThe path to file to send out over AXI "
    //         "DMA to the PL fabric. Can be a relative or absolute path.\n");
    // fprintf(stream, "\t<output path>:\t\tThe path to place the received data "
    //         "from the PL fabric into. Can be a relative or absolute path.\n");
    fprintf(stream, "\t-t <DMA tx channel>:\tThe device id of the DMA channel "
            "to use for transmitting the file. Default is to use the lowest "
            "numbered channel available.\n");
    fprintf(stream, "\t-r <DMA rx channel>:\tThe device id of the DMA channel "
            "to use for receiving the data from the PL fabric. Default is to "
            "use the lowest numbered channel available.\n");
    fprintf(stream, "\t-s <Output file size>:\tThe size of the output file in "
            "bytes. This is an integer value that must be at least the number "
            "of bytes received back. By default, this is the same as the size "
            "of the input file.\n");
    fprintf(stream, "\t-o <Output file size>:\tThe size of the output file in "
            "Mibs. This is a floating-point value that must be at least the "
            "number of bytes received back. By default, this is the same "
            "the size of the input file.\n");
    return;
}

/* Parses the command line arguments overriding the default transfer sizes,
 * and number of transfer to use for the benchmark if specified. */
static int parse_args(int argc, char **argv,  int *input_channel, int *output_channel, int *output_size)
{
    char option;
    int int_arg;
    double double_arg;
    bool o_specified, s_specified;
    int rc;

    // Set the default values for the arguments
    *input_channel = -1;
    *output_channel = -1;
    *output_size = -1;
    o_specified = false;
    s_specified = false;
    rc = 0;

    while ((option = getopt(argc, argv, "t:r:s:o:h")) != (char)-1)
    {
        switch (option)
        {
            // Parse the transmit channel device id
            case 't':
                rc = parse_int(option, optarg, &int_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *input_channel = int_arg;
                break;

            // Parse the receive channel device id
            case 'r':
                rc = parse_int(option, optarg, &int_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *output_channel = int_arg;
                break;

            // Parse the output file size (in bytes)
            case 's':
                rc = parse_int(option, optarg, &int_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *output_size = int_arg;
                s_specified = true;
                break;

            // Parse the output file size (in MiBs)
            case 'o':
                rc = parse_double(option, optarg, &double_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *output_size = MIB_TO_BYTE(double_arg);
                o_specified = true;
                break;

            case 'h':
                print_usage(true);
                exit(0);

            default:
                print_usage(false);
                return -EINVAL;
        }
    }

    // If one of -t or -r is specified, then both must be
    if ((*input_channel == -1) ^ (*output_channel == -1)) {
        fprintf(stderr, "Error: Either both -t and -r must be specified, or "
                "neither.\n");
        print_usage(false);
        return -EINVAL;
    }

    // Only one of -s and -o can be specified
    if (s_specified && o_specified) {
        fprintf(stderr, "Error: Only one of -s and -o can be specified.\n");
        print_usage(false);
        return -EINVAL;
    }

    // // Check that there are enough command line arguments
    // if (optind > argc-2) {
    //     fprintf(stderr, "Error: Too few command line arguments.\n");
    //     print_usage(false);
    //     return -EINVAL;
    // }

    // Check if there are too many command line arguments remaining
    if (optind < argc-2) {
        fprintf(stderr, "Error: Too many command line arguments.\n");
        print_usage(false);
        return -EINVAL;
    }

    // Parse out the input and output paths
    // *input_path = argv[optind];
    // *output_path = argv[optind+1];
    return 0;
}

//receive
void *rapidio_taks_rec(void *arg)
{
    
    // printf("r___________________________________________________________________");
    int ret = 0,i,err_num;
    unsigned int rec_len = 0;
    struct pollfd fds[1];
    char buff[10];
    static cnt = 0;
   
    fds[0].fd = gpio_fd1;
    fds[0].events  = POLLPRI;
    trans0.input_channel = 0;
    trans0.output_channel = 1;
    printf("AXI DMA0 File Transfer Info:\n");
    printf("\tTransmit Channel: %d\n", trans0.input_channel);
    printf("\tReceive Channel: %d\n", trans0.output_channel); 


    ret = read(gpio_fd1,buff,10);
    if( ret == -1 )
        MSG("read\n");

    while(1)
    {
      ret = poll(fds,1,-1);
      if( ret == -1 )
          MSG("poll\n");
      if( fds[0].revents & POLLPRI)
      {
        ret = lseek(gpio_fd1,0,SEEK_SET);
        if( ret == -1 )
            MSG("lseek\n");
        ret = read(gpio_fd1,buff,10);
        if( ret == -1 )
            MSG("read\n");

    //    printf("\n--------------------------------------------------------------------------------\n");
        rec_len = rapidio_jm_read(axidma_dev, &trans0, rbuffer0);
    //     XBram_Out32(map_base0+8,0x1);
    // //   usleep(15);
    //     XBram_Out32(map_base0+8,0x0);
        cnt++;
        if(rec_len > MAXLENGTH)
	    {
	     printf("gkhy_debug : DMA0 recv len error10240 \n");
	     continue;
	    }
        // rapidio_jm_send(axidma_dev, &trans0, rbuffer0);
        if(cnt%1000 == 0)
	     {
	       printf("\nDMA0 rec_len = 0x%x,cnt = %d\n",rec_len,cnt);
	     }
        // printf("\nrec_len = 0x%x,cnt=%d\n",rec_len,cnt);
        for(i=0;i<rec_len;i++)
	    {
		    if(rbuffer0[i] != tbuffer[i])
		    {
			//   printf("khy_debug :tbuffer[%d] : 0x%x,	rbuffer0[%d] : 0x%x\n",i,tbuffer[i],i,rbuffer0[i]);
			  err_num++;
		    }
	    }   
            if(err_num != 0)
            {
              printf("gkhy_debug:err_num = %d\n",err_num);
              err_num = 0;
            }
        //      if(cnt == 100000)
        //     {
        //       printf("gkhy_debug:cnt = %d\n",cnt);
        //       return 0;
        //     }
        // for(i = 0;i<(rec_len);i++)
        //   {
        //     if(i%16 == 0)
        //     {
        //         printf("\n");
        //     }
        //     printf("0x%02x ",rbuffer[i]);
        //   }
     
      }
    else
    printf("poll nothing--------------------------\n");
   }


   pthread_exit(0);
}
void *rapidio_taks_rec1(void *arg)
{
    
    // printf("r___________________________________________________________________");
    int ret = 0,i,err_num;
    unsigned int rec_len = 0;
    struct pollfd fds[1];
    char buff[10];
    static cnt = 0;
  

    fds[0].fd = gpio_fd3;
    fds[0].events  = POLLPRI;
    trans1.input_channel = 2;
    trans1.output_channel = 3;
    printf("AXI DMA1 File Transfer Info:\n");
    printf("\tTransmit Channel: %d\n", trans1.input_channel);
    printf("\tReceive Channel: %d\n", trans1.output_channel); 

    ret = read(gpio_fd3,buff,10);
    if( ret == -1 )
        MSG("read\n");

    while(1)
    {
      ret = poll(fds,1,-1);
      if( ret == -1 )
          MSG("poll\n");
      if( fds[0].revents & POLLPRI)
      {
        ret = lseek(gpio_fd3,0,SEEK_SET);
        if( ret == -1 )
            MSG("lseek\n");
        ret = read(gpio_fd3,buff,10);
        if( ret == -1 )
            MSG("read\n");

    //    printf("\n--------------------------------------------------------------------------------\n");
        rec_len = rapidio_dx_read(axidma_dev, &trans1, rbuffer1);
    //     XBram_Out32(map_base0+8,0x1);
    // //   usleep(15);
    //     XBram_Out32(map_base0+8,0x0);
        cnt++;
        if(rec_len > MAXLENGTH)
	    {
	     printf("gkhy_debug : DMA1 recv len error10240 \n");
	     continue;
	    }
        // rapidio_dx_send(axidma_dev, &trans1, rbuffer1);
        if(cnt%1000 == 0)
	     {
	       printf("\nDMA1 rec_len = 0x%x,cnt = %d\n",rec_len,cnt);
	     }
        // printf("\nrec_len = 0x%x,cnt=%d\n",rec_len,cnt);
        for(i=0;i<rec_len;i++)
	    {
		    if(rbuffer1[i] != tbuffer[i])
		    {
			  printf("khy_debug :tbuffer[%d] : 0x%x,	rbuffer1[%d] : 0x%x\n",i,tbuffer[i],i,rbuffer1[i]);
			  err_num++;
		    }
	    }   
            if(err_num != 0)
            {
              printf("gkhy_debug:err_num = %d\n",err_num);
              err_num = 0;
            }
        //      if(cnt == 100000)
        //     {
        //       printf("gkhy_debug:cnt = %d\n",cnt);
        //       return 0;
        //     }
        // for(i = 0;i<(rec_len);i++)
        //   {
        //     if(i%16 == 0)
        //     {
        //         printf("\n");
        //     }
        //     printf("0x%02x ",rbuffer[i]);
        //   }
     
      }
    else
    printf("poll nothing--------------------------\n");
   }


   pthread_exit(0);
}
void *rapidio_taks_rec2(void *arg)
{
    
    // printf("r___________________________________________________________________");
    int ret = 0,i,err_num;
    unsigned int rec_len = 0;
    struct pollfd fds[1];
    char buff[10];
    static cnt = 0;
  

    fds[0].fd = gpio_fd5;
    fds[0].events  = POLLPRI;
    trans2.input_channel = 4;
    trans2.output_channel = 5;
    printf("AXI DMA2 File Transfer Info:\n");
    printf("\tTransmit Channel: %d\n", trans2.input_channel);
    printf("\tReceive Channel: %d\n", trans2.output_channel); 

    ret = read(gpio_fd5,buff,10);
    if( ret == -1 )
        MSG("read\n");

    while(1)
    {
      ret = poll(fds,1,-1);
      if( ret == -1 )
          MSG("poll\n");
      if( fds[0].revents & POLLPRI)
      {
        ret = lseek(gpio_fd5,0,SEEK_SET);
        if( ret == -1 )
            MSG("lseek\n");
        ret = read(gpio_fd5,buff,10);
        if( ret == -1 )
            MSG("read\n");

    //    printf("\n--------------------------------------------------------------------------------\n");
        rec_len = rapidio_dd_read(axidma_dev, &trans2, rbuffer2);
    //     XBram_Out32(map_base0+8,0x1);
    // //   usleep(15);
    //     XBram_Out32(map_base0+8,0x0);
        cnt++;
        if(rec_len > MAXLENGTH)
	    {
	     printf("gkhy_debug : DMA2 recv len error10240 \n");
	     continue;
	    }
        // rapidio_dd_send(axidma_dev, &trans2, rbuffer2);
        if(cnt%1000 == 0)
	     {
	       printf("\nDMA2 rec_len = 0x%x,cnt = %d\n",rec_len,cnt);
	     }
        // printf("\nrec_len = 0x%x,cnt=%d\n",rec_len,cnt);
        for(i=0;i<rec_len;i++)
	    {
		    if(rbuffer2[i] != tbuffer[i])
		    {
			  printf("khy_debug :tbuffer[%d] : 0x%x,	rbuffer[%d] : 0x%x\n",i,tbuffer[i],i,rbuffer2[i]);
			  err_num++;
		    }
	    }   
            if(err_num != 0)
            {
              printf("gkhy_debug:err_num = %d\n",err_num);
              err_num = 0;
            }
        //      if(cnt == 100000)
        //     {
        //       printf("gkhy_debug:cnt = %d\n",cnt);
        //       return 0;
        //     }
        // for(i = 0;i<(rec_len);i++)
        //   {
        //     if(i%16 == 0)
        //     {
        //         printf("\n");
        //     }
        //     printf("0x%02x ",rbuffer[i]);
        //   }
     
      }
    else
    printf("poll nothing--------------------------\n");
   }


   pthread_exit(0);
}

void *rapidio_taks_rec3(void *arg)
{
    
    // printf("r___________________________________________________________________");
    int ret = 0,i,err_num;
    unsigned int rec_len = 0;
    struct pollfd fds[1];
    char buff[10];
    static cnt = 0;
  

    fds[0].fd = gpio_fd7;
    fds[0].events  = POLLPRI;
    trans3.input_channel = 6;
    trans3.output_channel = 7;
    printf("AXI DMA3 File Transfer Info:\n");
    printf("\tTransmit Channel: %d\n", trans3.input_channel);
    printf("\tReceive Channel: %d\n", trans3.output_channel); 

    ret = read(gpio_fd7,buff,10);
    if( ret == -1 )
        MSG("read\n");

    while(1)
    {
      ret = poll(fds,1,-1);
      if( ret == -1 )
          MSG("poll\n");
      if( fds[0].revents & POLLPRI)
      {
        ret = lseek(gpio_fd7,0,SEEK_SET);
        if( ret == -1 )
            MSG("lseek\n");
        ret = read(gpio_fd7,buff,10);
        if( ret == -1 )
            MSG("read\n");

    //    printf("\n--------------------------------------------------------------------------------\n");
        rec_len = rapidio_dj_read(axidma_dev, &trans3, rbuffer3);
    //     XBram_Out32(map_base0+8,0x1);
    // //   usleep(15);
    //     XBram_Out32(map_base0+8,0x0);
        cnt++;
        if(rec_len > MAXLENGTH)
	    {
	     printf("gkhy_debug : DMA3 recv len error10240 \n");
	     continue;
	    }
        // rapidio_dj_send(axidma_dev, &trans3, rbuffer3);
        if(cnt%1000 == 0)
	     {
	       printf("\nDMA3 rec_len = 0x%x,cnt = %d\n",rec_len,cnt);
	     }
        // printf("\nrec_len = 0x%x,cnt=%d\n",rec_len,cnt);
        for(i=0;i<rec_len;i++)
	    {
		    if(rbuffer3[i] != tbuffer[i])
		    {
			  printf("khy_debug :tbuffer[%d] : 0x%x,	rbuffer[%d] : 0x%x\n",i,tbuffer[i],i,rbuffer3[i]);
			  err_num++;
		    }
	    }   
            if(err_num != 0)
            {
              printf("gkhy_debug:err_num = %d\n",err_num);
              err_num = 0;
            }
        //      if(cnt == 100000)
        //     {
        //       printf("gkhy_debug:cnt = %d\n",cnt);
        //       return 0;
        //     }
        // for(i = 0;i<(rec_len);i++)
        //   {
        //     if(i%16 == 0)
        //     {
        //         printf("\n");
        //     }
        //     printf("0x%02x ",rbuffer[i]);
        //   }
     
      }
    else
    printf("poll nothing--------------------------\n");
   }


   pthread_exit(0);
}
void *rapidio_taks_send0(void *arg)
{
    int i;
    int cnt = 0;
    int rc,ret;
   
    // struct dma_transfer trans;

    while(1)
    {
      usleep(4000);
      
      rapidio_jm_send(axidma_dev, &trans0, sbuffer0);
    //   printf("send success");
      cnt++;
      if(cnt%1000 == 0)
	     {
	       printf("DMA0 send %d packet\n",cnt);
	     }
    //   printf("send %d packet",cnt);
    if(cnt == 100000)
        {
            printf("gkhy_debug:cnt = %d\n",cnt);
            return 0;
        }
      
     }
destroy_axidma:
    axidma_destroy(axidma_dev);
close_output:
    assert(close(trans0.output_fd) == 0);
// close_input:
//     assert(close(trans.input_fd) == 0);
ret:
    return rc;

    pthread_exit(0);
 
}
void *rapidio_taks_send1(void *arg)
{
    int i;
    int cnt = 0;
    int rc,ret;
   
    // struct dma_transfer trans;

    while(1)
    {
      usleep(4000);
      
      rapidio_dx_send(axidma_dev, &trans1, sbuffer1);
    //   printf("send success");
      cnt++;
      if(cnt%1000 == 0)
	     {
	       printf("DMA1 send %d packet\n",cnt);
	     }
    //   printf("send %d packet",cnt);
    if(cnt == 100000)
        {
            printf("gkhy_debug:cnt = %d\n",cnt);
            return 0;
        }
      
     }
destroy_axidma:
    axidma_destroy(axidma_dev);
close_output:
    assert(close(trans1.output_fd) == 0);
// close_input:
//     assert(close(trans.input_fd) == 0);
ret:
    return rc;

    pthread_exit(0);
 
}
void *rapidio_taks_send2(void *arg)
{
    int i;
    int cnt = 0;
    int rc,ret;
   
    // struct dma_transfer trans;

    while(1)
    {
      usleep(4000);
      
      rapidio_dd_send(axidma_dev, &trans2, sbuffer2);
    //   printf("send success");
      cnt++;
      if(cnt%1000 == 0)
	     {
	       printf("DMA2 send %d packet\n",cnt);
	     }
    //   printf("send %d packet",cnt);
    if(cnt == 100000)
        {
            printf("gkhy_debug:cnt = %d\n",cnt);
            return 0;
        }
      
     }
destroy_axidma:
    axidma_destroy(axidma_dev);
close_output:
    assert(close(trans2.output_fd) == 0);
// close_input:
//     assert(close(trans.input_fd) == 0);
ret:
    return rc;

    pthread_exit(0);
 
}
void *rapidio_taks_send3(void *arg)
{
    int i;
    int cnt = 0;
    int rc,ret;
   
    // struct dma_transfer trans;

    while(1)
    {
      usleep(4000);
      
      rapidio_dd_send(axidma_dev, &trans3, sbuffer3);
    //   printf("send success");
      cnt++;
      if(cnt%1000 == 0)
	     {
	       printf("DMA3 send %d packet\n",cnt);
	     }
    //   printf("send %d packet",cnt);
    if(cnt == 100000)
        {
            printf("gkhy_debug:cnt = %d\n",cnt);
            return 0;
        }
      
     }
destroy_axidma:
    axidma_destroy(axidma_dev);
close_output:
    assert(close(trans3.output_fd) == 0);
// close_input:
//     assert(close(trans.input_fd) == 0);
ret:
    return rc;

    pthread_exit(0);
 
}
/*----------------------------------------------------------------------------
 * Main
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    int rc;
    int i;
    int rec_len;
    char *input_path, *output_path;
    struct stat input_stat;
    const array_t *tx_chans, *rx_chans;
    int error;
    int ret;
   //Initialize the test buffer
    for(i = 0;i < MAXLENGTH;i++)
       {
        tbuffer[i]=i;
       }                          
    for(i = 0;i < TESTLENGTH;i++)
       {
        sbuffer0[i]=i;
        sbuffer1[i]=i;
        sbuffer2[i]=i;
        sbuffer3[i]=i;
       }
    
    pthread_t rapidio_sid0;
    pthread_t rapidio_sid1;
    pthread_t rapidio_sid2;
    pthread_t rapidio_sid3;
    pthread_t rapidio_rid0;
    pthread_t rapidio_rid1;
    pthread_t rapidio_rid2;
    pthread_t rapidio_rid3;

    GpioInit();
    //地址映射，使能读写DMA，单独规定
    axidma_config();
    

    //  解析输入参数
    memset(&trans, 0, sizeof(trans));
    if (parse_args(argc, argv, &trans.input_channel,
                   &trans.output_channel, &trans.output_size) < 0) {
        rc = 1;
        goto ret;
    }
    /*****************初始化设备和默认配置准备*******************************/

    // 初始化AXIDMA设备
    axidma_dev = axidma_init();
    if (axidma_dev == NULL) {
        fprintf(stderr, "Error: Failed to initialize the AXI DMA device.\n");
        rc = 1;
        goto close_output;
    }
     printf("Succeed to initialize the AXI DMA device.\n");

    
    // 如果还没有指定tx和rx通道，则获取收发通道
    tx_chans = axidma_get_dma_tx(axidma_dev);
   
    if (tx_chans->len < 1) {
        fprintf(stderr, "Error: No transmit channels were found.\n");
        rc = -ENODEV;
        goto destroy_axidma;
    }
    rx_chans = axidma_get_dma_rx(axidma_dev);
    
    if (rx_chans->len < 1) {
        fprintf(stderr, "Error: No receive channels were found.\n");
        rc = -ENODEV;
        goto destroy_axidma;
    }

    /* 如果用户没有指定通道，我们假设发送和接收通道是编号最低的通道。 */
    if (trans.input_channel == -1 && trans.output_channel == -1) {
        trans.input_channel = tx_chans->data[0];
        trans.output_channel = rx_chans->data[0];
    }
    // trans.input_channel = 0;
    // trans.output_channel = 1;
    // printf("AXI DMAt File Transfer Info:\n");
    // printf("\tTransmit Channel: %d\n", trans.input_channel);
    // printf("\tReceive Channel: %d\n", trans.output_channel);
    // printf("\tInput Data Size: %.4f MiB\n", BYTE_TO_MIB(trans.input_size));
    // printf("\tOutput Data Size: %.4f MiB\n\n", BYTE_TO_MIB(trans.output_size));
     /*****************************************************************************/
     /*********************设置收发长度，并映射相应空间************************/
    
    trans0.output_size = MAXLENGTH;//JM接收长度
    trans0.input_size = TESTLENGTH;//JM发送长度
    trans1.output_size = MAXLENGTH;//DX接收长度
    trans1.input_size = TESTLENGTH;//DX发送长度
    trans2.output_size = MAXLENGTH;//DD接收长度
    trans2.input_size = TESTLENGTH;//DD发送长度
    trans3.output_size = MAXLENGTH;//DJ接收长度
    trans3.input_size = TESTLENGTH;//DJ发送长度
    // 为输出文件分配一个缓冲区
    trans0.output_buf = axidma_malloc(axidma_dev, trans0.output_size);
    if (trans0.output_buf == NULL) {
        rc = -ENOMEM;
        // goto free_output_buf;
        axidma_free(axidma_dev, trans0.output_buf, trans0.output_size);
    }
    trans0.input_buf = axidma_malloc(axidma_dev, trans0.input_size);
    if (trans0.input_buf == NULL) {
        fprintf(stderr, "Failed to allocate the input buffer.\n");
        rc = -ENOMEM;
        axidma_free(axidma_dev, trans0.input_buf, trans0.input_size);
    }
    trans1.output_buf = axidma_malloc(axidma_dev, trans1.output_size);
    if (trans1.output_buf == NULL) {
        rc = -ENOMEM;
        // goto free_output_buf;
        axidma_free(axidma_dev, trans1.output_buf, trans1.output_size);
    }
    trans1.input_buf = axidma_malloc(axidma_dev, trans1.input_size);
    if (trans1.input_buf == NULL) {
        fprintf(stderr, "Failed to allocate the input buffer.\n");
        rc = -ENOMEM;
        axidma_free(axidma_dev, trans1.input_buf, trans1.input_size);
    }
    trans2.output_buf = axidma_malloc(axidma_dev, trans2.output_size);
    if (trans2.output_buf == NULL) {
        rc = -ENOMEM;
        // goto free_output_buf;
        axidma_free(axidma_dev, trans2.output_buf, trans2.output_size);
    }
    trans2.input_buf = axidma_malloc(axidma_dev, trans2.input_size);
    if (trans2.input_buf == NULL) {
        fprintf(stderr, "Failed to allocate the input buffer.\n");
        rc = -ENOMEM;
        axidma_free(axidma_dev, trans2.input_buf, trans2.input_size);
    }
    trans3.output_buf = axidma_malloc(axidma_dev, trans3.output_size);
    if (trans3.output_buf == NULL) {
        rc = -ENOMEM;
        // goto free_output_buf;
        axidma_free(axidma_dev, trans3.output_buf, trans3.output_size);
    }
    trans3.input_buf = axidma_malloc(axidma_dev, trans3.input_size);
    if (trans3.input_buf == NULL) {
        fprintf(stderr, "Failed to allocate the input buffer.\n");
        rc = -ENOMEM;
        axidma_free(axidma_dev, trans3.input_buf, trans3.input_size);
    }
    printf("DMA info......\n");
    /*****************************************************************************/
    //开8路线程分别对4路dma进行收发测试
    
      error=pthread_create(&rapidio_rid0, NULL, &rapidio_taks_rec,NULL);
      if(error != 0)
      {
        printf("pthreadrx_create fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_rid1, NULL, &rapidio_taks_rec1,NULL);
      if(error != 0)
      {
        printf("pthreadrx_create1 fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_rid2, NULL, &rapidio_taks_rec2,NULL);
      if(error != 0)
      {
        printf("pthreadrx_create2 fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_rid3, NULL, &rapidio_taks_rec3,NULL);
      if(error != 0)
      {
        printf("pthreadrx_create3fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_sid0, NULL, &rapidio_taks_send0,NULL);
      if(error != 0)
      {
        printf("pthreadtx_create fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_sid1, NULL, &rapidio_taks_send1,NULL);
      if(error != 0)
      {
        printf("pthreadtx_create fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_sid2, NULL, &rapidio_taks_send2,NULL);
      if(error != 0)
      {
        printf("pthreadtx_create fail\n");
        return -1;
      }
      error=pthread_create(&rapidio_sid3, NULL, &rapidio_taks_send3,NULL);
      if(error != 0)
      {
        printf("pthreadtx_create fail\n");
        return -1;
      }
    pthread_detach(rapidio_rid0);
    pthread_detach(rapidio_rid1);
    pthread_detach(rapidio_rid2);
    pthread_detach(rapidio_rid3);
    pthread_detach(rapidio_sid0);
    pthread_detach(rapidio_sid1);
    pthread_detach(rapidio_sid2);
    pthread_detach(rapidio_sid3);

    while(1)
    {
        sleep(1);
    }
 //  rc = (rc < 0) ? -rc : 0;    
destroy_axidma:
    axidma_destroy(axidma_dev);
close_output:
    assert(close(trans.output_fd) == 0);
// close_input:
//     assert(close(trans.input_fd) == 0);
ret:
    return rc;

}
