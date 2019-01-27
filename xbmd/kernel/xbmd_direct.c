#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
//#include <linux/pci-aspm.h>
//#include <linux/pci_regs.h>

#include <linux/uaccess.h>   /* copy_to_user */

#include "xbmd.h"

typedef struct xbmd_device
{
    char *name;
    
    unsigned long baseAddr;
    unsigned long baseLength;
    void __iomem *baseVirtual;
    
    struct pci_dev *pciDev;
    int             irq;
    
    dma_addr_t    h2dAddr;
    u8           *h2dBuffer;
    dma_addr_t    d2hAddr;
    u8           *d2hBuffer;
    
    u32           dmaH2DStart;
    u32           dmaH2DDone;
    u32           dmaD2HStart;
    u32           dmaD2HDone;
    
    struct cdev   chrDev;
    
    wait_queue_head_t dmaD2HQueue;
    wait_queue_head_t dmaH2DQueue;
    struct semaphore  dmaSem;
} xbmd_device;

#define RW_MAX_BYTES 4096

// Defines the Vendor ID.  Must be changed if core generated did not set the Vendor ID to the same value
#define PCI_VENDOR_ID_XILINX      0x10ee

// Defines the Device ID.  Must be changed if core generated did not set the Device ID to the same value
#define PCI_DEVICE_ID_XILINX_PCIE 0x7011

int             gDrvrMajor = 245;           // Major number not dynamic.
int             gDrvrMinor = 0;


static void xpcie_write_reg(xbmd_device *dev, u32 dwOffset, u32 val)
{
    writel(val, dev->baseVirtual + (4 * dwOffset));
}

static u32 xpcie_read_reg(xbmd_device *dev, u32 dwOffset)
{
    u32 result = readl(dev->baseVirtual + (4 * dwOffset));
    return result;
}

irqreturn_t xpcie_irq_msi_handler(int irq, void *dev_id)
{
    xbmd_device *dev = dev_id;
    u32 reg = 0;
    
    printk(KERN_WARNING"%s: Interrupt Handler Start ..", dev->name);
    
    reg = xpcie_read_reg(dev, Reg_DeviceDMACS);
    if ((reg & 0x1) && (reg & 0x100)) {
        printk(KERN_WARNING"%s: Device to host interrupt ..\n", dev->name);
        
        dev->dmaD2HDone = 1;
        wake_up_interruptible(&dev->dmaD2HQueue);
    }
    
    if ((reg & 0x10000) && (reg & 0x1000000)) {
        printk(KERN_WARNING"%s: Host to device interrupt ..\n", dev->name);
        
        dev->dmaH2DDone = 1;
        wake_up_interruptible(&dev->dmaH2DQueue);
    }
    
    printk(KERN_WARNING"%s: Interrupt Handler End ..\n", dev->name);
    
    return IRQ_HANDLED;
}

static void xpcie_initiator_reset(xbmd_device *dev)
{
    xpcie_write_reg(dev, Reg_DeviceCS, 1);
    xpcie_write_reg(dev, Reg_DeviceCS, 0);
}

static void xpcie_init_card(xbmd_device *dev)
{
    xpcie_initiator_reset(dev);
    
    xpcie_write_reg(dev, Reg_WriteTlpAddress, dev->d2hAddr);
    xpcie_write_reg(dev, Reg_ReadTlpAddress, dev->h2dAddr);
}


static int xpcie_open(struct inode *inode, struct file *filep)
{
    xbmd_device *dev = container_of(inode->i_cdev, xbmd_device, chrDev);
    filep->private_data = dev;
    
    if (!dev) {
        printk(KERN_WARNING"XBMD: Open: No device associated with this file.");
        return -EBUSY;
    }
    
    if (!dev->baseVirtual || !dev->h2dBuffer || !dev->d2hBuffer) {
        printk(KERN_WARNING"XBMD: Open: No memory associated with %s.", dev->name);
        return -ENOMEM;
    }
    
    printk(KERN_INFO"%s: Open: module opened\n", dev->name);
    return 0;
}

