// NAME : eim driver program (External Interface Module)
// FUNC : transfer data between FPGA and ARM
// DATE : 2013.08.05 by Young
// DESP : 16-bit data/addr multiplexed mode (default)
//        synchronous transmission mode
//        dmode / MUM / BCD / WWSC sysfs
// HIST : V1.0 2013.08.05 - eim driver program
//        V1.1 2013.09.04 - add FPP function
//        V1.2 2013.09.20 - add WWSC device attribute

#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/iomux-mx6q.h>


// print debug information
#define DEBUG 			        (0)

// download mode
#define DOWNLOAD_PROGRAM        (1)
#define DOWNLOAD_PARAMETERS     (2)

// registers address
#define EIM_MEM_BASE  	        (0x0C000000)
#define EIM_MEM_LEN             (0x4000000)
#define EIM_BASE 		        (0x021B8000)
#define EIM_LEN  		        (0x98 + 0x4)
#define CCM_BASE 		        (0x020C4000)
#define CCM_LEN 		        (0x88 + 0x4)
#define CSI0_DAT8_BASE          (0x020E0648)
#define CSI0_DAT8_LEN           (0x04)
#define CSI0_DAT9_BASE          (0x020E064C)
#define CSI0_DAT9_LEN           (0x04)

#define DEVICE_NAME 	        "eim"

// GPIO defination
#define GPIO_FPP_UNUSE          IMX_GPIO_NR(4,14)   // KEY_COL4
#define GPIO_FPP_nCONFIG        IMX_GPIO_NR(3,16)   // EIM_D16
#define GPIO_FPP_nSTATUS        IMX_GPIO_NR(3,17)   // EIM_D17
#define GPIO_FPP_CONF_DONE      IMX_GPIO_NR(3,18)   // EIM_D18

// GPIO methods
#define DRIVE_nCONFIG_LOW()     gpio_set_value(GPIO_FPP_nCONFIG, 0)
#define DRIVE_nCONFIG_HIGH()    gpio_set_value(GPIO_FPP_nCONFIG, 1)

#define READ_nSTATUS()          gpio_get_value(GPIO_FPP_nSTATUS)
#define READ_CONF_DONE()        gpio_get_value(GPIO_FPP_CONF_DONE)

// set CSI0_DAT8 (I2C1_SDA) & CSI_DAT9 (I2C1_SCL) as push-pull mode (not open-drain mode)
#define CSI0_DAT8_PCR           (mdev->csi0_dat8_base)
#define CSI0_DAT9_PCR           (mdev->csi0_dat9_base)
#define SET_DAT6_PUSHPULL()     iowrite32(0x0001B0B0, CSI0_DAT8_PCR)
#define SET_DAT7_PUSHPULL()     iowrite32(0x0001B0B0, CSI0_DAT9_PCR)


// IOMUX configuration (eim_mux)
static iomux_v3_cfg_t eim_mux_pads[] = {
	// 16-bit data/addr multiplexed
    MX6Q_PAD_EIM_DA0__WEIM_WEIM_DA_A_0,
    MX6Q_PAD_EIM_DA1__WEIM_WEIM_DA_A_1,
    MX6Q_PAD_EIM_DA2__WEIM_WEIM_DA_A_2,
    MX6Q_PAD_EIM_DA3__WEIM_WEIM_DA_A_3,
    MX6Q_PAD_EIM_DA4__WEIM_WEIM_DA_A_4,
    MX6Q_PAD_EIM_DA5__WEIM_WEIM_DA_A_5,
    MX6Q_PAD_EIM_DA6__WEIM_WEIM_DA_A_6,
    MX6Q_PAD_EIM_DA7__WEIM_WEIM_DA_A_7,
    MX6Q_PAD_EIM_DA8__WEIM_WEIM_DA_A_8,
    MX6Q_PAD_EIM_DA9__WEIM_WEIM_DA_A_9,
    MX6Q_PAD_EIM_DA10__WEIM_WEIM_DA_A_10,
    MX6Q_PAD_EIM_DA11__WEIM_WEIM_DA_A_11,
    MX6Q_PAD_EIM_DA12__WEIM_WEIM_DA_A_12,
    MX6Q_PAD_EIM_DA13__WEIM_WEIM_DA_A_13,
    MX6Q_PAD_EIM_DA14__WEIM_WEIM_DA_A_14,
    MX6Q_PAD_EIM_DA15__WEIM_WEIM_DA_A_15,
	// burst clock
    MX6Q_PAD_EIM_BCLK__WEIM_WEIM_BCLK,
	// read from FPGA (active-L)
    MX6Q_PAD_EIM_OE__WEIM_WEIM_OE,
	// write to FPGA (active-L)
    MX6Q_PAD_EIM_RW__WEIM_WEIM_RW,
	// address valid (active-L)
    MX6Q_PAD_EIM_LBA__WEIM_WEIM_LBA,
	// cs1 64M (active-L)
    MX6Q_PAD_EIM_CS1__WEIM_WEIM_CS_1,
    // KEY_COL4 -- UNUSE
    MX6Q_PAD_KEY_COL4__GPIO_4_14,
    // EIM_D16 -- nCONFIG
    MX6Q_PAD_EIM_D16__GPIO_3_16,
    // EIM_D17 -- nSTATUS
    MX6Q_PAD_EIM_D17__GPIO_3_17,
    // EIM_D18 -- CONF_DONE
    MX6Q_PAD_EIM_D18__GPIO_3_18,
};

