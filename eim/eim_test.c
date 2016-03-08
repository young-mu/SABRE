// eim_test.c
// write and read individually
// ./eim_test w        - write process
// ./eim_test r        - read process
// ./eim_test r d      - read process with debug information

#include <stdio.h> 
#include <stdlib.h> 
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define LEN (480)

// download modes
#define DOWNLOAD_MODE       (2)
#define DOWNLOAD_PROGRAM    (1)
#define DOWNLOAD_PARAMETERS (2)

int main(int argc, char **argv)  
{  
	// check the number of input arguments 
	if (2 != argc & 3 != argc) 
	{
        printf("Wrong arguments.\n");
        return -1;
    }

	// write buffer initialization
    unsigned char *wbuf;	
    wbuf = (unsigned char *)malloc(sizeof(unsigned char) * LEN);
	memset(wbuf, 0, sizeof(unsigned char) * LEN);
    for (int i = 0; i < LEN; i++) 
    {
        wbuf[i] = i % 256;
    }
    
    // read buffer initialization
    unsigned char *rbuf;    
    rbuf = (unsigned char *)malloc(sizeof(unsigned char) * LEN);
	memset(rbuf, 0, sizeof(unsigned char) * LEN);
    
    // open file
    int fd;  
    fd = open("/dev/eim", O_RDWR);  
    if (fd < 0) 
    {  
        printf("open /dev/eim failed.\n");  
        return -1;  
    }    
   
    // write
    int wcnt = 0;
    struct timeval wtstart, wtend;
    long wtuse = 0;;
	if ('w' == *argv[1]) 
	{
        // write and check download mode
        int dmode_wfd;
        dmode_wfd = open("/sys/class/eim/eim/dmode", O_RDWR);
        int wdmode = 0;
        wdmode = (int)DOWNLOAD_MODE;
        char dmode_wbuf[10];
        memset(dmode_wbuf, 0, sizeof(char) * 10);
        sprintf(dmode_wbuf, "%d", wdmode);
        write(dmode_wfd, (void *)dmode_wbuf, 10);
        close(dmode_wfd);        

        int dmode_rfd;
        dmode_rfd = open("/sys/class/eim/eim/dmode", O_RDWR);
        int rdmode = 0;
        char dmode_rbuf[10];
        memset(dmode_rbuf, 0, sizeof(char) * 10);
        read(dmode_rfd, (void *)dmode_rbuf, 10);
        rdmode = atoi(dmode_rbuf);
        if (wdmode == rdmode)
        {
            if (DOWNLOAD_PROGRAM == rdmode)
            {
                printf("------------------------------------\n");
                printf("Download mode : download FPGA program.\n");
                printf("------------------------------------\n");
            }
            else if (DOWNLOAD_PARAMETERS == rdmode)
            {   
                printf("------------------------------------\n");
                printf("Download mode : download front-end parameters.\n");
                printf("------------------------------------\n");
            }
        }
        else
        {
            printf("Invalid download mode : %d.\n", rdmode);
            return -1;
        }
        close(dmode_rfd);                

		printf("Writing...\n");

    	gettimeofday(&wtstart, NULL);
    	wcnt = write(fd, (void *)wbuf, LEN);
        if (wcnt != LEN) 
        {
            printf("write failed.\n");
        }
        gettimeofday(&wtend, NULL);
        
        wtuse = 1000000 * (wtend.tv_sec - wtstart.tv_sec) + (wtend.tv_usec - wtstart.tv_usec);
        printf("Write %d B, used %ld us. Write speed : %.2f MB/s.\n",  LEN, wtuse, (float)((float)LEN / wtuse * 1000000 / 1024 / 1024)); 
        printf("------------------------------------\n");
	}

	// read 
	int rcnt = 0;
	struct timeval rtstart, rtend;
	long rtuse = 0;
	if ('r' == *argv[1]) 
	{
        printf("------------------------------------\n");
		printf("Reading...\n");

		gettimeofday(&rtstart, NULL);
		rcnt = read(fd, (void*)rbuf, LEN);
        if (rcnt != LEN) 
        {
        	printf("read failed.\n");
    	}
        gettimeofday(&rtend, NULL);
        
        rtuse = 1000000 * (rtend.tv_sec - rtstart.tv_sec) + (rtend.tv_usec - rtstart.tv_usec);
        printf("Read %d B, used %ld us. Read speed : %.2f MB/s.\n",  LEN, rtuse, (float)((float)LEN / rtuse * 1000000 / 1024 / 1024));
        printf("------------------------------------\n");
	}
	
	// error check
	int ecnt = 0;
    if ('r' == *argv[1])
    {
	    for (int e = 0; e < LEN; e++) 
	    {
		    if (wbuf[e] != rbuf[e]) 
		    {
        	    ecnt++;
                if (3 == argc)
                {
                    if ('d' == *argv[2])
                    {
           	            printf("Wrong. wbuf[%d] = %d, rbuf[%d] = %d\n", e, wbuf[e], e, rbuf[e]);
                    }
                }
            }
	    }
        if (0 == ecnt) 
        {
   	    	printf("Right.\n");
        } 
        else 
        {
            printf("Wrong. The number is %d of %d.\n", ecnt, (int)LEN);
            printf("Error ration : %.2f%\n", (float)(100 * (float)ecnt / (int)LEN));
            printf("------------------------------------\n");
	    }
    }

	free(wbuf);
	free(rbuf);
    close(fd);  
    
    return 0;  
}  