static int xpcie_release(struct inode *inode, struct file *filep)
{
    xbmd_device *dev = filep->private_data;
    printk(KERN_INFO"%s: Release: module released\n", dev->name);
    return 0;
}

static ssize_t xpcie_write(struct file *filep, const char __user *buf, size_t count,
                           loff_t *f_pos)
{
    xbmd_device *dev = filep->private_data;
    ssize_t bytesWritten = 0;
    u32 tlpSize = 128;
    u32 tlpCount = 0;
    u32 dmaControlReg = 0;
    
    printk(KERN_INFO "%s: xpcie_write: %ld bytes should be written...\n", dev->name, count);
    
    if (count > RW_MAX_BYTES) {
        count = RW_MAX_BYTES;
    }
    if (count == 0) {
        return bytesWritten;
    }
    
    printk(KERN_INFO "%s: xpcie_write: Will only do %ld bytes...\n", dev->name, count);
    
    tlpCount = count / (sizeof(u32) * tlpSize);
    if (sizeof(u32) * tlpSize * tlpCount != count) {
        printk(KERN_ALERT "%s: Misaligned DMA write count. Only whole TLP's are supported.\n",
               dev->name);
        return -EFAULT;
    }
    
    if (copy_from_user(dev->h2dBuffer, buf, count)) {
        printk(KERN_ALERT "%s: XPCIe_write: Failed copy from user.\n", dev->name);
        return -EFAULT;
    }
    
    printk(KERN_INFO "%s: xpcie_write: Copied data from user...\n", dev->name);
    
    if (down_interruptible(&dev->dmaSem)) {
        return -ERESTARTSYS;
    }
    
    if (dev->dmaH2DStart) {
        up(&dev->dmaSem);
        printk(KERN_ALERT "DMA write starting while still busy!\n");
        return -EFAULT;
    }
    dev->dmaH2DDone = 0;
    dev->dmaH2DStart = 1;
    
    printk(KERN_INFO "%s: xpcie_write: Resetting initiator...\n", dev->name);
    
    // NOTE(michiel): Setup dma registers
    xpcie_initiator_reset(dev);
    //dmaControlReg = xpcie_read_reg(dev, Reg_DeviceDMACS);
    xpcie_write_reg(dev, Reg_ReadTlpSize, tlpSize);
    xpcie_write_reg(dev, Reg_ReadTlpCount, tlpCount);
    xpcie_write_reg(dev, Reg_DeviceMiscControl, 0);
    
    dmaControlReg |= (1 << 16); // NOTE(michiel): Enable read from memory
    dmaControlReg &= ~(1 << 23); // NOTE(michiel): Make sure interrupt is enabled
    xpcie_write_reg(dev, Reg_DeviceDMACS, dmaControlReg);
    
    printk(KERN_INFO "%s: xpcie_write: Started DMA transfer...\n", dev->name);
    
    while (!dev->dmaH2DDone) {
        up(&dev->dmaSem);
        if (wait_event_interruptible(dev->dmaH2DQueue, (dev->dmaH2DDone != 0))) {
            dev->dmaH2DStart = 0;
            return -ERESTARTSYS;
        }
        if (down_interruptible(&dev->dmaSem)) {
            dev->dmaH2DStart = 0;
            return -ERESTARTSYS;
        }
    }
    
    dev->dmaH2DStart = 0;
    dev->dmaH2DDone = 0;
    up(&dev->dmaSem);
    
    bytesWritten = count;
    printk(KERN_INFO "%s: XPCIe_Write: %ld bytes have been written...\n", dev->name,
           bytesWritten);
    
    return bytesWritten;
}