// IOMUX configuration (eim_nomux)
static iomux_v3_cfg_t eim_nomux_pads[] = {
	// 16-bit addr
    MX6Q_PAD_EIM_DA0__WEIM_WEIM_DA_A_0,
    MX6Q_PAD_EIM_DA1__WEIM_WEIM_DA_A_1,
    MX6Q_PAD_EIM_DA2__WEIM_WEIM_DA_A_2,
    MX6Q_PAD_EIM_DA3__WEIM_WEIM_DA_A_3,
    MX6Q_PAD_EIM_DA4__WEIM_WEIM_DA_A_4,
    MX6Q_PAD_EIM_DA5__WEIM_WEIM_DA_A_5,
    MX6Q_PAD_EIM_DA6__WEIM_WEIM_DA_A_6,
    MX6Q_PAD_EIM_DA7__WEIM_WEIM_DA_A_7,
    MX6Q_PAD_EIM_DA8__WEIM_WEIM_DA_A_8,
    MX6Q_PAD_EIM_DA9__WEIM_WEIM_DA_A_9,
    MX6Q_PAD_EIM_DA10__WEIM_WEIM_DA_A_10,
    MX6Q_PAD_EIM_DA11__WEIM_WEIM_DA_A_11,
    MX6Q_PAD_EIM_DA12__WEIM_WEIM_DA_A_12,
    MX6Q_PAD_EIM_DA13__WEIM_WEIM_DA_A_13,
    MX6Q_PAD_EIM_DA14__WEIM_WEIM_DA_A_14,
    MX6Q_PAD_EIM_DA15__WEIM_WEIM_DA_A_15,
    // 16-bit data
    MX6Q_PAD_CSI0_DATA_EN__WEIM_WEIM_D_0,
    MX6Q_PAD_CSI0_VSYNC__WEIM_WEIM_D_1,
    MX6Q_PAD_CSI0_DAT4__WEIM_WEIM_D_2,
    MX6Q_PAD_CSI0_DAT5__WEIM_WEIM_D_3,
    MX6Q_PAD_CSI0_DAT6__WEIM_WEIM_D_4,
    MX6Q_PAD_CSI0_DAT7__WEIM_WEIM_D_5,
    MX6Q_PAD_CSI0_DAT8__WEIM_WEIM_D_6,
    MX6Q_PAD_CSI0_DAT9__WEIM_WEIM_D_7,
    MX6Q_PAD_CSI0_DAT12__WEIM_WEIM_D_8,
    MX6Q_PAD_CSI0_DAT13__WEIM_WEIM_D_9,
    MX6Q_PAD_CSI0_DAT14__WEIM_WEIM_D_10,
    MX6Q_PAD_CSI0_DAT15__WEIM_WEIM_D_11,
    MX6Q_PAD_CSI0_DAT16__WEIM_WEIM_D_12,
    MX6Q_PAD_CSI0_DAT17__WEIM_WEIM_D_13,
    MX6Q_PAD_CSI0_DAT18__WEIM_WEIM_D_14,
    MX6Q_PAD_CSI0_DAT19__WEIM_WEIM_D_15,
	// burst clock
    MX6Q_PAD_EIM_BCLK__WEIM_WEIM_BCLK,
	// read from FPGA (active-L)
    MX6Q_PAD_EIM_OE__WEIM_WEIM_OE,
	// write to FPGA (active-L)
    MX6Q_PAD_EIM_RW__WEIM_WEIM_RW,
	// cs1 64M (active-L)
    MX6Q_PAD_EIM_CS1__WEIM_WEIM_CS_1,
    // KEY_COL4 -- UNUSE
    MX6Q_PAD_KEY_COL4__GPIO_4_14,
    // EIM_D16 -- nCONFIG
    MX6Q_PAD_EIM_D16__GPIO_3_16,
    // EIM_D17 -- nSTATUS
    MX6Q_PAD_EIM_D17__GPIO_3_17,
    // EIM_D18 -- CONF_DONE
    MX6Q_PAD_EIM_D18__GPIO_3_18,
};

// eim device struct
typedef struct _eim_dev
{
	// char device
    struct cdev *cdev;

    // device number
    dev_t devno;
    struct class *eim_class;
    struct device *eim_device;

	// virtual address
    void __iomem *eim_mem_base;
    void __iomem *eim_base;
    void __iomem *ccm_base;
    void __iomem *csi0_dat8_base;
    void __iomem *csi0_dat9_base;

	// device open state
    atomic_t open_state;

    // download mode
    int eim_dmode;

    // mutex lock
    struct mutex eim_mutex_lock;
}eim_dev;
static eim_dev *mdev = NULL;

