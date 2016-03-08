// NAME : fpp driver program (Fast Passive Parallel)
// FUNC : configure FPGA by FPP using GPIO
// DATE : 2013.08.31 by Young
// DESP : misc device architecture
// HIST : V1.0 fpp driver program @ 2013.08.31 by Young
    
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/iomux-mx6q.h>

// print debug information
#define DEBUG 			        (0)

// registers address
#define GPIO5_BASE              (0x020AC000)
#define GPIO5_LEN               (0x1C + 0x04)
#define CSI0_DAT8_BASE          (0x020E0648)        
#define CSI0_DAT8_LEN           (0x04)              
#define CSI0_DAT9_BASE          (0x020E064C)        
#define CSI0_DAT9_LEN           (0x04)

#define DEVICE_NAME 	        "fpp"

// GPIO defination 
#define GPIO_FPP_DCLK           IMX_GPIO_NR(6, 31)      
#define GPIO_FPP_UNUSE          IMX_GPIO_NR(4, 14)      
#define GPIO_FPP_nCONFIG        IMX_GPIO_NR(3, 16)      
#define GPIO_FPP_nSTATUS        IMX_GPIO_NR(3, 17)      
#define GPIO_FPP_CONF_DONE      IMX_GPIO_NR(3, 18)      

// GPIO methods
#define DRIVE_DCLK_LOW()        gpio_set_value(GPIO_FPP_DCLK, 0)
#define DRIVE_DCLK_HIGH()       gpio_set_value(GPIO_FPP_DCLK, 1)

#define DRIVE_nCONFIG_LOW()     gpio_set_value(GPIO_FPP_nCONFIG, 0)
#define DRIVE_nCONFIG_HIGH()    gpio_set_value(GPIO_FPP_nCONFIG, 1)

#define READ_nSTATUS()          gpio_get_value(GPIO_FPP_nSTATUS)
#define READ_CONF_DONE()        gpio_get_value(GPIO_FPP_CONF_DONE)

#define GPIO5_DR                (gpio5_base)
#define GPIO5_GDIR              (gpio5_base + 0x04)
#define DRIVE_DATA(data)        iowrite32( ((data) << 20), GPIO5_DR)
#define SET_DATA_OUTPUT()       iowrite32( ((0xFF) << 20), GPIO5_GDIR)

// set CSI0_DAT8 (I2C1_SDA) & CSI_DAT9 (I2C1_SCL) as push-pull mode (not open-drain mode)
#define CSI0_DAT8_PCR           (csi0_dat8_base)
#define CSI0_DAT9_PCR           (csi0_dat9_base)
#define SET_DAT6_PUSHPULL()     iowrite32(0x0001B0B0, CSI0_DAT8_PCR)
#define SET_DAT7_PUSHPULL()     iowrite32(0x0001B0B0, CSI0_DAT9_PCR)

static iomux_v3_cfg_t fpp_pads[] = {
    // EIM_BCLK -- DCLK
    MX6Q_PAD_EIM_BCLK__GPIO_6_31,
    // KEY_COL4 -- UNUSE
    MX6Q_PAD_KEY_COL4__GPIO_4_14,
    // EIM_D16 -- nCONFIG
    MX6Q_PAD_EIM_D16__GPIO_3_16,
    // EIM_D17 -- nSTATUS
    MX6Q_PAD_EIM_D17__GPIO_3_17,
    // EIM_D18 -- CONF_DONE
    MX6Q_PAD_EIM_D18__GPIO_3_18,
    // EIM_D[0..7] -- DATA[0..7]
    MX6Q_PAD_CSI0_DATA_EN__GPIO_5_20,
    MX6Q_PAD_CSI0_VSYNC__GPIO_5_21,
    MX6Q_PAD_CSI0_DAT4__GPIO_5_22,
    MX6Q_PAD_CSI0_DAT5__GPIO_5_23,
    MX6Q_PAD_CSI0_DAT6__GPIO_5_24,
    MX6Q_PAD_CSI0_DAT7__GPIO_5_25,
    MX6Q_PAD_CSI0_DAT8__GPIO_5_26,  
    MX6Q_PAD_CSI0_DAT9__GPIO_5_27,
}; 

// virtual address
static void __iomem *gpio5_base;
static void __iomem *csi0_dat8_base;
static void __iomem *csi0_dat9_base;

// device open state
static atomic_t open_state;