static ssize_t xpcie_read(struct file *filep, char __user *buf, size_t count, loff_t *f_pos)
{
    xbmd_device *dev = filep->private_data;
    ssize_t bytesRead = 0;
    u32 tlpSize = 128;
    u32 tlpCount = 0;
    u32 dmaControlReg = 0;
    
    printk(KERN_INFO "%s: xpcie_read: %ld bytes should be read...\n", dev->name, count);
    
    if (count > RW_MAX_BYTES) {
        count = RW_MAX_BYTES;
    }
    if (count == 0) {
        return 0;
    }
    
    printk(KERN_INFO "%s: xpcie_read: Will only do %ld bytes...\n", dev->name, count);
    
    tlpCount = count / (sizeof(u32) * tlpSize);
    if (sizeof(u32) * tlpSize * tlpCount != count) {
        printk(KERN_ALERT "%s: Misaligned DMA Read count. Only whole TLP's are supported.\n",
               dev->name);
        return -EFAULT;
    }
    
    if (down_interruptible(&dev->dmaSem)) {
        return -ERESTARTSYS;
    }
    
    if (dev->dmaD2HStart) {
        up(&dev->dmaSem);
        printk(KERN_ALERT "DMA read starting while still busy!\n");
        return -EFAULT;
    }
    dev->dmaD2HDone = 0;
    dev->dmaD2HStart = 1;
    
    printk(KERN_INFO "%s: xpcie_read: Resetting initiator...\n", dev->name);
    
    // NOTE(michiel): Setup dma registers
    xpcie_initiator_reset(dev);
    //dmaControlReg = xpcie_read_reg(dev, Reg_DeviceDMACS);
    xpcie_write_reg(dev, Reg_WriteTlpSize, tlpSize);
    xpcie_write_reg(dev, Reg_WriteTlpCount, tlpCount);
    xpcie_write_reg(dev, Reg_DeviceMiscControl, 0);
    
    dmaControlReg |= 1; // NOTE(michiel): Enable write to memory
    dmaControlReg &= ~(1 << 7); // NOTE(michiel): Make sure interrupt is enabled
    xpcie_write_reg(dev, Reg_DeviceDMACS, dmaControlReg);
    
    printk(KERN_INFO "%s: xpcie_read: Started DMA transfer...\n", dev->name);
    
    while (!dev->dmaD2HDone) {
        up(&dev->dmaSem);
        if (wait_event_interruptible(dev->dmaD2HQueue, (dev->dmaD2HDone != 0))) {
            dev->dmaD2HStart = 0;
            return -ERESTARTSYS;
        }
        if (down_interruptible(&dev->dmaSem)) {
            dev->dmaD2HStart = 0;
            return -ERESTARTSYS;
        }
    }
    
    dev->dmaD2HStart = 0;
    dev->dmaD2HDone = 0;
    up(&dev->dmaSem);
    
    if (copy_to_user(buf, dev->d2hBuffer, count)) {
        printk(KERN_ALERT "%s: XPCIe_read: Failed copy to user.\n", dev->name);
        return -EFAULT;
    }
    
    bytesRead = count;
    printk(KERN_INFO "%s: XPCIe_Read: %ld bytes have been read...\n", dev->name,
           bytesRead);
    
    return bytesRead;
}

struct file_operations xpcie_fops = {
    .owner   = THIS_MODULE,
    .read    = xpcie_read,
    .write   = xpcie_write,
    .open    = xpcie_open,
    .release = xpcie_release,
};