// ------------------------------------------------------------
// Description :
// 	   This function completes eim-related address mapping.
// Parameters :
//     None.
// Return Value :
//     0 - eim_map success.
// Errors :
//     None.
// -------------------------------------------------------------
static int eim_map(void)
{
	int req_eim_mem_base = 0;
	int req_eim_base = 0;
	int req_ccm_base = 0;
    int req_csi0_dat8_base = 0;
    int req_csi0_dat9_base = 0;

	// eim_mem_base
	req_eim_mem_base = (int)request_mem_region(EIM_MEM_BASE, EIM_MEM_LEN, "EIM_MEM");
    if (0 == req_eim_mem_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : request_mem_region EIM_MEM_BASE failed.\n");
        return -EBUSY;
    }
	mdev->eim_mem_base = ioremap(EIM_MEM_BASE, EIM_MEM_LEN);
    if (!mdev->eim_mem_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : ioremap EIM_MEM_BASE failed.\n");
        return -EBUSY;
    }

	// eim_base
	req_eim_base = (int)request_mem_region(EIM_BASE, EIM_LEN, "EIM");
    if (0 == req_eim_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : request_mem_region EIM_BASE failed.\n");
        return -EBUSY;
    }
	mdev->eim_base = ioremap(EIM_BASE, EIM_LEN);
    if (!mdev->eim_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : ioremap EIM_BASE failed.\n");
        return -EBUSY;
    }

	// ccm_base
	req_ccm_base = (int)request_mem_region(CCM_BASE, CCM_LEN, "CCM");
    if (0 == req_ccm_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : request_mem_region CCM_BASE failed.\n");
        return -EBUSY;
    }
    mdev->ccm_base = ioremap(CCM_BASE, CCM_LEN);
    if (!mdev->ccm_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : ioremap CCM_BASE failed.\n");
        return -EBUSY;
    }

    // csi0_dat8_base
    req_csi0_dat8_base = (int)request_mem_region(CSI0_DAT8_BASE, CSI0_DAT8_LEN, "CSI0_DAT8");
    if (0 == req_csi0_dat8_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : request_mem_region CSI0_DAT8_BASE failed.\n");
        return -EBUSY;
    }
	mdev->csi0_dat8_base = ioremap(CSI0_DAT8_BASE, CSI0_DAT8_LEN);
    if (!mdev->csi0_dat8_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : ioremap CSI0_DAT8_BASE failed.\n");
        return -EBUSY;
    }

    // csi0_dat9_base
    req_csi0_dat9_base = (int)request_mem_region(CSI0_DAT9_BASE, CSI0_DAT9_LEN, "CSI0_DAT9");
    if (0 == req_csi0_dat9_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : request_mem_region CSI0_DAT9_BASE failed.\n");
        return -EBUSY;
    }
	mdev->csi0_dat9_base = ioremap(CSI0_DAT9_BASE, CSI0_DAT9_LEN);
    if (!mdev->csi0_dat9_base)
    {
        printk(KERN_ERR "< eim.c > eim_map : ioremap CSI0_DAT9_BASE failed.\n");
        return -EBUSY;
    }

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function cancels eim-related address mapping.
// Parameters :
//     None.
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
static void eim_unmap(void)
{
	// eim_mem_base
    if (mdev->eim_mem_base)
    {
        iounmap(mdev->eim_mem_base);
        release_mem_region(EIM_MEM_BASE, EIM_MEM_LEN);
    }

	// eim_base
    if (mdev->eim_base)
    {
        iounmap(mdev->eim_base);
        release_mem_region(EIM_BASE, EIM_LEN);
    }

	// ccm_base
    if (mdev->ccm_base)
    {
        iounmap(mdev->ccm_base);
        release_mem_region(CCM_BASE, CCM_LEN);
    }

    // csi0_dat8_base
    if (mdev->csi0_dat8_base)
    {
        iounmap(mdev->csi0_dat8_base);
        release_mem_region(CSI0_DAT8_BASE, CSI0_DAT8_LEN);
    }

    // csi0_dat9_base
    if (mdev->csi0_dat9_base)
    {
        iounmap(mdev->csi0_dat9_base);
        release_mem_region(CSI0_DAT9_BASE, CSI0_DAT9_LEN);
    }
}

// ------------------------------------------------------------
// Description :
// 	   This function sets value of different bit fields in one 32-bit register.
// Parameters :
//     shift - bit offeset
// 	   bits - the number of bits
//     val - value
// Return Value :
//     specified bit-offset value
// Errors :
//     None.
// -------------------------------------------------------------
static u32 bitfield(int shift, int bits, int val)
{
	return ( (val & ( (1 << bits ) - 1 ) ) << shift);
}

// ------------------------------------------------------------
// Description :
// 	   This function completes eim iomux configuration.
// Parameters :
//     eim_mum - eim iomux mode (1 is mux and 0 is NOT mux)
// Return Value :
//     0 - eim_config success.
// Errors :
//     None.
// -------------------------------------------------------------
static int eim_iomux(int eim_mum)
{
    if (1 == eim_mum)
    {
        mxc_iomux_v3_setup_multiple_pads(eim_mux_pads, ARRAY_SIZE(eim_mux_pads));
    }
    else if (0 == eim_mum)
    {
        mxc_iomux_v3_setup_multiple_pads(eim_nomux_pads, ARRAY_SIZE(eim_nomux_pads));
    }
    else
    {
    	printk(KERN_ERR "< eim.c > eim_iomux : the input is invalid. It should be 0 or 1.\n");
    	return -EFAULT;
    }
    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function completes eim-related registers configuration.
// Parameters :
//     None.
// Return Value :
//     0 - eim_config success.
// Errors :
//     None.
// -------------------------------------------------------------
static int eim_config(void)
{
    u32 iomuxc_gpr1_wreg = 0;
	u32 cs1gpr1_wreg = 0;
	u32 cs1gpr2_wreg = 0;
	u32 cs1crc1_wreg = 0;
	u32 cs1crc2_wreg = 0;
	u32 cs1wrc1_wreg = 0;
	u32 cs1wrc2_wreg = 0;
    u32 ccm_ccgr6_rreg = 0;
    u32 ccm_ccgr6_wreg = 0;
    int ret_eim_iomux = 0;

    // set register IOMUXC_GPR1 as CS1(64M) --- ref : IMX6DQRM p1903
    int ADDRS1 	= 1;
    int ACT_CS1 = 1;
    int ADDRS0 	= 1;
    int ACT_CS0 = 1;

	// set register CS1GCR1 --- ref : IMX6DQRM p1038-1041
    int PSZ 	= 0;	// Page Size - 8 words page size
	int WP 		= 0;	// Write Protect - allowed
    int GBC 	= 1;	// Gap Between CS - 1 EIM clock cycle
    int AUS		= 0;	// Address Unshifted - shifted
    int CSREC 	= 1;	// CS Recovery - 1 EIM clock cycle
    int SP 		= 0;	// Supervisor Protect - allowded
    int DSZ 	= 1;	// Port Size - 16 bit port resides on DATA[15:0]
    int BCS 	= 1;	// Wait Cycle Brfore Burst Clock Start - 1 EIM clock cycle
    int BCD 	= 3;	// Burst Clock Divisor - 0(132M) 1(66M) 2(44M) 3(33M)
    int WC 		= 0;	// Write Continuous - according to BL value
    int BL 		= 3;	// Burst Length - 32 words Memory wrap burst length
    int CREP 	= 0;	// Configuration Register Enable Polarity - NC
    int CRE 	= 0;	// Configuration Register Enable - disabled
    int RFL 	= 1;	// Read Fix Latency
    int WFL 	= 1;	// Write Fix Latency
    int MUM 	= 1;	// Multiplexed Mode
    int SRD 	= 1;	// Synchronous Read Data
    int SWR 	= 1;	// Synchronous Write Data
    int CSEN 	= 1;	// CS Enable

	// set register CS1GCR2 --- ref : IMX6DQRM p1042-1043
    int M16BG   = 1;	// Muxed 16 bypass grant - EIM ignores the grant signal
	int DAP     = 0;	// Data Acknowledge Polarity
    int DAE     = 0;	// Data Acknowledge Enable
    int DAPS    = 0;	// Data Acknowledge Poling Start
    int ADH     = 0;	// Address Hold Time

    // set register CS1RCR1 --- ref : IMX6DQRM p1043-1045
    int RWSC 	= 3;	// Read Wait State Control - The first 3 burst clocks are ignored
	int RADVA 	= 0;	// ADV Assertion
	int RAL		= 0;	// Read ADV Low
	int RADVN	= 0;	// ADV Negtation
	int OEA		= 0;	// OE Assertion
	int OEN		= 0;	// OE Negtation
	int RCSA	= 0;	// Read CS Assertion
	int RCSN	= 0;	// Read CS Negation

    // set register CS1RCR2 --- ref : IMX6DQRM p1046-1047
    int APR 	= 0;	// Asynchronous Page Read
	int PAT 	= 7;	// Page Access Time - Address width is 9 EIM clock cycles
	int RL		= 0;	// Read Latency
	int RBEA	= 0;	// Read BE Assertion
	int RBE		= 0;	// Read BE enable
	int RBEN	= 0;	// Read BE Negation

	// set register CS1WCR1 --- ref : IMX6DQRM p1047-1050
    int WAL 	= 0;	// Write ADV Low
	int WBED 	= 0;	// Write Byte Enable Disable
	int WWSC	= 1;	// Write Wait State Control - The first burst clock is ignored
	int WADVA	= 0;	// ADV Assertion
	int WADVN	= 0;	// ADV Negation
	int WBEA	= 0;	// BE Assertion
	int WBEN	= 0;	// BE[3:0] Negation
	int WEA		= 0;	// WE Assertion
	int WEN		= 0;	// WE Negation
	int WCSA	= 0;	// Write CS Assertion
	int WCSN	= 0;	// Write CS Negation

    // set register CS1WCR2 --- ref : IMX6DQRM p1050
    int WBCDD 	= 0;	// Write Burst Clock Divisor Decrement, NC

    // enable eim clock --- ref : IMX6DQRM p894-895
    int EC = 3;	        // EIM Clock

    // IOMUXC_GPR1
    iomuxc_gpr1_wreg = bitfield(4, 2, ADDRS1) |
    	      	 	   bitfield(3, 1, ACT_CS1) |
    	       		   bitfield(1, 2, ADDRS0) |
    	       		   bitfield(0, 1, ACT_CS0);
    mxc_iomux_set_gpr_register(1, 0, 6, iomuxc_gpr1_wreg);

	// CS1GCR1
	cs1gpr1_wreg = bitfield(28, 4, PSZ) |
				   bitfield(27, 1, WP) |
				   bitfield(24, 3, GBC) |
				   bitfield(23, 1, AUS) |
				   bitfield(20, 3, CSREC) |
				   bitfield(19, 1, SP) |
				   bitfield(16, 3, DSZ) |
				   bitfield(14, 2, BCS) |
				   bitfield(12, 2, BCD) |
				   bitfield(11, 1, WC) |
				   bitfield(8, 3, BL) |
				   bitfield(7, 1, CREP) |
				   bitfield(6, 1, CRE) |
				   bitfield(5, 1, RFL) |
				   bitfield(4, 1, WFL) |
				   bitfield(3, 1, MUM) |
				   bitfield(2, 1, SRD) |
				   bitfield(1, 1, SWR) |
				   bitfield(0, 1, CSEN);
    iowrite32(cs1gpr1_wreg, mdev->eim_base + 0x18);

	// CS1GCR2
	cs1gpr2_wreg = bitfield(12, 1, M16BG) |
				   bitfield(9, 1, DAP) |
				   bitfield(8, 3, DAE) |
				   bitfield(4, 4, DAPS) |
				   bitfield(0, 2, ADH);
    iowrite32(cs1gpr2_wreg, mdev->eim_base + 0x1C);

    // CS1RCR1
	cs1crc1_wreg = bitfield(24, 6, RWSC) |
				   bitfield(20, 3, RADVA) |
				   bitfield(19, 1, RAL) |
				   bitfield(16, 3, RADVN) |
				   bitfield(12, 3, OEA) |
				   bitfield(8, 3, OEN) |
				   bitfield(4, 3, RCSA) |
				   bitfield(0, 3, RCSN);
    iowrite32(cs1crc1_wreg, mdev->eim_base + 0x20);

    // CS1RCR2
	cs1crc2_wreg = bitfield(15, 1, APR) |
				   bitfield(12, 3, PAT) |
				   bitfield(8, 2, RL) |
				   bitfield(4, 3, RBEA) |
				   bitfield(3, 1, RBE) |
				   bitfield(0, 3, RBEN);
    iowrite32(cs1crc2_wreg, mdev->eim_base + 0x24);

	// CS1WCR1
	cs1wrc1_wreg = bitfield(31, 1, WAL) |
				   bitfield(30, 1, WBED) |
		 		   bitfield(24, 6, WWSC) |
		 		   bitfield(21, 3, WADVA) |
		 		   bitfield(18, 3, WADVN) |
		 		   bitfield(15, 3, WBEA) |
		 		   bitfield(12, 3, WBEN) |
				   bitfield(9, 3, WEA) |
				   bitfield(6, 3, WEN) |
				   bitfield(3, 3, WCSA ) |
				   bitfield(0, 3, WCSN);
    iowrite32(cs1wrc1_wreg, mdev->eim_base + 0x28);

    // CS1WCR2
	cs1wrc2_wreg = bitfield(0, 1, WBCDD);
    iowrite32(cs1wrc2_wreg, mdev->eim_base + 0x2C);

    // enable eim clock
    ccm_ccgr6_rreg = ioread32(mdev->ccm_base + 0x80);
    ccm_ccgr6_wreg = bitfield(10, 2, EC);
    iowrite32(ccm_ccgr6_rreg | ccm_ccgr6_wreg, mdev->ccm_base + 0x80);

    // eim iomux configuration
    ret_eim_iomux = eim_iomux(MUM);
    if (ret_eim_iomux)
    {
        return -EFAULT;
    }

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function ensures that eim device can be only opened once .
// Parameters :
//     None.
// Return Value :
//     0 - eim_open success.
// Errors :
//     None.
// -------------------------------------------------------------
static int eim_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

    if (!mdev)
    {
        printk(KERN_ERR "< eim.c > eim_open : eim device is not valid (mdev is NULL).\n");
        return -EFAULT;
    }

	ret = atomic_dec_and_test(&mdev->open_state);
    if (0 == ret)
    {
        printk(KERN_ERR "< eim.c > eim_open : eim device has been opened already.\n");
        atomic_inc(&mdev->open_state);
        return -EBUSY;
    }

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim open.\n");
#endif

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function releases all the resources eim device uses.
// Parameters :
//     None.
// Return Value :
//     0 - eim_release success.
// Errors :
//     None.
// -------------------------------------------------------------
static int eim_release(struct inode *inode, struct file *filp)
{
    if (!mdev)
    {
        printk(KERN_ERR "< eim.c > eim_release : eim device is not valid (mdev is NULL).\n");
        return -EFAULT;
    }

    atomic_inc(&mdev->open_state);

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim release.\n");
#endif

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function implements write file operation.
// Parameters :
//	   filp - object file
//	   buf - buffer pointer in user space
//	   count - the actual number of data to written
//	   fops - the offset of data
// Return Value :
//     positive value - the actual number of data copied
//	   negative value - copy_from_user error
// Errors :
//     None.
// -------------------------------------------------------------
static ssize_t eim_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
    int mode = 0;
    mode = mdev->eim_dmode;
    if (DOWNLOAD_PROGRAM == mode)
    {
        int ret = 0;

        // put nCONFIG low and then pull it up
        DRIVE_nCONFIG_LOW();
        ndelay(500);
        DRIVE_nCONFIG_HIGH();

        // check nSTATUS if it is desserted or not
        if (READ_nSTATUS())
        {
            printk("< eim.c > fpp_write : nSTATUS is still high.\n");
            return -EFAULT;
        }

        // check nSTATUS if it is asserted or not
        udelay(230);
        if (!READ_nSTATUS())
        {
            printk("< eim.c > eim_write : nSTATUS is still low.\n");
            return -EFAULT;
        }

        // delay more than 2 us and then configure FPGA
        udelay(2);

        // drive config_data on data bus
        // config_data should be driven on the bus on the rising edge of DCLK
	    ret = copy_from_user((void *)mdev->eim_mem_base, buf, min(EIM_MEM_LEN, (int)count));
	    if (ret)
	    {
	        printk(KERN_ERR "< eim.c > eim_write : copy_from_user failed.\n");
	        return -EFAULT;
	    }

        // check CONF_DONE if it is asserted or not
        if (!READ_CONF_DONE())
        {
            printk("< eim.c > eim_write : CONF_DONE is still low.\n");
            return -EFAULT;
        }

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim write : download FPGA program.\n");
#endif

    }
    else if (DOWNLOAD_PARAMETERS == mode)
    {
        int ret = 0;
	    ret = copy_from_user((void *)mdev->eim_mem_base, buf, min(EIM_MEM_LEN, (int)count));
	    if (ret)
	    {
	        printk(KERN_ERR "< eim.c > eim_write : copy_from_user failed.\n");
	        return -EFAULT;
	    }

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim write : download front-end parameters.\n");
#endif

    }
    else
    {
        printk(KERN_ERR "< eim.c > eim_write : invalid dmode %d.\n", mode);
	    return -EFAULT;
    }

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim write.\n");
#endif

    return min(EIM_MEM_LEN, (int)count);
}

// ------------------------------------------------------------
// Description :
// 	   This function implements read file operation.
// Parameters :
//	   filp - object file
//	   buf - buffer pointer in user space
//	   count - the actual number of data to read
//	   fops - the offset of data
// Return Value :
//     positive value - the actual number of data read
//	   negative value - copy_to_user error
// Errors :
//     None.
// -------------------------------------------------------------
static ssize_t eim_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
	int ret = 0;
	ret = copy_to_user(buf, (void *)mdev->eim_mem_base, min(EIM_MEM_LEN, (int)count));
    if (ret)
    {
    	printk(KERN_ERR "< eim.c > eim_read : copy_to_user failed.\n");
        return -EFAULT;
    }

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim read.\n");
#endif

    return min(EIM_MEM_LEN, (int)count);
}

// ------------------------------------------------------------
// Description :
// 	   This function implements IO control file operation.
// Parameters :
//	   filp - object file
//	   cmd - command
//	   arg - arguments of command above
// Return Value :
//	   0 - eim_ioctl success
// Errors :
//     None.
// ------------------------------------------------------------
static long eim_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	// Nothing to do

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim IO control.\n");
    printk(KERN_INFO "< eim.c > cmd:%d, arg:%ld.\n", cmd, arg);
#endif

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function maps physical memory into user space.
// Parameters :
//	   filp - object file
//	   vma - virtual memory area struct
// Return Value :
//	   0 - eim_mmap success
// Errors :
//     None.
// ------------------------------------------------------------
static int eim_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;

    vma->vm_flags |= VM_RESERVED;
    vma->vm_flags |= VM_IO;

	ret = remap_pfn_range(vma, vma->vm_start, EIM_MEM_BASE >> PAGE_SHIFT, EIM_MEM_LEN, PAGE_SHARED);
    if (ret)
    {
        printk(KERN_ERR "< eim.c > eim_mmap : remap_pfn_range failed.\n");
        return -ENXIO;
    }

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim memory map.\n");
#endif

    return 0;
}

static struct file_operations eim_fops =
{
    .owner              =	THIS_MODULE,
    .open               =   eim_open,
    .release            =   eim_release,
    .write              =   eim_write,
    .read               =   eim_read,
    .unlocked_ioctl     =   eim_ioctl,
    .mmap               =   eim_mmap,
};

// READ & WRITE methods of '/sys/class/eim/eim/dmode' device attribute
static ssize_t eim_dmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int eim_dmode = 0;

	mutex_lock(&mdev->eim_mutex_lock);
    eim_dmode = mdev->eim_dmode;
	mutex_unlock(&mdev->eim_mutex_lock);

	return sprintf(buf, "%d\n", eim_dmode);
}

