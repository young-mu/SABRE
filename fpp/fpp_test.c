// NAME : fpp test program
// FUNC : FPGA J1-1 40M clock & J1-3 1M clock
// DATE : 2013.08.31 by Young

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

int main(int argc, char **argv)
{
    // open rbf file (raw binary file)
    FILE *rbf_file = NULL;
	rbf_file = fopen("FPP_test.rbf", "rb");
    if (NULL == rbf_file)
    {
        return -1;
    }
   
    // get rbf file size
    int rbf_size = 0;
    fseek(rbf_file, 0, SEEK_END);
    rbf_size = ftell(rbf_file);
    fseek(rbf_file, 0, SEEK_SET);
    
    // allocate rbf buffer and read from rbf file
    unsigned char *rbf_buf;
    rbf_buf = (unsigned char*)malloc(sizeof(unsigned char) * rbf_size);
	fread(rbf_buf, sizeof(unsigned char), rbf_size, rbf_file);
    
    // open fpp device
    int fpp_dev = 0;
    fpp_dev = open("/dev/fpp", O_RDWR);
    if (fpp_dev < 0)
    {
        printf("open /dev/fpp failed.\n");
        free(rbf_buf);
        fclose(rbf_file);
        return -1;
    }   

    // configure FPGA
    int write_cnt = 0;
    struct timeval conf_start, conf_end;
    long conf_use = 0;
    printf("****************************\n");
    printf("configuration starts.\n");
    gettimeofday(&conf_start, NULL);
    write_cnt = write(fpp_dev, (void *)rbf_buf, rbf_size);
    gettimeofday(&conf_end, NULL);
    conf_use = 1000000 * (conf_end.tv_sec - conf_start.tv_sec) + (conf_end.tv_usec - conf_start.tv_usec);
    if (write_cnt != rbf_size) 
    {
        printf("configuration fails.\n");
    }
    else
    {
        printf("configuration succeeds.\n"); 
        printf("****************************\n");
        printf("configuration time : %.2f s.\n", (float)conf_use / 1000000);
        printf("****************************\n");
    }

    
    close(fpp_dev);
    free(rbf_buf);
    fclose(rbf_file);

    return 0;
}
