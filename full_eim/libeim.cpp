#include "libeim.h"

// ------------------------------------------------------------
// Description :
// 	   EIM constructor.
// Parameters :
//     None.
// Return Value :
//     None.
// Errors :
//     None.
// ------------------------------------------------------------
eim::eim()
{
    m_eim_fd = 0;
    m_fpgalength = 0;
    m_paralength = 0;
    m_datalength = 0;
    m_dmode = 0;
    m_MUM = 0;
    m_BCD = 0;
    m_WWSC = 0;
    m_fpga_wbuf = NULL;
    m_fpga_wbuf16 = NULL;
    m_para_wbuf = NULL;
	m_widx = 0;
    m_data_rbuf = NULL;
	m_rbuf_idx = 0;
    m_data_rbuf16 = NULL;
}

// ------------------------------------------------------------
// Description :
// 	   EIM deconstructor.
// Parameters :
//     None.
// Return Value :
//     None.
// Errors :
//     None.
// ------------------------------------------------------------
eim::~eim()
{
    // release resources
	if (m_para_wbuf)
	{
    	free(m_para_wbuf);
		m_para_wbuf = NULL;
	}
	if (m_data_rbuf)
	{
    	munmap(m_data_rbuf, SDMA_M2M_RBUF);
    	m_data_rbuf = NULL;
	}
	if (m_data_rbuf16)
	{
		free(m_data_rbuf16);
		m_data_rbuf = NULL;
	}

    // close file
    close(m_eim_fd);
}

// ------------------------------------------------------------
// Description :
// 	   This functions completes private members initilization.
// Parameters :
//     paralength - the number of parameters to be downloaded.
//     datalength - the number of data to be uploaded.
// Return Value :
//     0 - eim_init success.
// Errors :
//     None.
// ------------------------------------------------------------
int eim::eim_init(int paralength, int datalength)
{
    sprintf(m_device_addr, "%s", "/dev/eim");
    sprintf(m_fpgafile_addr, "%s", "./fpga_ram.rbf");
    sprintf(m_devattr_dmode_addr, "%s", "/sys/class/eim/eim/dmode");
    sprintf(m_devattr_MUM_addr, "%s", "/sys/class/eim/eim/MUM");
    sprintf(m_devattr_BCD_addr, "%s", "/sys/class/eim/eim/BCD");
    sprintf(m_devattr_WWSC_addr, "%s", "/sys/class/eim/eim/WWSC");

    // open file 
    m_eim_fd = open(m_device_addr, O_RDWR);  
    if (m_eim_fd < 0) 
    {  
        cout<<"< libeim.cpp > eim_init : open "<<m_device_addr<<" failed."<<endl;  
        return -1;
    }    

    // set length (datalength is the uint of 16KB Ring Buffer)
    eim_set_fpgalength(FPGA_FILE_LENGTH);
    eim_set_paralength(paralength);
    eim_set_datalength(datalength);

    // get dmode / MUM / BCD / WWSC
    m_dmode = eim_get_dmode();
    m_MUM = eim_get_mum();
    m_BCD = eim_get_bcd();
    m_WWSC = eim_get_wwsc();
}