static ssize_t eim_dmode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int eim_dmode = 0;

	eim_dmode = simple_strtoul(buf, NULL, 10);
    eim_dmode = eim_dmode <DOWNLOAD_PROGRAM ? DOWNLOAD_PROGRAM : eim_dmode;
	eim_dmode = eim_dmode > DOWNLOAD_PARAMETERS ? DOWNLOAD_PARAMETERS : eim_dmode;

	mutex_lock(&mdev->eim_mutex_lock);
    mdev->eim_dmode = eim_dmode;
	mutex_unlock(&mdev->eim_mutex_lock);

	return count;
}

// READ & WRITE methods of '/sys/class/eim/eim/MUM' device attribute
static ssize_t eim_mum_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int eim_mum = 0;
    int eim_mum_mask = 1;
    u32 cs1gpr1_rreg = 0;

	mutex_lock(&mdev->eim_mutex_lock);
    cs1gpr1_rreg = ioread32(mdev->eim_base + 0x18);
    eim_mum = ( cs1gpr1_rreg & bitfield(3, 1, eim_mum_mask) ) >> 3;
	mutex_unlock(&mdev->eim_mutex_lock);

	return sprintf(buf, "%d\n", eim_mum);
}

static ssize_t eim_mum_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int eim_mum = 0;
    int eim_mum_mask = 1;
    u32 cs1gpr1_rreg = 0;
    u32 cs1gpr1_wreg = 0;

	eim_mum = simple_strtoul(buf, NULL, 10);
	eim_mum = eim_mum < 0 ? 0 : eim_mum;
	eim_mum = eim_mum > eim_mum_mask ? eim_mum_mask : eim_mum;

	mutex_lock(&mdev->eim_mutex_lock);
    cs1gpr1_rreg = ioread32(mdev->eim_base + 0x18);
    cs1gpr1_wreg = (cs1gpr1_rreg & ~bitfield(3, 1, eim_mum_mask)) | bitfield(3, 1, eim_mum);
    iowrite32(cs1gpr1_wreg, mdev->eim_base + 0x18);
    eim_iomux(eim_mum);
	mutex_unlock(&mdev->eim_mutex_lock);

	return count;
}

