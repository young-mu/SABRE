// NAME : sdma_m2m driver program
// FUNC : Direct Memory Access from Memory to Memory
// DATE : 2013.11.07 by Young
// DESP : DMA wbuf -> rbuf (mmap to USER_SPACE buffer)
// HIST : V1.0 2013.11.07 - sdma_m2m driver program
//		  V1.1 2013.11.16 - add sdev & DMA initilization
//		  V1.2 2013.11.19 - add ring buffer

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <mach/dma.h>
#include <linux/uaccess.h>  
#include <linux/completion.h>
#include <linux/signal.h>
#include <linux/delay.h>

#define DEVICE_NAME			"sdma_m2m"
#define SDMA_M2M_WBUF		(1024)						// 1KB
#define RBUF_CNT			16
#define SDMA_M2M_RBUF  		(RBUF_CNT * SDMA_M2M_WBUF)	// 16KB
#define DEBUG 				0

// sdma_m2m device struct
typedef struct _sdma_m2m_dev
{
	// major number
	int gMajor;
    
	// class and device
	struct class *sdma_m2m_class;
	struct device *sdma_m2m_device;
    
	// DMA channel
	struct dma_chan *dma_m2m_chan;

	// DMA tx descriptor
	struct dma_async_tx_descriptor *dma_m2m_desc;

	// DMA configuration
	struct dma_slave_config dma_m2m_config;

	// scatterlist
	struct scatterlist sg1;
	struct scatterlist sg2;

	// write and read buffer
	unsigned char *wbuf;
	unsigned char *rbuf;
	int rbuf_cnt;
    
	// device open state
    atomic_t open_state;

	// DMA completion
	// ensures that DMA work has been completed before read returns
	struct completion dma_callback_ok;

}sdma_m2m_dev;
static sdma_m2m_dev *sdev = NULL;