// ------------------------------------------------------------
// Description :
// 	   This function completes fpp-related address mapping.
// Parameters :
//     None.
// Return Value :
//     0 - fpp_map success.
// Errors :
//     None.
// -------------------------------------------------------------
static int fpp_map(void)
{
	int req_gpio5_base = 0;
    int req_csi0_dat8_base = 0;
    int req_csi0_dat9_base = 0;

    // gpio5_base
	req_gpio5_base = (int)request_mem_region(GPIO5_BASE, GPIO5_LEN, "GPIO5");
    if (0 == req_gpio5_base) 
    {
        printk(KERN_ERR "fpp_map : request_mem_region GPIO5_BASE failed.\n");
        return -EBUSY;
    }  
	gpio5_base = ioremap(GPIO5_BASE, GPIO5_LEN);
    if (!gpio5_base) 
    {
        printk(KERN_ERR "fpp_map : ioremap GPIO5_BASE failed.\n");
        return -EBUSY;
    }    
    
    // csi0_dat8_base
    req_csi0_dat8_base = (int)request_mem_region(CSI0_DAT8_BASE, CSI0_DAT8_LEN, "CSI0_DAT8");
    if (0 == req_csi0_dat8_base)
    {
        printk(KERN_ERR "fpp_map : request_mem_region CSI0_DAT8_BASE failed.\n");
        return -EBUSY;
    }  
	csi0_dat8_base = ioremap(CSI0_DAT8_BASE, CSI0_DAT8_LEN);
    if (!csi0_dat8_base) 
    {
        printk(KERN_ERR "fpp_map : ioremap CSI0_DAT8_BASE failed.\n");
        return -EBUSY;
    }  

    // csi0_dat9_base
    req_csi0_dat9_base = (int)request_mem_region(CSI0_DAT9_BASE, CSI0_DAT9_LEN, "CSI0_DAT9");
    if (0 == req_csi0_dat9_base)
    {
        printk(KERN_ERR "fpp_map : request_mem_region CSI0_DAT9_BASE failed.\n");
        return -EBUSY;
    }  
	csi0_dat9_base = ioremap(CSI0_DAT9_BASE, CSI0_DAT9_LEN);
    if (!csi0_dat9_base) 
    {
        printk(KERN_ERR "fpp_map : ioremap CSI0_DAT9_BASE failed.\n");
        return -EBUSY;
    }

    return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function cancels fpp-related address mapping.
// Parameters :
//     None.
// Return Value :
//     None.
// Errors :
//     None.
// -------------------------------------------------------------
static void fpp_unmap(void)
{
    // gpio5_base
    if (gpio5_base)
    {
        iounmap(gpio5_base);
        release_mem_region(GPIO5_BASE, GPIO5_LEN);
    }

    // csi0_dat8_base
    if (csi0_dat8_base)
    {
        iounmap(csi0_dat8_base);
        release_mem_region(CSI0_DAT8_BASE, CSI0_DAT8_LEN);
    }

    // csi0_dat9_base
    if (csi0_dat9_base)
    {
        iounmap(csi0_dat9_base);
        release_mem_region(CSI0_DAT9_BASE, CSI0_DAT9_LEN);
    }
}
    
// ------------------------------------------------------------
// Description :
// 	   This function ensures that fpp device can be only opened once .
// Parameters :
//     None.
// Return Value :
//     0 - fpp_open success.
// Errors :
//     None.
// -------------------------------------------------------------
static int fpp_open(struct inode *inode, struct file *filp)  
{  
	int ret = 0;
	ret = atomic_dec_and_test(&open_state);
    if (0 == ret) 
    {
        printk(KERN_ERR "fpp_open : fpp device has been opened already.\n");
        atomic_inc(&open_state);
        return -EBUSY;
    }

#if DEBUG == 1
    printk(KERN_INFO "fpp open.\n");  
#endif

    return 0;  
}  

// ------------------------------------------------------------
// Description :
// 	   This function completes atomic inc of open state.
// Parameters :
//     None.
// Return Value :
//     0 - fpp_release success.
// Errors :
//     None.
// -------------------------------------------------------------
static int fpp_release(struct inode *inode, struct file *filp)  
{  
    atomic_inc(&open_state);

#if DEBUG == 1
    printk(KERN_INFO "fpp release.\n");  
#endif

    return 0;  
}  

// ------------------------------------------------------------
// Description :
// 	   This function implements write file operation.
// Parameters :
//	   filp - object file
//	   buf - buffer pointer in user space
//	   count - the actual number of data to write
//	   fops - the offset of data
// Return Value :
//     positive value - the actual number of data copied
//	   negative value - fpp_write error
// Errors :
//     None.
// -------------------------------------------------------------
static ssize_t fpp_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)  
{  
    unsigned char *config_data = NULL;
    int ret = 0;
    int i = 0;

    config_data = (unsigned char*)kmalloc(sizeof(unsigned char) * count, GFP_KERNEL);
    if (!config_data)
    {
        printk(KERN_ERR "fpp_write : kmalloc failed.\n");
        return -EFAULT;
    }

    ret = copy_from_user(config_data, buf, count);
    if (ret) 
    {
        printk(KERN_ERR "fpp_write : copy_from_user failed.\n");
        return -EFAULT;
    }

    // put nCONFIG low and then pull it up
    DRIVE_nCONFIG_LOW();
    ndelay(500);
    DRIVE_nCONFIG_HIGH();

    // check nSTATUS if it is desserted or not 
    if (READ_nSTATUS())
    {
        printk("fpp_write : nSTATUS is still high.\n");
        return -EFAULT;
    }    
    
    // check nSTATUS if it is asserted or not 
    udelay(230);
    if (!READ_nSTATUS())
    {
        printk("fpp_write : nSTATUS is still low.\n");
        return -EFAULT;
    }
    
    // delay more than 2 us and then configure FPGA
    udelay(2);
    
    // drive config_data on data bus
    // config_data should be driven on the bus on the rising edge of DCLK
    for (i = 0; i < count; i++) 
    {
        DRIVE_DATA(config_data[i]);
        DRIVE_DCLK_HIGH(); 
        DRIVE_DCLK_LOW();
    }

    // check CONF_DONE if it is asserted or not
    if (!READ_CONF_DONE())
    {
        printk("fpp_write : CONF_DONE is still low.\n");
        return -EFAULT;
    }     

#if DEBUG == 1
    printk(KERN_INFO "fpp write.\n");  
#endif
    
    return count;
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
//     0 - fpp_read success.
// Errors :
//     None.
// -------------------------------------------------------------
static ssize_t fpp_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)  
{
	// Nothing to do 

#if DEBUG == 1
    printk(KERN_INFO "fpp read.\n");  
#endif

    return 0;  
}  