// READ & WRITE methods of '/sys/class/eim/eim/BCD' device attribute
static ssize_t eim_bcd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int eim_bcd = 0;
    int eim_bcd_mask = 3;
	u32 cs1gpr1_rreg = 0;

	mutex_lock(&mdev->eim_mutex_lock);
    cs1gpr1_rreg = ioread32(mdev->eim_base + 0x18);
    eim_bcd = ( cs1gpr1_rreg & bitfield(12, 2, eim_bcd_mask) ) >> 12;
	mutex_unlock(&mdev->eim_mutex_lock);

	return sprintf(buf, "%d\n", eim_bcd);
}

static ssize_t eim_bcd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int eim_bcd = 0;
    int eim_bcd_mask = 3;
    u32 cs1gpr1_rreg = 0;
    u32 cs1gpr1_wreg = 0;

	eim_bcd = simple_strtoul(buf, NULL, 10);
    eim_bcd = eim_bcd < 0 ? 0 : eim_bcd;
	eim_bcd = eim_bcd > eim_bcd_mask ? eim_bcd_mask : eim_bcd;

	mutex_lock(&mdev->eim_mutex_lock);
    cs1gpr1_rreg = ioread32(mdev->eim_base + 0x18);
    cs1gpr1_wreg = (cs1gpr1_rreg & ~bitfield(12, 2, eim_bcd_mask)) | bitfield(12, 2, eim_bcd);
    iowrite32(cs1gpr1_wreg, mdev->eim_base + 0x18);
	mutex_unlock(&mdev->eim_mutex_lock);

	return count;
}