static void xpcie_exit(xbmd_device *dev)
{
    if (!dev) {
        return;
    }
    
    if (dev->d2hBuffer) {
        // Assume we also have a chrdev (chrdev is initialized just after writeBuffer is allocated)
        dev_t devno = MKDEV(gDrvrMajor, gDrvrMinor);
        cdev_del(&dev->chrDev);
        unregister_chrdev_region(devno, 1);
        
        pci_free_consistent(dev->pciDev, DMA_BUF_SIZE, dev->d2hBuffer, dev->d2hAddr);
        dev->d2hBuffer = 0;
        dev->d2hAddr = 0;
    }
    
    if (dev->h2dBuffer) {
        pci_free_consistent(dev->pciDev, DMA_BUF_SIZE, dev->h2dBuffer, dev->h2dAddr);
        dev->h2dBuffer = 0;
        dev->h2dAddr = 0;
    }
    
    if (dev->irq >= 0) {
        free_irq(dev->irq, dev);
        dev->irq = -1;
        pci_free_irq_vectors(dev->pciDev);
    }
    
    if (dev->baseVirtual) {
        pci_iounmap(dev->pciDev, dev->baseVirtual);
        dev->baseVirtual = 0;
    }
    
    pci_release_regions(dev->pciDev);
    pci_clear_master(dev->pciDev);
    pci_disable_device(dev->pciDev);
    pci_set_drvdata(dev->pciDev, 0);
    
    dev->baseAddr = 0;
    kfree(dev);
    
    // Update Kernel log stating driver is unloaded
    printk(KERN_ALERT"%s driver is unloaded\n", dev->name);
}

