#include "libeim.h"

#include <sys/time.h>

// the length must be any integer times of double bytes
#define LEN                 (480)

int main(int argc, char **argv)
{       
    if (argc != 2)
    {
        printf("input error : the number of input arguments must be 2.\n");
        return -1;
    }

    eim my_eim;
    my_eim.eim_init(LEN, LEN);

    if ('1' == *argv[1])
    {
        struct timeval wtstart, wtend;
        long wtuse = 0;
        int ret = 0;

        gettimeofday(&wtstart, NULL);
        ret = my_eim.eim_write16();
        gettimeofday(&wtend, NULL);
        wtuse = 1000000 * (wtend.tv_sec - wtstart.tv_sec) + (wtend.tv_usec - wtstart.tv_usec);

        if (0 == ret)
        {
            printf("Configure FPGA succeeds.\n");
            printf("------------------------------------\n");
            printf("The time of FPGA configuration : %0.2f s.\n", (float)wtuse / 1000000); 
            printf("------------------------------------\n");
        }
        else
        {
            printf("Configure FPGA fails.\n");
        }
    }
    else if ('2' == *argv[1])
    {
        unsigned char rbuf[LEN];
//		unsigned short rbuf[LEN];
		int count = 0;
		for (int k = 0; k < 32; k++)
		{
        	count = 0;
			// write (0-511, 1-512, 2-513, ..., 31-542)
		    my_eim.eim_write();
			// read
		    my_eim.eim_read(rbuf);
//			my_eim.eim_read16((unsigned char *)rbuf);
			// test
		    for (int i = 0; i < LEN; i++)
		    {
		        if ((i + k) % 256 == rbuf[i])
		            count++;
		    }
		    if (LEN == count)
		        printf("right @ %d\n", k);
		    else
			{
		        printf("wrong @ %d (count = %d)\n", k, count);
				for (int i = 0; i < LEN; i++)
				{
					printf("rbuf[%d] = %d\n", i, rbuf[i]);
				}
			}
		}
    }
    else
    {
        printf("input error : the second argument must be 1 or 2.\n");
        return -1;
    }

    return 0;
}
