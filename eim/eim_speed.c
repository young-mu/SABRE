// eim_speed.c
// EIM speed test
// the second argument specifies the number of MBytes data

#include <stdio.h> 
#include <stdlib.h> 
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define LEN (512)

// download modes
#define DOWNLOAD_MODE       (2)
#define DOWNLOAD_PROGRAM    (1)
#define DOWNLOAD_PARAMETERS (2)


int main(int argc, char **argv)  
{  
	// check the number of input arguments 
    if (argc != 2)
    {
        printf("Wrong arguments.\n");
        return 0;
    }

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
    
    // num MB
    float nM = 0;
    nM = atof(argv[1]);
    int NUM = 0;
    NUM = nM * 1024 * 1024 / LEN;
    
	// write & read buffer initialization
    unsigned char **wbuf = NULL;	
    unsigned char **rbuf = NULL;
    wbuf = (unsigned char **)malloc(sizeof(unsigned char *) * NUM);
    rbuf = (unsigned char **)malloc(sizeof(unsigned char *) * NUM);
	for (int i = 0; i < NUM; i++)
	{
		wbuf[i] = (unsigned char*)malloc(sizeof(unsigned char) * LEN);
		rbuf[i] = (unsigned char*)malloc(sizeof(unsigned char) * LEN);
	}
    for (int i = 0; i < NUM; i++)
    {
	    memset(wbuf[i], 0, sizeof(unsigned char) * LEN);
        memset(rbuf[i], 0, sizeof(unsigned char) * LEN);
    }
    for (int i = 0; i < NUM; i++) 
    {
        for (int j = 0; j < LEN; j++)
        {
        	wbuf[i][j] = j % 256;
        }
    }

    // open file
    int fd;  
    fd = open("/dev/eim", O_RDWR);  
    if (fd < 0) 
    {  
        printf("open /dev/eim failed.\n");  
        return -1;  
    }    
  
    int wcnt = 0;
    int rcnt = 0;
    struct timeval wtstart, wtend;
    struct timeval rtstart, rtend;
    long wtuse = 0;
    long rtuse = 0;
    long ecnt = 0;
    for (int i = 0; i < NUM; i++)
    {
        wcnt = 0;
        rcnt = 0;
        
        // write
    	gettimeofday(&wtstart, NULL);
    	wcnt = write(fd, (void *)wbuf[i], LEN);
        if (wcnt != LEN) 
        {
            printf("write failed - %d.\n", i);
        }
        gettimeofday(&wtend, NULL);
        
        wtuse += 1000000 * (wtend.tv_sec - wtstart.tv_sec) + (wtend.tv_usec - wtstart.tv_usec);

        // read
        gettimeofday(&rtstart, NULL);
        rcnt = read(fd, (void *)rbuf[i], LEN);
        if (rcnt != LEN)
        {
            printf("read failed - %d.\n", i);
        }
        gettimeofday(&rtend, NULL);
        
        rtuse += 1000000 * (rtend.tv_sec - rtstart.tv_sec) + (rtend.tv_usec - rtstart.tv_usec);

        // error check
        for (int e = 0; e < LEN; e++) 
	    {
		    if (wbuf[i][e] != rbuf[i][e]) 
		    {
                ecnt++;
                if (3 == argc)
                {
                    if ('d' == *argv[2])
                    {
                        printf("Wrong. wbuf[%d][%d] = %d, rbuf[%d][%d] = %d\n", i, e, wbuf[e], i, e, rbuf[e]);
                    }
                }
            }
	    }
	}

    if (ecnt != 0)
    {
        printf("EIM speed test failed.\n");
        printf("%ld error of all the %ld data.\n", ecnt, NUM * LEN);
        printf("Error ratio : %.2f%\n", (float)(100 * (float)ecnt / NUM / LEN));
        printf("------------------------------------\n");
    }
    else
    {
        printf("EIM speed test passed.\n");
        printf("Write %.2f MB, used %.2f ms. Write speed : %.2f MB/s.\n", nM, 
                (float)((float)wtuse / 1000), 
                (float)((float)NUM * LEN / wtuse * 1000000 / 1024 / 1024)); 
        printf("Read %.2f MB, used %.2f ms. Read speed : %.2f MB/s.\n", nM, 
                (float)((float)rtuse / 1000), 
                (float)((float)NUM * LEN / rtuse * 1000000 / 1024 / 1024));
        printf("------------------------------------\n"); 
    }

    // close file
    close(fd);  

    return 0;  
}  