static int xpcie_probe(struct pci_dev *pci, const struct pci_device_id *pci_id)
{
    u8 version;
    dev_t devno;
    
    // TODO(michiel): Ref for now: https://elixir.bootlin.com/linux/v4.9.150/source/drivers/gpu/drm/bridge/dw-hdmi-ahb-audio.c#L582
    xbmd_device *dev = kzalloc(sizeof(xbmd_device), GFP_KERNEL);
    dev->name = "Grimmerst";
    dev->pciDev = pci;
    dev->irq = -1;
    
    init_waitqueue_head(&dev->dmaH2DQueue);
    init_waitqueue_head(&dev->dmaD2HQueue);
    sema_init(&dev->dmaSem, 1);
    
    pci_set_drvdata(pci, dev);
    
    // Bus Master Enable
    if (0 > pci_enable_device(pci)) {
        printk(KERN_WARNING"%s: Init: Device not enabled.\n", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    pci_set_master(pci);
    
    // Check the memory region to see if it is in use
    // Try to gain exclusive control of memory for demo hardware.
    if (0 > pci_request_regions(pci, "GrimmDMABak")) {
        printk(KERN_WARNING"%s: Init: Memory in use.\n", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    
    // Get Base Address of registers from pci structure. Should come from pci_dev
    // structure, but that element seems to be missing on the development system.
    dev->baseAddr = pci_resource_start(pci, 0);
    
    if (dev->baseAddr < 0) {
        printk(KERN_WARNING"%s: Init: Base Address not set.\n", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    
    // Print Base Address to kernel log
    printk(KERN_INFO"%s: Init: Base hw val %X\n", dev->name, (unsigned int)dev->baseAddr);
    
    // Get the Base Address Length
    dev->baseLength = pci_resource_len(pci, 0);
    
    // Print the Base Address Length to Kernel Log
    printk(KERN_INFO"%s: Init: Base hw len %d\n", dev->name, (unsigned int)dev->baseLength);
    
    // Remap the I/O register block so that it can be safely accessed.
    // I/O register block starts at gBaseHdwr and is 32 bytes long.
    dev->baseVirtual = pci_iomap(pci, 0, dev->baseLength);
    if (!dev->baseVirtual) {
        printk(KERN_WARNING"%s: Init: Could not remap memory.\n", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    
    // Print out the aquired virtual base addresss
    printk(KERN_INFO"%s: Init: Virt HW address %lX\n", dev->name,
           (unsigned long)dev->baseVirtual);
    
    //---START: Initialize Hardware
    
    printk(KERN_INFO"%s: Init: Initialize Hardware Done..\n",dev->name);
    
    // Request IRQ from OS.
    printk(KERN_INFO"%s: Init: Device IRQ: %d\n",dev->name, pci->irq);
    if (pci_alloc_irq_vectors(dev->pciDev, 8, 8, PCI_IRQ_MSI) != 8) {
        printk(KERN_WARNING"%s: Init: Unable to allocate all MSI IRQs", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    
    dev->irq = pci_irq_vector(pci, 0);
    if (0 > dev->irq) {
        printk(KERN_WARNING"%s: Init: Unable to find IRQ vector", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    if (0 > request_irq(dev->irq, xpcie_irq_msi_handler, 0, dev->name, dev)) {
        printk(KERN_WARNING"%s: Init: Unable to allocate IRQ", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    
    //--- END: Initialize Hardware
    
    //--- START: Allocate Buffers
    
    // Allocate the read buffer with size DMA_BUF_SIZE and return the starting address
    dev->h2dBuffer = pci_alloc_consistent(pci, DMA_BUF_SIZE, &dev->h2dAddr);
    if (!dev->h2dBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate read buffer.\n",dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    // Print Read buffer size and address to kernel log
    printk(KERN_INFO"%s: Read Buffer Allocation: %lX->%X\n", dev->name,
           (unsigned long)dev->h2dBuffer, (unsigned int)dev->h2dAddr);
    
    // Allocate the write buffer with size DMA_BUF_SIZE and return the starting address
    dev->d2hBuffer = pci_alloc_consistent(pci, DMA_BUF_SIZE, &dev->d2hAddr);
    if (!dev->d2hBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate gBuffer.\n",dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    // Print Write buffer size and address to kernel log
    printk(KERN_INFO"%s: Write Buffer Allocation: %lX->%X\n", dev->name,
           (unsigned long)dev->d2hBuffer, (unsigned int)dev->d2hAddr);
    
    //--- END: Allocate Buffers
    
    //--- START: Register Driver
    
    // Register with the kernel as a character device.
    if (gDrvrMajor) {
        dev_t devno = MKDEV(gDrvrMajor, gDrvrMinor);
        if (0 > register_chrdev_region(devno, 1, dev->name)) {
            printk(KERN_WARNING"%s: Init: will not register\n", dev->name);
            xpcie_exit(dev);
            return -EFAULT;
        }
    } else {
        dev_t devno = 0;
        if (0 > alloc_chrdev_region(&devno, gDrvrMinor, 1, dev->name)) {
            printk(KERN_WARNING"%s: Init: will not register\n", dev->name);
            xpcie_exit(dev);
            return -EFAULT;
        }
        gDrvrMajor = MAJOR(devno);
    }
    
    devno = MKDEV(gDrvrMajor, gDrvrMinor);
    cdev_init(&dev->chrDev, &xpcie_fops);
    dev->chrDev.owner = THIS_MODULE;
    if (0 > cdev_add(&dev->chrDev, devno, 1)) {
        printk(KERN_WARNING"%s: Init: will not register\n", dev->name);
        xpcie_exit(dev);
        return -EFAULT;
    }
    
    printk(KERN_INFO"%s: Init: module registered\n", dev->name);
    
    //--- END: Register Driver
    
    // The driver is now successfully loaded.  All HW is initialized, IRQ's assigned, and buffers allocated
    printk("%s driver is loaded\n", dev->name);
    
    pci_read_config_byte(pci, 8, &version);
    printk(KERN_INFO "%s: Version: %d\n", dev->name, version);
    
    // Initializing card registers
    xpcie_init_card(dev);
    
    return 0;
}

void xpcie_remove(struct pci_dev *pci)
{
    xpcie_exit(pci_get_drvdata(pci));
}

static const struct pci_device_id xpcie_ids[] =
{
    {
        PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_XILINX_PCIE,
        PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0
    },
    { 0 }
};

static struct pci_driver xpcie_drive =
{
    .name = KBUILD_MODNAME,
    .id_table = xpcie_ids,
    .probe = xpcie_probe,
    .remove = xpcie_remove,
};

module_pci_driver(xpcie_drive);
