// sdma_m2m_test.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>

#define WBUF_SIZE			1024						// 1KB
#define RBUF_CNT			16
#define RING_BUF_SIZE		(RBUF_CNT * WBUF_SIZE)		// 16KB

int main(int argc, char **argv) 
{
	// define write buffer
    unsigned char wbuf[WBUF_SIZE] = {0};

	// open sdma_m2m device file
    int fd;
    fd = open("/dev/sdma_m2m", O_RDWR);
    if (fd < 0)
	{
        printf("open /dev/sdma_m2m failed.\n");  
        return -1; 
    }

	// initiate read buffer
    unsigned char *rbuf = NULL;
    rbuf = (unsigned char *)mmap(NULL, RING_BUF_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (rbuf == MAP_FAILED)
    {
        perror("< sdma_m2m_test.c > mmap failed.\n");
        return -1;
    }

	// test
	for (int cnt = 0; cnt < 32; cnt++)
	{
		// initiate write buffer
		for (int i = 0; i < WBUF_SIZE; i++)
		{
			wbuf[i] = cnt + i;
		}

		// write WBUF_SIZE bytes
		write(fd, wbuf, WBUF_SIZE);

		// trigger DMA trasferring from wbuf to rbuf in Kernel Space
		// ( NODE : when it returns, DMA work has been completed )
		read(fd, NULL, WBUF_SIZE);

		// check results
		for (int i = 0; i < WBUF_SIZE; i++)
		{
			if (wbuf[i] != rbuf[i + (cnt % RBUF_CNT) * 1024])
			{
				printf("ERROR at %d\n", i);
				printf("wbuf[%d] : %d - rbuf[%d] : %d\n", i, wbuf[i], i, rbuf[i + (cnt % RBUF_CNT) * 1024]);
				break;
			}
		}
		printf("OK @ %d\n", cnt);
	}

	// release resources
    munmap(rbuf, RING_BUF_SIZE);
    close(fd);

	return 0;
}
