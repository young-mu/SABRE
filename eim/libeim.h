#ifndef _LIBEIM_H_
#define _LIBEIM_H_

#include <iostream>
#include <stdio.h>
#include <stdlib.h> 
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

using namespace std;

// FPGA file length
#define FPGA_FILE_LENGTH        (4928127)

// dmode - download mode
#define EIM_DOWNLOAD_PROGRAM    (1)
#define EIM_DOWNLOAD_PARAMETERS (2)

// MUM - mux mode
#define EIM_NOMUX               (0)
#define EIM_MUX                 (1)

// BCD - burst clock division
#define EIM_BCLK_132M           (0)
#define EIM_BCLK_66M            (1)
#define EIM_BCLK_44M            (2)
#define EIM_BCLK_33M            (3)

// WWSC - write wait state control
#define EIM_WWSC_4CLKs          (0)
#define EIM_WWSC_5CLKs          (1)          

// conversion type
#define EIM_W_TYPE              (0)
#define EIM_R_TYPE              (1)

// endian
#define EIM_BIG_ENDIAN          (0)
#define EIM_LITTLE_ENDIAN       (1)

class eim
{
public:
    eim();
    ~eim();

    // eim initialization
    int eim_init(int paralength, int datalength);

    // download 8-bit data (front-end parameters)
    int eim_write(void);

    // download 16-bit data (FPGA program)
    int eim_write16(void);

    // upload 8-bit data
    int eim_read(unsigned char *buf);

    // upload 16-bit data
    int eim_read16(unsigned char *buf);

    // set & get fpgalength
    void eim_set_fpgalength(int length);
    int eim_get_fpgalength(void);

    // set & get paralength
    void eim_set_paralength(int length);
    int eim_get_paralength(void);

    // set & get datalength
    void eim_set_datalength(int length);
    int eim_get_datalength(void);

    // set & get download mode
    void eim_set_dmode(int dmode);
    int eim_get_dmode(void);

    // set & get mux mode
    void eim_set_mum(int mum);
    int eim_get_mum(void);

    // set & get burst clock division
    void eim_set_bcd(int bcd);
    int eim_get_bcd(void);

    // set & get write wait state control
    void eim_set_wwsc(int wwsc);
    int eim_get_wwsc(void);

private:
    // eim device file descriptor
    int m_eim_fd;

    // the length of FPGA program
    int m_fpgalength;

    // the length of front-end parameters
    int m_paralength;

    // the length of data to be uploaded
    int m_datalength;

    // device atributes dmode / MUM / BCD / WWSC
    int m_dmode;
    int m_MUM;
    int m_BCD;
    int m_WWSC;

    // 8-bit FPGA program 
    unsigned char *m_fpga_wbuf;

    // 16-bit FPGA program adding zero
    unsigned char *m_fpga_wbuf16;

    // 8-bit front-edn parameters
    unsigned char *m_para_wbuf;

    // 8-bit data to be uploaded
    unsigned char *m_data_rbuf;

    // 16-bit data to be uploaded
    unsigned char *m_data_rbuf16;

    // device address
    char m_device_addr[20];

    // FPGA program file address
    char m_fpgafile_addr[20];

    // device attributes addresses dmode / MUM / BCD / WWSC
    char m_devattr_dmode_addr[30];
    char m_devattr_MUM_addr[30];
    char m_devattr_BCD_addr[30];
    char m_devattr_WWSC_addr[30];

    // convert 8-bit data to 16-bit data
    void char2short(int convtype, int endian);
};

#endif