// ------------------------------------------------------------
// Description :
// 	   This function writes 8-bit data (parameters) to FPGA through eim.
// Parameters :
//     None.
// Return Value :
//     0 - eim_write success.
// Errors :
//     None.
// ------------------------------------------------------------
int eim::eim_write(void)
{ 
    // check dmode / MUM / WWSC
    if (EIM_DOWNLOAD_PARAMETERS != m_dmode)
    {
        eim_set_dmode(EIM_DOWNLOAD_PARAMETERS);
		m_dmode = EIM_DOWNLOAD_PARAMETERS;
    }
    if (EIM_MUX != m_MUM)
    {
        eim_set_mum(EIM_MUX);
		m_MUM = EIM_MUX;
    }
    if (EIM_WWSC_5CLKs != m_WWSC)
    {
        eim_set_wwsc(EIM_WWSC_5CLKs);
		m_WWSC = EIM_WWSC_5CLKs;
    }

	// temporary initilization of m_para_wbuf
	for (int i = 0; i < m_paralength; i++) 
    {
        m_para_wbuf[i] = (i + m_widx) % 256;	
    }	
	m_widx = (m_widx + 1) % 256;

    // download front-end parameters
    int wcnt = 0;
    wcnt = write(m_eim_fd, (void *)m_para_wbuf, m_paralength);
    if (wcnt != m_paralength) 
    {
        cout<<"< libeim.cpp > eim_write : write failed."<<endl;;
        return -1;
    }

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function writes 16-bit data (program) to FPGA through eim.
// Parameters :
//     None.
// Return Value :
//     0 - eim_write16 success.
// Errors :
//     None.
// ------------------------------------------------------------
int eim::eim_write16(void)
{
    // open FPGA program file
    FILE *fp = NULL;
	fp = fopen(m_fpgafile_addr, "rb");
    if (NULL == fp)
    {
        cout<<"< libeim.cpp > eim_write16 : fopen failed."<<endl;
        return -1;
    }
	fread(m_fpga_wbuf, sizeof(unsigned char), m_fpgalength, fp);
    fclose(fp);
    
    // check dmode / MUM / WWSC
    if (EIM_DOWNLOAD_PROGRAM != m_dmode)
    {
        eim_set_dmode(EIM_DOWNLOAD_PROGRAM);
		m_dmode = EIM_DOWNLOAD_PROGRAM;
    }
    if (EIM_NOMUX != m_MUM)
    {
        eim_set_mum(EIM_NOMUX);
		m_MUM = EIM_NOMUX;
    }
    if (EIM_WWSC_4CLKs != m_WWSC)
    {
        eim_set_wwsc(EIM_WWSC_4CLKs);
		m_WWSC = EIM_WWSC_4CLKs;
    }

    // convert 8-bit to 16-bit
    char2short(EIM_W_TYPE, EIM_LITTLE_ENDIAN);

    // download FPGA program
    int wcnt = 0;
    wcnt = write(m_eim_fd, (void *)m_fpga_wbuf16, m_fpgalength * 2);
    if (wcnt != (m_fpgalength * 2)) 
    {
        cout<<"< libeim.cpp > eim_write16 : write failed."<<endl;;
        return -1;
    }

    // release resources
	if (m_fpga_wbuf)
	{
    	free(m_fpga_wbuf);
		m_fpga_wbuf = NULL;
	}
	if (m_fpga_wbuf16)
	{
    	free(m_fpga_wbuf16);
		m_fpga_wbuf16 = NULL;
	}

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function reads 8-bit data from FPGA through eim.
// Parameters :
//     buf - the address storing the read-back data
// Return Value :
//     0 - eim_read success.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_read(unsigned char *buf)
{
	// trigger DMA trasferring from eim_mem_base to dma_rbuf in Kernel Space
    read(m_eim_fd, NULL, m_datalength);

	// copy from Ring Buffer to application buffer
    memcpy(buf, m_data_rbuf + m_rbuf_idx * SDMA_M2M_UNIT, m_datalength);

	// add the index of Ring Buffer
	m_rbuf_idx = (m_rbuf_idx + 1) % SDMA_RBUF_CNT;

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function reads 16-bit data from FPGA through eim.
// Parameters :
//     buf - the address storing the read-back data
// Return Value :
//     0 - eim_read success.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_read16(unsigned char *buf)
{
	// trigger DMA trasferring from eim_mem_base to dma_rbuf in Kernel Space
    read(m_eim_fd, NULL, m_datalength);

	// convert from 8-bit to 16-bit
    char2short(EIM_R_TYPE, EIM_LITTLE_ENDIAN);

	// copy from Ring Buffer to application buffer
    memcpy(buf, m_data_rbuf16, m_datalength * 2);

	// add the index of Ring Buffer
	m_rbuf_idx = (m_rbuf_idx + 1) % SDMA_RBUF_CNT;

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function sets fpga length and initiates fpga write buffer.
// Parameters :
//     datalength - the length of fpga program to be downloaded
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_fpgalength(int fpgalength)
{
    m_fpgalength = fpgalength;
	
    m_fpga_wbuf = new unsigned char[m_fpgalength];
	memset(m_fpga_wbuf, 0, sizeof(unsigned char) * m_fpgalength);    

    m_fpga_wbuf16 = new unsigned char[m_fpgalength * 2];
	memset(m_fpga_wbuf16, 0, sizeof(unsigned char) * m_fpgalength * 2);    
}

// ------------------------------------------------------------
// Description :
// 	   This function gets fpga length.
// Parameters :
//     None.
// Return Value :
//     m_fpgalength - fpga length.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_fpgalength(void)
{
    return m_fpgalength;
}

// ------------------------------------------------------------
// Description :
// 	   This function sets para length and initiates para write buffer.
// Parameters :
//     paralength - the length of parameters to be downloaded
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_paralength(int paralength)
{
    m_paralength = paralength;

    m_para_wbuf = new unsigned char[m_paralength];
	memset(m_para_wbuf, 0, sizeof(unsigned char) * m_paralength);
}

// ------------------------------------------------------------
// Description :
// 	   This function gets para length.
// Parameters :
//     None.
// Return Value :
//     m_paralength - para length.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_paralength(void)
{
    return m_paralength;
}

// ------------------------------------------------------------
// Description :
// 	   This function sets data length and initiates data read buffer.
// Parameters :
//     datalength - the length of data to be uploaded
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_datalength(int datalength)
{
    m_datalength = datalength;

	// 16KB 8-bit buffer (each uint occupies 1KB)
    m_data_rbuf = (unsigned char *)mmap(NULL, SDMA_M2M_RBUF, PROT_READ, MAP_SHARED, m_eim_fd, 0);
    if (m_data_rbuf == MAP_FAILED)
    {
		cout<<"< libeim.cpp > eim_set_datalength : mmap failed."<<endl;
    }

	// 1KB 16-bit buffer (one uint)
	m_data_rbuf16 = new unsigned char[SDMA_M2M_UNIT * 2];
	memset(m_data_rbuf16, 0, sizeof(unsigned char) * SDMA_M2M_UNIT * 2);
}

// ------------------------------------------------------------
// Description :
// 	   This function gets data length.
// Parameters :
//     None.
// Return Value :
//     m_datalength - data length.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_datalength(void)
{
    return m_datalength;
}

// ------------------------------------------------------------
// Description :
// 	   This function converts 8-bit char data to 16-bit short data.
// Parameters :
//     convtype - conversion type W_TYPE or R_TYPE
//     endian - EIM_BIG_ENDIAN or EIM_LITTLE_ENDIAN
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::char2short(int convtype, int endian)
{
    if (EIM_W_TYPE == convtype)
    {
        if (EIM_BIG_ENDIAN == endian)
        {
            for (int i = 0; i < m_fpgalength; i++)
            {
                m_fpga_wbuf16[2 * i + 1] = m_fpga_wbuf[i];
            }
        }
        else if (EIM_LITTLE_ENDIAN == endian)
        {
            for (int i = 0; i < m_fpgalength; i++)
            {
                m_fpga_wbuf16[2 * i] = m_fpga_wbuf[i];
            }
        }
    }
    else if (EIM_R_TYPE == convtype)
    {
        if (EIM_BIG_ENDIAN == endian)
        {
            for (int i = 0; i < m_datalength; i++)
            {
                m_data_rbuf16[2 * i + 1] = m_data_rbuf[i + m_rbuf_idx * SDMA_M2M_UNIT];
            }
        }
        else if (EIM_LITTLE_ENDIAN == endian)
        {
            for (int i = 0; i < m_datalength; i++)
            {
                m_data_rbuf16[2 * i] = m_data_rbuf[i + m_rbuf_idx * SDMA_M2M_UNIT];
            }
        }
    }
}

// ------------------------------------------------------------
// Description :
// 	   This function sets download mode by sysfs.
// Parameters :
//     dmode - eim dowload mode
//     1 - download FPGA program
//     2 - parameters
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_dmode(int dmode)
{
    int dmode_wfd;
    dmode_wfd = open(m_devattr_dmode_addr, O_RDWR);
    int wdmode = 0;
    wdmode = dmode;
    char dmode_wbuf[10] = {0};
    sprintf(dmode_wbuf, "%d", wdmode);
    write(dmode_wfd, (void *)dmode_wbuf, sizeof(char) * 10);
    close(dmode_wfd);        
}

// ------------------------------------------------------------
// Description :
// 	   This function gets download mode by sysfs.
// Parameters :
//     None.
// Return Value :
//     rdmode - read download mode.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_dmode(void)
{
    int dmode_rfd;
    dmode_rfd = open(m_devattr_dmode_addr, O_RDWR);
    int rdmode = 0;
    char dmode_rbuf[10] = {0};
    read(dmode_rfd, (void *)dmode_rbuf, sizeof(char) * 10);
    rdmode = atoi(dmode_rbuf);
    close(dmode_rfd);  

    return rdmode;
}

// ------------------------------------------------------------
// Description :
// 	   This function sets mux mode by sysfs.
// Parameters :
//     mum - eim mux mode
//     0 - no mux
//     1 - mux
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_mum(int mum)
{
    int mum_wfd;
    mum_wfd = open(m_devattr_MUM_addr, O_RDWR);
    int wmum = 0;
    wmum = mum;
    char mum_wbuf[10] = {0};
    sprintf(mum_wbuf, "%d", wmum);
    write(mum_wfd, (void *)mum_wbuf, sizeof(char) * 10);
    close(mum_wfd);   
}

// ------------------------------------------------------------
// Description :
// 	   This function gets mux mode by sysfs.
// Parameters :
//     None.
// Return Value :
//     rmum - read mux mode.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_mum(void)
{
    int mum_rfd;
    mum_rfd = open(m_devattr_MUM_addr, O_RDWR);
    int rmum = 0;
    char mum_rbuf[10] = {0};
    read(mum_rfd, (void *)mum_rbuf, sizeof(char) * 10);
    rmum = atoi(mum_rbuf);
    close(mum_rfd);  

    return rmum;
}

// ------------------------------------------------------------
// Description :
// 	   This function sets burst clock division by sysfs.
// Parameters :
//     bcd - burst clock division (0 / 1 / 2 / 3)
//     0 - no division @ 132M Hz
//     1 - two division @ 66M Hz
//     2 - three division @ 44M Hz
//     3 - four division @ 33M Hz
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_bcd(int bcd)
{
    int bcd_wfd;
    bcd_wfd = open(m_devattr_BCD_addr, O_RDWR);
    int wbcd = 0;
    wbcd = bcd;
    char bcd_wbuf[10] = {0};
    sprintf(bcd_wbuf, "%d", wbcd);
    write(bcd_wfd, (void *)bcd_wbuf, sizeof(char) * 10);
    close(bcd_wfd);   
}

// ------------------------------------------------------------
// Description :
// 	   This function gets burst clock division by sysfs.
// Parameters :
//     None.
// Return Value :
//     rbcd - read burst clock division.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_bcd(void)
{
    int bcd_rfd;
    bcd_rfd = open(m_devattr_BCD_addr, O_RDWR);
    int rbcd = 0;
    char bcd_rbuf[10] = {0};
    read(bcd_rfd, (void *)bcd_rbuf, sizeof(char) * 10);
    rbcd = atoi(bcd_rbuf);
    close(bcd_rfd);  

    return rbcd;
}

// ------------------------------------------------------------
// Description :
// 	   This function sets write wait state control by sysfs.
// Parameters :
//     wwsc - write wait state control (0 / 1 / 2 / ... / 63)
//     0 - 4 clocks 
//     1 - 1+4 clocks (1th clk is addr in multiplexed mode)
//                    (1 clk is ignored in non-multiplexed mode)
//     2 - 2+4 clocks (1th clk is addr in multiplexed mode and 1 clk is ignored)
//                    (2 clks are ignored in non-multiplexed mode)
//     ...
//     63 - 63+4 clocks (1th clk is addr in multiplexed mode and 62 clks are ignored)
//                      (63 clks are ignored in non-multiplexed mode)
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
void eim::eim_set_wwsc(int wwsc)
{
    int wwsc_wfd;
    wwsc_wfd = open(m_devattr_WWSC_addr, O_RDWR);
    int wwwsc = 0;
    wwwsc = wwsc;
    char wwsc_wbuf[10] = {0};
    sprintf(wwsc_wbuf, "%d", wwwsc);
    write(wwsc_wfd, (void *)wwsc_wbuf, sizeof(char) * 10);
    close(wwsc_wfd);   
}

// ------------------------------------------------------------
// Description :
// 	   This function gets write wait state control by sysfs.
// Parameters :
//     None.
// Return Value :
//     r_wwsc - read write wait state control.
// Errors :
//     None.
// -------------------------------------------------------------
int eim::eim_get_wwsc(void)
{
    int wwsc_rfd;
    wwsc_rfd = open(m_devattr_WWSC_addr, O_RDWR);
    int rwwsc = 0;
    char wwsc_rbuf[10] = {0};
    read(wwsc_rfd, (void *)wwsc_rbuf, sizeof(char) * 10);
    rwwsc = atoi(wwsc_rbuf);
    close(wwsc_rfd);  

    return rwwsc;
}