// READ & WRITE methods of '/sys/class/eim/eim/WWSC' device attribute
static ssize_t eim_wwsc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int eim_wwsc = 0;
    int eim_wwsc_mask = 63;
	u32 cs1wcr1_rreg = 0;

	mutex_lock(&mdev->eim_mutex_lock);
    cs1wcr1_rreg = ioread32(mdev->eim_base + 0x28);
    eim_wwsc = ( cs1wcr1_rreg & bitfield(24, 6, eim_wwsc_mask) ) >> 24;
	mutex_unlock(&mdev->eim_mutex_lock);

	return sprintf(buf, "%d\n", eim_wwsc);
}

static ssize_t eim_wwsc_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int eim_wwsc = 0;
    int eim_wwsc_mask = 63;
    u32 cs1wcr1_rreg = 0;
    u32 cs1wcr1_wreg = 0;

	eim_wwsc = simple_strtoul(buf, NULL, 10);
    eim_wwsc = eim_wwsc < 0 ? 0 : eim_wwsc;
	eim_wwsc = eim_wwsc > eim_wwsc_mask ? eim_wwsc_mask : eim_wwsc;

	mutex_lock(&mdev->eim_mutex_lock);
    cs1wcr1_rreg = ioread32(mdev->eim_base + 0x28);
    cs1wcr1_wreg = (cs1wcr1_rreg & ~bitfield(24, 6, eim_wwsc_mask)) | bitfield(24, 6, eim_wwsc);
    iowrite32(cs1wcr1_wreg, mdev->eim_base + 0x28);
	mutex_unlock(&mdev->eim_mutex_lock);

	return count;
}