// ------------------------------------------------------------
// Description :
// 	   This function implements IO control file operation.
// Parameters :
//	   filp - object file
//	   cmd - command 
//	   arg - arguments of command above
// Return Value :
//	   0 - fpp_ioctl success
// Errors :
//     None.
// ------------------------------------------------------------
static long fpp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)  
{  
	// Nothing to do 
	
#if DEBUG == 1
    printk(KERN_INFO "fpp IO control.\n");  
    printk(KERN_INFO "cmd:%d, arg:%ld.\n", cmd, arg);  
#endif

    return 0;  
}  

static struct file_operations fpp_fops =   
{  
    .owner              =	THIS_MODULE,  
    .open               =   fpp_open,  
    .release            =   fpp_release,  
    .write              =   fpp_write,  
    .read               =   fpp_read,  
    .unlocked_ioctl     =   fpp_ioctl,
}; 

struct miscdevice fpp_dev = 
{
    .minor      = MISC_DYNAMIC_MINOR,
    .fops       = &fpp_fops,
    // /sys/class/misc/<DEVICE_NAME>
    .name       = DEVICE_NAME,
    // /dev/<DEVICE_NAME>
    .nodename   = DEVICE_NAME,
};

// ------------------------------------------------------------
// Description :
// 	   FPP initialization.
// Parameters :
//	   None.
// Return Value :
//	   0 - fpp_init success.
// Errors :
//     None.
// ------------------------------------------------------------
static int __init fpp_init(void)  
{
    // register misc device
    int ret = 0;
    ret = misc_register(&fpp_dev);
    if (ret)
    {
        goto unregister_fpp;
    }
    
    // fpp iomux 
    mxc_iomux_v3_setup_multiple_pads(fpp_pads, ARRAY_SIZE(fpp_pads));

    // fpp address map
    ret = fpp_map();
    if (ret) 
    {
        goto unmap_fpp;
    }
    
    // gpio init
    gpio_request(GPIO_FPP_DCLK, "DCLK");
    gpio_request(GPIO_FPP_UNUSE, "UNUSE");
    gpio_request(GPIO_FPP_nCONFIG, "nCONFIG");
    gpio_request(GPIO_FPP_nSTATUS, "nSTATUS");
    gpio_request(GPIO_FPP_CONF_DONE, "CONF_DONE");
    gpio_direction_output(GPIO_FPP_DCLK, 0);
    gpio_direction_input(GPIO_FPP_UNUSE);
    gpio_direction_output(GPIO_FPP_nCONFIG, 1);
    gpio_direction_input(GPIO_FPP_nSTATUS);
    gpio_direction_input(GPIO_FPP_CONF_DONE);
    SET_DAT6_PUSHPULL();
    SET_DAT7_PUSHPULL();
    SET_DATA_OUTPUT();
    DRIVE_DATA(0x00);

	// initiate open_state
    atomic_set(&open_state, 1);

#if DEBUG == 1
	printk(KERN_INFO "fpp init.\n");
#endif

    return 0;

unmap_fpp : 
	fpp_unmap();

unregister_fpp :
    misc_deregister(&fpp_dev);
    
    return ret;
}

// ------------------------------------------------------------
// Description :
// 	   FPP exit.
// Parameters :
//	   None.
// Return Value :
//	   None.
// Errors :
//     None.
// ------------------------------------------------------------
static void __exit fpp_exit(void)  
{  
    // gpio release
    gpio_free(GPIO_FPP_DCLK);
    gpio_free(GPIO_FPP_UNUSE);
    gpio_free(GPIO_FPP_nCONFIG);
    gpio_free(GPIO_FPP_nSTATUS);
    gpio_free(GPIO_FPP_CONF_DONE);

    // fpp address map
    fpp_unmap();

    // deregister misc device
    misc_deregister(&fpp_dev);
    
#if DEBUG == 1
    printk(KERN_INFO "fpp exit.\n");  
#endif
}

module_init(fpp_init);  
module_exit(fpp_exit);  

MODULE_AUTHOR("Young");
MODULE_LICENSE("GPL");  
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Configure FPGA by FPP using GPIO"); 