// ------------------------------------------------------------
// Description :
// 	   This function ensures that device can be open only once.
// Parameters :
//     None.
// Return Value :
//     0 - sdma_m2m_open success.
// Errors :
//     None.
// -------------------------------------------------------------
int sdma_m2m_open(struct inode * inode, struct file * filp)
{
	int ret = 0;
 
    if (!sdev) 
    {
        printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_open : sdma_m2m device is not valid (sdev is NULL).\n");
        return -EFAULT;
    }

	ret = atomic_dec_and_test(&sdev->open_state);
    if (0 == ret) 
    {
        printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_open : sdma_m2m device has been opened already.\n");
        atomic_inc(&sdev->open_state);
        return -EBUSY;
    }

	return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function controls open state of device.
// Parameters :
//     None.
// Return Value :
//     0 - sdma_m2m_release success.
// Errors :
//     None.
// -------------------------------------------------------------
int sdma_m2m_release(struct inode *inode, struct file * filp)
{
    if (!sdev) 
    {
        printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_release : sdma_m2m device is not valid (sdev is NULL).\n");
        return -EFAULT;
    }

    atomic_inc(&sdev->open_state);

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
ssize_t sdma_m2m_write(struct file * filp, const char __user * buf, size_t count, loff_t * offset)
{
    int ret = 0;

	ret = copy_from_user(sdev->wbuf, (void *)buf, count);
    if (ret) 
    {
    	printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_write : copy_to_user failed.\n");
        return -EFAULT;
    }

	return 0;
}

static void dma_m2m_callback(void *data)
{
#if DEBUG == 1
	// print its own function name
	printk(KERN_INFO "< sdma_m2m.c > dma_m2m_callback : %s.\n", __func__);
#endif

	// trigger wait_for_completion process
	complete(&sdev->dma_callback_ok);
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
ssize_t sdma_m2m_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
	// correspond sg1 and wbuf
	sg_init_one(&sdev->sg1, sdev->wbuf, count);
	dma_map_sg(NULL, &sdev->sg1, 1, sdev->dma_m2m_config.direction);
	sdev->dma_m2m_desc = sdev->dma_m2m_chan->device->device_prep_slave_sg(sdev->dma_m2m_chan, &sdev->sg1, 1, sdev->dma_m2m_config.direction, 1);
    if (!sdev->dma_m2m_desc)
    {
        printk(KERN_INFO "< sdma_m2m.c > sdma_m2m_write : device_prep_slave_sg wbuf failed.\n");
        return -1;
    }

	// correspond sg2 and rbuf
	sg_init_one(&sdev->sg2, sdev->rbuf + sdev->rbuf_cnt * 1024, count);
	// change index of ring buffer
	sdev->rbuf_cnt = (sdev->rbuf_cnt + 1) % RBUF_CNT;		
	dma_map_sg(NULL, &sdev->sg2, 1, sdev->dma_m2m_config.direction);
	sdev->dma_m2m_desc = sdev->dma_m2m_chan->device->device_prep_slave_sg(sdev->dma_m2m_chan, &sdev->sg2, 1, sdev->dma_m2m_config.direction, 0);
    if (!sdev->dma_m2m_desc)
    {
        printk(KERN_INFO "< sdma_m2m.c > sdma_m2m_write : device_prep_slave_sg rbuf failed.\n");
        return -1;
    }

	// set callback function
	sdev->dma_m2m_desc->callback = dma_m2m_callback;

	// add to the DMA transferring queue
	dmaengine_submit(sdev->dma_m2m_desc);	

	// start DMA transferring
	sdev->dma_m2m_chan->device->device_issue_pending(sdev->dma_m2m_chan);	

#if DEBUG == 1
	// delay 5s
	ssleep(5);
#endif

	// wait for DMA callback function completion
	// ensure that DMA work has been completed before read returns
	wait_for_completion(&sdev->dma_callback_ok);	

	// release resources
	dma_unmap_sg(NULL, &sdev->sg1, 1, sdev->dma_m2m_config.direction);
	dma_unmap_sg(NULL, &sdev->sg2, 1, sdev->dma_m2m_config.direction);

	return 0;
}

// ------------------------------------------------------------
// Description :
// 	   This function maps physical memory into user space.
// Parameters :
//	   filp - object file
//	   vma - virtual memory area struct
// Return Value :
//	   0 - sdma_m2m_mmap success
// Errors :
//     None.
// ------------------------------------------------------------
static int sdma_m2m_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long page = virt_to_phys(sdev->rbuf);

	vma->vm_flags |= (VM_IO | VM_DONTEXPAND);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = io_remap_pfn_range(vma, vma->vm_start, page >> PAGE_SHIFT, size, vma->vm_page_prot);
	if (ret)
	{
		return -EAGAIN;
	}

	return 0;
}

struct file_operations sdma_m2m_fops = 
{
	.owner		=	THIS_MODULE,
	.open		=	sdma_m2m_open,
	.release	=	sdma_m2m_release,
	.write		=	sdma_m2m_write,
	.read		=	sdma_m2m_read,
    .mmap		=	sdma_m2m_mmap,
};

static bool dma_m2m_filter(struct dma_chan *chan, void *param)
{
	if (!imx_dma_is_general_purpose(chan))
	{
		return false;
	}

	chan->private = param;
	return true;
}

// ------------------------------------------------------------
// Description :
// 	   SDMA_M2M initialization.
// Parameters :
//	   None.
// Return Value :
//	   0 - sdma_m2m_init success.
// Errors :
//     None.
// ------------------------------------------------------------
static int __init sdma_m2m_init(void)
{
	int err = 0;
	dma_cap_mask_t dma_m2m_mask;
	struct imx_dma_data m2m_dma_data = {0};

	dma_cap_zero(dma_m2m_mask);
	dma_cap_set(DMA_SLAVE, dma_m2m_mask);

	m2m_dma_data.peripheral_type = IMX_DMATYPE_MEMORY;
	m2m_dma_data.priority = DMA_PRIO_HIGH;

	// allocate memory for sdma_m2m device
    sdev = kzalloc(sizeof(sdma_m2m_dev), GFP_KERNEL);
    if (!sdev) 
    {
        err = -ENOMEM;
        goto kfree_sdev;
    }

	// initiate rbuf_cnt / open_state / dma_callback_ok
	sdev->rbuf_cnt = 0;	
	atomic_set(&sdev->open_state, 1);
	init_completion(&sdev->dma_callback_ok);

	// request DMA channel
	sdev->dma_m2m_chan = dma_request_channel(dma_m2m_mask, dma_m2m_filter, &m2m_dma_data);
	if (!sdev->dma_m2m_chan) 
	{
		printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_open : dma_request_channel failed.\n");
		return -EINVAL;
	}

	// configure DMA channel 
	sdev->dma_m2m_config.direction = DMA_MEM_TO_MEM;
	sdev->dma_m2m_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	dmaengine_slave_config(sdev->dma_m2m_chan, &sdev->dma_m2m_config);

	// write buffer 1KB
	sdev->wbuf = kzalloc(SDMA_M2M_WBUF, GFP_DMA);
	if (!sdev->wbuf) 
	{
		printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_open : kzalloc wbuf failed.\n");
		goto kfree_wbuf;
	}
	
	// read buffer 16KB
    sdev->rbuf = kzalloc(SDMA_M2M_RBUF, GFP_DMA);
	if (!sdev->rbuf) 
	{
		printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_open : kzalloc rbuf failed.\n");
		goto kfree_rbuf;
	}

	// register a character device
	sdev->gMajor = register_chrdev(0, DEVICE_NAME, &sdma_m2m_fops);
	if (sdev->gMajor < 0) 
	{
		printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_init : register_chrdev failed.\n");
		err = -EFAULT;
		goto unregister_chrdev;
	}

	// create directory '/sys/class/sdma_m2m/'
	sdev->sdma_m2m_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (!sdev->sdma_m2m_class) 
	{
        printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_init : class_create failed.\n");
		err = -EFAULT;
		goto destroy_class;
	}

	// create file node '/dev/sdma_m2m' and '/sys/class/sdma_m2m/sdma_m2m'
	sdev->sdma_m2m_device = device_create(sdev->sdma_m2m_class, NULL, MKDEV(sdev->gMajor, 0), NULL, "%s", DEVICE_NAME);
	if (!sdev->sdma_m2m_device) 
	{
		printk(KERN_ERR "< sdma_m2m.c > sdma_m2m_init : device_create failed.\n");
		err = -EFAULT;
		goto destroy_device;
	}

#if DEBUG == 1
	printk(KERN_INFO "< sdma_m2m.c > sdma_m2m init.\n");
#endif

	return 0;

destroy_device:
	device_destroy(sdev->sdma_m2m_class, MKDEV(sdev->gMajor, 0));

destroy_class:
	class_destroy(sdev->sdma_m2m_class);

unregister_chrdev:
	unregister_chrdev(sdev->gMajor, DEVICE_NAME);

kfree_rbuf:
	kfree(sdev->rbuf);

kfree_wbuf:
	kfree(sdev->wbuf);

kfree_sdev:
	kfree(sdev);

	return err;
}

// ------------------------------------------------------------
// Description :
// 	   SDMA exit.
// Parameters :
//	   None.
// Return Value :
//	   None.
// Errors :
//     None.
// ------------------------------------------------------------
static void sdma_m2m_exit(void)
{
	if (sdev)
	{
		if (sdev->sdma_m2m_device)
		{
			device_destroy(sdev->sdma_m2m_class, MKDEV(sdev->gMajor, 0));
		}
		if (sdev->sdma_m2m_class)
		{
			class_destroy(sdev->sdma_m2m_class);
		}
		unregister_chrdev(sdev->gMajor, DEVICE_NAME);
		
		if (sdev->rbuf)
		{
			kfree(sdev->rbuf);
			sdev->rbuf = NULL;
		}
		if (sdev->wbuf)
		{
			kfree(sdev->wbuf);
			sdev->wbuf = NULL;
		}
		if (sdev->dma_m2m_chan)
		{
			dma_release_channel(sdev->dma_m2m_chan);
			sdev->dma_m2m_chan = NULL;
		}	
	}

#if DEBUG == 1
	printk(KERN_INFO "< sdma_m2m.c > sdma_m2m exit.\n");
#endif
}

module_init(sdma_m2m_init);
module_exit(sdma_m2m_exit);

MODULE_AUTHOR("Young");
MODULE_LICENSE("GPL");  
MODULE_VERSION("1.2");
MODULE_DESCRIPTION("Freescale i.MX6 SDMA_M2M Module"); 