// define device attributes
static DEVICE_ATTR(dmode, S_IRUGO | S_IWUSR, eim_dmode_show, eim_dmode_store);
static DEVICE_ATTR(MUM, S_IRUGO | S_IWUSR, eim_mum_show, eim_mum_store);
static DEVICE_ATTR(BCD, S_IRUGO | S_IWUSR, eim_bcd_show, eim_bcd_store);
static DEVICE_ATTR(WWSC, S_IRUGO | S_IWUSR, eim_wwsc_show, eim_wwsc_store);

// ------------------------------------------------------------
// Description :
// 	   EIM initialization.
// Parameters :
//	   None.
// Return Value :
//	   0 - eim_init success.
// Errors :
//     None.
// ------------------------------------------------------------
static int __init eim_init(void)
{
	int err = 0;
    int ret_alloc_chrdev_region = 0;
    int ret_cdev_add = 0;
    int ret_device_create_file_dmode = 0;
	int ret_device_create_file_mum = 0;
    int ret_device_create_file_bcd = 0;
    int ret_device_create_file_wwsc = 0;
    int ret_eim_map = 0;
    int ret_eim_config_1 = 0;
	int ret_eim_config_2 = 0;

	// allocate memory for eim device
	// kzalloc() is equivalent to kmalloc() and memset()
    mdev = kzalloc(sizeof(eim_dev), GFP_KERNEL);
    if (!mdev)
    {
        err = -ENOMEM;
        goto kfree_mdev;
    }

	// register device major and minor number dynamically
    ret_alloc_chrdev_region = alloc_chrdev_region(&mdev->devno, 0, 1, DEVICE_NAME);
    if (ret_alloc_chrdev_region < 0)
    {
        printk(KERN_ERR "< eim.c > setup_eim : alloc_chrdev region failed.\n");
        goto unregister_mdev;
    }

	// allocate memory for char device
    mdev->cdev = kzalloc(sizeof(struct cdev), GFP_KERNEL);
    if (!mdev->cdev)
    {
        err = -ENOMEM;
        goto kfree_cdev;
    }
    cdev_init(mdev->cdev, &eim_fops);
    mdev->cdev->owner = THIS_MODULE;

    // add char devcie
    ret_cdev_add = cdev_add(mdev->cdev, mdev->devno, 1);
    if (ret_cdev_add)
    {
        printk(KERN_ERR "< eim.c > setup_eim : cdev_add failed");
        goto delete_cdev;
    }

	// initiate open_state / eim_dmode / eim_mutex_lock
    atomic_set(&mdev->open_state, 1);
    mdev->eim_dmode = DOWNLOAD_PARAMETERS;
    mutex_init(&mdev->eim_mutex_lock);

	// create directory '/sys/class/eim/'
    mdev->eim_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (!mdev->eim_class)
    {
        printk(KERN_ERR "< eim.c > setup_eim : class_create failed.\n");
        err = -EFAULT;
        goto destroy_class;
    }

    // create file node '/dev/eim' and '/sys/class/eim/eim'
    mdev->eim_device = device_create(mdev->eim_class, NULL, mdev->devno, NULL, "%s", DEVICE_NAME);
    if (!mdev->eim_device)
    {
        printk(KERN_ERR "< eim.c > setup_eim : device_create failed.\n");
        err = -EFAULT;
        goto destroy_device;
    }

    // create device attribute 'sys/class/eim/eim/dmode'
	// create device attribute 'sys/class/eim/eim/MUM'
    // create device attribute 'sys/class/eim/eim/BCD'
    // create device attribute 'sys/class/eim/eim/WWSC'
    ret_device_create_file_dmode = device_create_file(mdev->eim_device, &dev_attr_dmode);
    if (ret_device_create_file_dmode)
    {
        printk(KERN_ERR "< eim.c > setup_eim : device_create_file dmode failed.\n");
        err = -EFAULT;
        goto destroy_device;
    }
	ret_device_create_file_mum = device_create_file(mdev->eim_device, &dev_attr_MUM);
	if (ret_device_create_file_mum)
	{
		printk(KERN_ERR "< eim.c > setup_eim : device_create_file MUM failed.\n");
		err = -EFAULT;
		goto destroy_device;
	}
    ret_device_create_file_bcd = device_create_file(mdev->eim_device, &dev_attr_BCD);
    if (ret_device_create_file_bcd)
    {
        printk(KERN_ERR "< eim.c > setup_eim : device_create_file BCD failed.\n");
        err = -EFAULT;
        goto destroy_device;
    }
    ret_device_create_file_wwsc = device_create_file(mdev->eim_device, &dev_attr_WWSC);
    if (ret_device_create_file_wwsc)
    {
        printk(KERN_ERR "< eim.c > setup_eim : device_create_file WWSC failed.\n");
        err = -EFAULT;
        goto destroy_device;
    }

	// eim address map
    ret_eim_map = eim_map();
    if (ret_eim_map)
    {
        goto unmap_eim;
    }

	// eim registers configuration twice
    ret_eim_config_1 = eim_config();
    if (ret_eim_config_1)
    {
    	printk(KERN_INFO "< eim.c > setup_eim : eim_config failed first time.\n");
    	err = -EFAULT;
        goto unmap_eim;
    }
    ret_eim_config_2 = eim_config();
    if (ret_eim_config_2)
    {
    	printk(KERN_INFO "< eim.c > setup_eim : eim_config failed second time.\n");
    	err = -EFAULT;
        goto unmap_eim;
    }

    // GPIO init
    gpio_request(GPIO_FPP_UNUSE, "UNUSE");
    gpio_request(GPIO_FPP_nCONFIG, "nCONFIG");
    gpio_request(GPIO_FPP_nSTATUS, "nSTATUS");
    gpio_request(GPIO_FPP_CONF_DONE, "CONF_DONE");
    gpio_direction_input(GPIO_FPP_UNUSE);
    gpio_direction_output(GPIO_FPP_nCONFIG, 1);
    gpio_direction_input(GPIO_FPP_nSTATUS);
    gpio_direction_input(GPIO_FPP_CONF_DONE);
    SET_DAT6_PUSHPULL();
    SET_DAT7_PUSHPULL();

#if DEBUG == 1
	printk(KERN_INFO "eim init.\n");
#endif

    return 0;

unmap_eim :
	eim_unmap();

destroy_device :
	device_destroy(mdev->eim_class, mdev->devno);

destroy_class :
	class_destroy(mdev->eim_class);

delete_cdev :
	cdev_del(mdev->cdev);

kfree_cdev :
	kfree(mdev->cdev);

unregister_mdev :
	unregister_chrdev_region(mdev->devno, 1);

kfree_mdev :
	kfree(mdev);

    return err;
}

// ------------------------------------------------------------
// Description :
// 	   EIM exit.
// Parameters :
//	   None.
// Return Value :
//	   None.
// Errors :
//     None.
// ------------------------------------------------------------
static void __exit eim_exit(void)
{
   	if (mdev)
    {
        eim_unmap();

		if (mdev->eim_device)
		{
			device_destroy(mdev->eim_class, mdev->devno);
		}

		if (mdev->eim_class)
		{
			class_destroy(mdev->eim_class);
		}

        if (mdev->cdev)
        {
            cdev_del(mdev->cdev);
            kfree(mdev->cdev);
        }

        if (mdev->devno)
        {
            unregister_chrdev_region(mdev->devno, 1);
        }

        kfree(mdev);
    }

    // release GPIO
    gpio_free(GPIO_FPP_UNUSE);
    gpio_free(GPIO_FPP_nCONFIG);
    gpio_free(GPIO_FPP_nSTATUS);
    gpio_free(GPIO_FPP_CONF_DONE);

#if DEBUG == 1
    printk(KERN_INFO "< eim.c > eim exit.\n");
#endif
}

module_init(eim_init);
module_exit(eim_exit);

MODULE_AUTHOR("Young");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.2");
MODULE_DESCRIPTION("Freescale i.MX6 EIM port Module");
