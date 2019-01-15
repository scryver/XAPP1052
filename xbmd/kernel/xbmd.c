// (c) Copyright 2009  2009 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.


//--------------------------------------------------------------------------------
//-- Filename: xbmd.c
//--
//-- Description: XBMD device driver.
//--
//-- XBMD is an example Red Hat device driver which exercises XBMD design
//-- Device driver has been tested on Red Hat Fedora FC9 2.6.15.
//--------------------------------------------------------------------------------

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
//#include <linux/pci-aspm.h>
//#include <linux/pci_regs.h>

#include <linux/uaccess.h>   /* copy_to_user */

#include "xbmd.h"

// semaphores
enum  {
        SEM_READ,
        SEM_WRITE,
        SEM_WRITEREG,
        SEM_READREG,
        SEM_WAITFOR,
        SEM_DMA,
        NUM_SEMS
      };

//semaphores
struct semaphore gSem[NUM_SEMS];

MODULE_LICENSE("Dual BSD/GPL");

// Defines the Vendor ID.  Must be changed if core generated did not set the Vendor ID to the same value
#define PCI_VENDOR_ID_XILINX      0x10ee

// Defines the Device ID.  Must be changed if core generated did not set the Device ID to the same value
#define PCI_DEVICE_ID_XILINX_PCIE 0x7011

// Defining
#define XBMD_REGISTER_SIZE        (4*8)    // There are eight registers, and each is 4 bytes wide.
#define HAVE_REGION               0x01     // I/O Memory region
#define HAVE_IRQ                  0x02     // Interupt

//Status Flags:
//       1 = Resouce successfully acquired
//       0 = Resource not acquired.

#define HAVE_REGION 0x01                    // I/O Memory region
#define HAVE_IRQ    0x02                    // Interupt
#define HAVE_KREG   0x04                    // Kernel registration

typedef struct xbmd_device
{
    const char *name;
    
unsigned long baseAddr;
    unsigned long baseLength;
    void __iomem *baseVirtual;
    
    struct pci_dev *pciDev;
    int             irq;
    
    dma_addr_t    readAddr;
    u8           *readBuffer;
    dma_addr_t    writeAddr;
    u8           *writeBuffer;
    
    struct cdev   chrDev;
    } xbmd_device;

int             gDrvrMajor = 241;           // Major number not dynamic.
int             gDrvrMinor = 0;

typedef union RegWrite {
    u64 raw;
    struct {
        int reg;
        int value;
    };
} RegWrite;

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------

irqreturn_t XPCIe_IRQHandler(int irq, void *dev_id);
irqreturn_t XPCIe_IRQMSIHandler(int irq, void *dev_id);
u32   XPCIe_ReadReg (xbmd_device *dev, u32 dw_offset);
void  XPCIe_WriteReg (xbmd_device *dev, u32 dw_offset, u32 val);
void  XPCIe_InitCard (xbmd_device *dev);
void  XPCIe_InitiatorReset (xbmd_device *dev);
u32 XPCIe_ReadCfgReg (xbmd_device *dev, u32 byte);
u32 XPCIe_WriteCfgReg (xbmd_device *dev, u32 byte, u32 value);

//---------------------------------------------------------------------------
// Name:        XPCIe_Open
//
// Description: Book keeping routine invoked each time the device is opened.
//
// Arguments: inode :
//            filp  :
//
// Returns: 0 on success, error code on failure.
//
// Modification log:
// Date      Who  Description
//
//---------------------------------------------------------------------------
int XPCIe_Open(struct inode *inode, struct file *filp)
{
    xbmd_device *dev = container_of(inode->i_cdev, xbmd_device, chrDev);
    filp->private_data = dev;
    
    if (!dev) {
        printk(KERN_WARNING"XBMD: Open: No device associated with this file.");
        return -EBUSY;
    }
    
    if (!dev->baseVirtual || !dev->readBuffer || !dev->writeBuffer) {
        return -ENOMEM;
    }
    
    // TODO(michiel): Check for 0 dev
    printk(KERN_INFO"%s: Open: module opened\n", dev->name);
  return SUCCESS;
}

//---------------------------------------------------------------------------
// Name:        XPCIe_Release
//
// Description: Book keeping routine invoked each time the device is closed.
//
// Arguments: inode :
//            filp  :
//
// Returns: 0 on success, error code on failure.
//
// Modification log:
// Date      Who  Description
//
//---------------------------------------------------------------------------
int XPCIe_Release(struct inode *inode, struct file *filp)
{
    xbmd_device *dev = filp->private_data;
  printk(KERN_INFO"%s: Release: module released\n", dev->name);
  return(SUCCESS);
}

//---------------------------------------------------------------------------
// Name:        XPCIe_Write
//
// Description: This routine is invoked from user space to write data to
//              the PCIe device.
//
// Arguments: filp  : file pointer to opened device.
//            buf   : pointer to location in users space, where data is to
//                    be acquired.
//            count : Amount of data in bytes user wishes to send.
//
// Returns: SUCCESS  = Success
//          CRIT_ERR = Critical failure
//
// Modification log:
// Date      Who  Description
//
//---------------------------------------------------------------------------
ssize_t XPCIe_Write(struct file *filp, const char __user *buf, size_t count,
                       loff_t *f_pos)
{
    xbmd_device *dev = filp->private_data;
    int ret = SUCCESS;
    if (copy_from_user(dev->writeBuffer, buf, count)) {
        printk("%s: XPCIe_Write: Failed copy from user.\n", dev->name);
        return -EFAULT;
    }
    printk(KERN_INFO"%s: XPCIe_Write: %ld bytes have been written...\n", dev->name, count);
    if (copy_from_user(dev->readBuffer, buf, count)) {
        printk("%s: XPCIe_Write: Failed copy from user.\n", dev->name);
        return -EFAULT;
    }
    printk(KERN_INFO"%s: XPCIe_Write: %ld bytes have been read...\n", dev->name, count);
    return (ret);
}

//---------------------------------------------------------------------------
// Name:        XPCIe_Read
//
// Description: This routine is invoked from user space to read data from
//              the PCIe device. ***NOTE: This routine returns the entire
//              buffer, (DMA_BUF_SIZE), count is ignored!. The user App must
//              do any needed processing on the buffer.
//
// Arguments: filp  : file pointer to opened device.
//            buf   : pointer to location in users space, where data is to
//                    be placed.
//            count : Amount of data in bytes user wishes to read.
//
// Returns: SUCCESS  = Success
//          CRIT_ERR = Critical failure
//
//  Modification log:
//  Date      Who  Description
//----------------------------------------------------------------------------

ssize_t XPCIe_Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    xbmd_device *dev = filp->private_data;
    if (copy_to_user(buf, dev->writeBuffer, count)) {
        printk("%s: XPCIe_Read: Failed copy to user.\n", dev->name);
        return -EFAULT;
    }
    printk(KERN_INFO"%s: XPCIe_Read: %ld bytes have been read...\n", dev->name, count);
    return (0);
}

//---------------------------------------------------------------------------
// Name:        XPCIe_Ioctl
//
// Description: This routine is invoked from user space to configure the
//              running driver.
//
// Arguments: filp  : File pointer to opened device.
//            cmd   : Ioctl command to execute.
//            arg   : Argument to Ioctl command.
//
// Returns: 0 on success, error code on failure.
//
// Modification log:
// Date      Who  Description
//
//---------------------------------------------------------------------------
#define IOCTL_READ_REG(x)     { regx = XPCIe_ReadReg(dev, x); ret = put_user(regx, (u32 *)arg); }
#define IOCTL_WRITE_REG(x)    { XPCIe_WriteReg(dev, x, arg); }
long XPCIe_Ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    xbmd_device *dev = filp->private_data;
  u32 regx;
  int ret = SUCCESS;

  switch (cmd) {
        // Initailizes XBMD application
        case XBMD_IOC_INITCARD:       { XPCIe_InitCard(dev); } break;
        // Resets XBMD applications
        case XBMD_IOC_RESET:          { XPCIe_InitiatorReset(dev); } break;
        case XBMD_IOC_DISP_REGS:      {} break;
        // Read: Device Control Status Register
        case XBMD_IOC_READ_CTRL:      IOCTL_READ_REG(Reg_DeviceCS) break;
        // Read: DMA Control Status Register
        case XBMD_IOC_READ_DMA_CTRL:  IOCTL_READ_REG(Reg_DeviceDMACS) break;
        // Read: Write DMA TLP Address Register
        case XBMD_IOC_READ_WR_ADDR:   IOCTL_READ_REG(Reg_WriteTlpAddress) break;
        // Read: Write DMA TLP Size Register
        case XBMD_IOC_READ_WR_LEN:    IOCTL_READ_REG(Reg_WriteTlpSize) break;
        // Read: Write DMA TLP Count Register
        case XBMD_IOC_READ_WR_COUNT:  IOCTL_READ_REG(Reg_WriteTlpCount) break;
        // Read: Write DMA TLP Pattern Register
        case XBMD_IOC_READ_WR_PTRN:   IOCTL_READ_REG(Reg_WriteTlpPattern) break;
        // Read: Read DMA TLP Pattern Register
        case XBMD_IOC_READ_RD_PTRN:   IOCTL_READ_REG(Reg_ReadTlpPattern) break;
        // Read: Read DMA TLP Address Register
        case XBMD_IOC_READ_RD_ADDR:   IOCTL_READ_REG(Reg_ReadTlpAddress) break;
        // Read: Read DMA TLP Size Register
        case XBMD_IOC_READ_RD_LEN:    IOCTL_READ_REG(Reg_ReadTlpSize) break;
        // Read: Read DMA TLP Count Register
        case XBMD_IOC_READ_RD_COUNT:  IOCTL_READ_REG(Reg_ReadTlpCount) break;
        // Read: Write DMA Performance Register
        case XBMD_IOC_READ_WR_PERF:   IOCTL_READ_REG(Reg_WriteDMAPerf) break;
        // Read: Read DMA Performance Register
        case XBMD_IOC_READ_RD_PERF:   IOCTL_READ_REG(Reg_ReadDMAPerf) break;
        // Read: Read DMA Status Register
        case XBMD_IOC_READ_CMPL:      IOCTL_READ_REG(Reg_ReadComplStatus) break;
        // Read: Number of Read Completion w/ Data Register
        case XBMD_IOC_READ_CWDATA:    IOCTL_READ_REG(Reg_ComplWithData) break;
        // Read: Read Completion Size Register
        case XBMD_IOC_READ_CSIZE:     IOCTL_READ_REG(Reg_ComplSize) break;
        // Read: Device Link Width Status Register
        case XBMD_IOC_READ_LINKWDTH:  IOCTL_READ_REG(Reg_DeviceLinkWidth) break;
        // Read: Device Link Transaction Size Status Register
        case XBMD_IOC_READ_LINKLEN:   IOCTL_READ_REG(Reg_DeviceLinkTlpSize) break;
        // Read: Device Miscellaneous Control Register
        case XBMD_IOC_READ_MISC_CTL:  IOCTL_READ_REG(Reg_DeviceMiscControl) break;
        // Read: Device MSI Control
        case XBMD_IOC_READ_INTRPT:    IOCTL_READ_REG(Reg_DeviceMSIControl) break;
        // Read: Device Directed Link Change Register
        case XBMD_IOC_READ_DIR_LINK:  IOCTL_READ_REG(Reg_DeviceDirectedLinkChange) break;
        // Read: Device FC Control Register
        case XBMD_IOC_READ_FC_CTRL:   IOCTL_READ_REG(Reg_DeviceFCControl) break;
        // Read: Device FC Posted Information
        case XBMD_IOC_READ_FC_POST:   IOCTL_READ_REG(Reg_DeviceFCPostedInfo) break;
        // Read: Device FC Non Posted Information
        case XBMD_IOC_READ_FC_NPOST:  IOCTL_READ_REG(Reg_DeviceFCNonPostedInfo) break;
        // Read: Device FC Completion Information
        case XBMD_IOC_READ_FC_CMPL:   IOCTL_READ_REG(Reg_DeviceFCCompletionInfo) break;
        
        // Write: DMA Control Status Register
        case XBMD_IOC_WRITE_DMA_CTRL: IOCTL_WRITE_REG(Reg_DeviceDMACS) break;
        // Write: Write DMA TLP Size Register
        case XBMD_IOC_WRITE_WR_LEN:   IOCTL_WRITE_REG(Reg_WriteTlpSize) break;
        // Write: Write DMA TLP Count Register
        case XBMD_IOC_WRITE_WR_COUNT: IOCTL_WRITE_REG(Reg_WriteTlpCount) break;
        // Write: Write DMA TLP Pattern Register
        case XBMD_IOC_WRITE_WR_PTRN:  IOCTL_WRITE_REG(Reg_WriteTlpPattern) break;
        // Write: Read DMA TLP Size Register
        case XBMD_IOC_WRITE_RD_LEN:   IOCTL_WRITE_REG(Reg_ReadTlpSize) break;
        // Write: Read DMA TLP Count Register
        case XBMD_IOC_WRITE_RD_COUNT: IOCTL_WRITE_REG(Reg_ReadTlpCount) break;
        // Write: Read DMA TLP Pattern Register
        case XBMD_IOC_WRITE_RD_PTRN:  IOCTL_WRITE_REG(Reg_ReadTlpPattern) break;
        // Write: Device Miscellaneous Control Register
        case XBMD_IOC_WRITE_MISC_CTL: IOCTL_WRITE_REG(Reg_DeviceMiscControl) break;
        // Write: Device Directed Link Change Register
        case XBMD_IOC_WRITE_DIR_LINK: IOCTL_WRITE_REG(Reg_DeviceDirectedLinkChange) break;
        
        // Read: Any XBMD Reg.  Added generic functionality so all register can be read
    case XBMD_IOC_RD_BMD_REG: {
        if (get_user(regx, (u32 *)arg) < 0) {
            ret = -EFAULT;
        } else {
        regx = XPCIe_ReadReg(dev, regx);
        ret = put_user(regx, (u32 *)arg);
            }
        } break;
        // Read: Any CFG Reg.  Added generic functionality so all register can be read
    case XBMD_IOC_RD_CFG_REG: {
        if (get_user(regx, (u32 *)arg) < 0) {
             ret = -EFAULT;
        } else {
                regx = XPCIe_ReadCfgReg(dev, regx);
        ret = put_user(regx, (u32 *)arg);
            }
        } break;
        // Write: Any BMD Reg.  Added generic functionality so all register can be read
    case XBMD_IOC_WR_BMD_REG: {
            RegWrite bmdArg = {0};
            if (get_user(bmdArg.raw, (u64 *)arg) < 0) {
                ret = -EFAULT;
            } else {
                XPCIe_WriteReg(dev, bmdArg.reg, bmdArg.value);
            printk(KERN_WARNING"%d: Write Register.\n", bmdArg.reg);
            printk(KERN_WARNING"%d: Write Value.\n", bmdArg.value);
            }
        } break;
        // Write: Any CFG Reg.  Added generic functionality so all register can be read
    case XBMD_IOC_WR_CFG_REG: {
            RegWrite cfgArg = {0};
            if (get_user(cfgArg.raw, (u64 *)arg) < 0) {
                ret = -EFAULT;
            } else {
                XPCIe_WriteCfgReg(dev, cfgArg.reg, cfgArg.value);
            printk(KERN_WARNING"%d: Write Register.\n", cfgArg.reg);
            printk(KERN_WARNING"%d: Write Value.\n", cfgArg.value);
            }
        } break;
        
        default: {} break;
  }

  return ret;
}
#undef IOCTL_WRITE_REG
#undef IOCTL_READ_REG

// Aliasing write, read, ioctl, etc...
struct file_operations XPCIe_Intf = {
    owner:      THIS_MODULE,
    read:       XPCIe_Read,
    write:      XPCIe_Write,
    unlocked_ioctl:      XPCIe_Ioctl,
    open:       XPCIe_Open,
    release:    XPCIe_Release,
};

//--- XPCIe_exit(): Performs any cleanup required before releasing the device
//--- Arguments: None
//--- Return Value: None
//--- Detailed Description: Performs all cleanup functions required before releasing device
static void
XPCIe_Exit(xbmd_device *dev)
{
    if (!dev) {
        return;
    }
    
    if (dev->writeBuffer) {
        // Assume we also have a chrdev (chrdev is initialized just after writeBuffer is allocated)
        dev_t devno = MKDEV(gDrvrMajor, gDrvrMinor);
        cdev_del(&dev->chrDev);
        unregister_chrdev_region(devno, 1);
        
        pci_free_consistent(dev->pciDev, DMA_BUF_SIZE, dev->writeBuffer, dev->writeAddr);
        dev->writeBuffer = 0;
        dev->writeAddr = 0;
    }
    
    if (dev->readBuffer) {
        pci_free_consistent(dev->pciDev, DMA_BUF_SIZE, dev->readBuffer, dev->readAddr);
        dev->readBuffer = 0;
        dev->readAddr = 0;
    }
    
    if (dev->irq >= 0) {
        free_irq(dev->irq, dev);
        #if 1
        pci_free_irq_vectors(dev->pciDev);
        #endif
        dev->irq = -1;
    }
    
    if (dev->baseVirtual) {
        pci_iounmap(dev->pciDev, dev->baseVirtual);
        dev->baseVirtual = 0;
    }
    
    pci_release_regions(dev->pciDev);
    pci_clear_master(dev->pciDev);
    pci_disable_device(dev->pciDev);
    
    dev->baseAddr = 0;
    kfree(dev);
    
    // Update Kernel log stating driver is unloaded
    printk(KERN_ALERT"%s driver is unloaded\n", dev->name);
}

static int
XPCIe_Probe(struct pci_dev *pci, const struct pci_device_id *pci_id)
{
    u8 version;
    dev_t devno;
  // Find the Xilinx EP device.  The device is found by matching device and vendor ID's which is defined
  // at the top of this file.  Be default, the driver will look for 10EE & 0007.  If the core is generated
  // with other settings, the defines at the top must be changed or the driver will not load
// Update: The pci kernel module does the lookup now and calls this probe function if it
    // matched the ids defined at the top of the file.
    
    // TODO(michiel): Ref for now: https://elixir.bootlin.com/linux/v4.9.150/source/drivers/gpu/drm/bridge/dw-hdmi-ahb-audio.c#L582
    xbmd_device *dev = kzalloc(sizeof(xbmd_device), GFP_KERNEL);
    dev->name = "xbmd";
    dev->pciDev = pci;
    dev->irq = -1;
    
  // Get Base Address of registers from pci structure. Should come from pci_dev
  // structure, but that element seems to be missing on the development system.
  dev->baseAddr = pci_resource_start(pci, 0);

    if (dev->baseAddr < 0) {
    printk(KERN_WARNING"%s: Init: Base Address not set.\n", dev->name);
        XPCIe_Exit(dev);
    return (CRIT_ERR);
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
        XPCIe_Exit(dev);
    return (CRIT_ERR);
  }

  // Print out the aquired virtual base addresss
  printk(KERN_INFO"%s: Init: Virt HW address %lX\n", dev->name,
           (unsigned long)dev->baseVirtual);

  // Get IRQ from pci_dev structure. It may have been remapped by the kernel,
  // and this value will be the correct one.
   dev->irq = pci->irq;
  printk(KERN_INFO"%s: Init: Device IRQ: %d\n",dev->name, dev->irq);

    
  //---START: Initialize Hardware

  // Check the memory region to see if it is in use
  // Try to gain exclusive control of memory for demo hardware.
    if (0 > pci_request_regions(pci, "3GIO_Demo_Drv")) {
        printk(KERN_WARNING"%s: Init: Memory in use.\n", dev->name);
        XPCIe_Exit(dev);
        return (CRIT_ERR);
    }

  printk(KERN_INFO"%s: Init: Initialize Hardware Done..\n",dev->name);
    
    // Bus Master Enable
    if (0 > pci_enable_device(pci)) {
        printk(KERN_WARNING"%s: Init: Device not enabled.\n", dev->name);
        XPCIe_Exit(dev);
        return (CRIT_ERR);
    }
    pci_set_master(pci);
    
  // Request IRQ from OS.
  // In past architectures, the SHARED and SAMPLE_RANDOM flags were called: SA_SHIRQ and SA_SAMPLE_RANDOM
  // respectively.  In older Fedora core installations, the request arguments may need to be reverted back.
  // SA_SHIRQ | SA_SAMPLE_RANDOM
    printk(KERN_INFO"%s: ISR Setup..\n", dev->name);
  #if 0
  if (0 > request_irq(dev->irq, XPCIe_IRQHandler, IRQF_SHARED, dev->name, dev)) {
    printk(KERN_WARNING"%s: Init: Unable to allocate IRQ",dev->name);
        XPCIe_Exit(dev);
    return (CRIT_ERR);
  }
    #else
    if (pci_alloc_irq_vectors(dev->pciDev, 8, 8, PCI_IRQ_MSI) != 8) {
        printk(KERN_WARNING"%s: Init: Unable to allocate all MSI IRQs", dev->name);
        XPCIe_Exit(dev);
        return CRIT_ERR;
    }
    
     dev->irq = pci_irq_vector(pci, 0);
    if (0 > dev->irq) {
        printk(KERN_WARNING"%s: Init: Unable to find IRQ vector", dev->name);
        XPCIe_Exit(dev);
        return CRIT_ERR;
    }
    if (0 > request_irq(dev->irq, XPCIe_IRQMSIHandler, 0, dev->name, dev)) {
        printk(KERN_WARNING"%s: Init: Unable to allocate IRQ", dev->name);
        XPCIe_Exit(dev);
        return (CRIT_ERR);
    }
    #endif

  //--- END: Initialize Hardware

  //--- START: Allocate Buffers

  // Allocate the read buffer with size DMA_BUF_SIZE and return the starting address
    dev->readBuffer = pci_alloc_consistent(pci, DMA_BUF_SIZE, &dev->readAddr);
  if (!dev->readBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate read buffer.\n",dev->name);
        XPCIe_Exit(dev);
    return (CRIT_ERR);
  }
  // Print Read buffer size and address to kernel log
  printk(KERN_INFO"%s: Read Buffer Allocation: %lX->%X\n", dev->name,
           (unsigned long)dev->readBuffer, (unsigned int)dev->readAddr);

  // Allocate the write buffer with size DMA_BUF_SIZE and return the starting address
    dev->writeBuffer = pci_alloc_consistent(pci, DMA_BUF_SIZE, &dev->writeAddr);
  if (!dev->writeBuffer) {
        printk(KERN_CRIT"%s: Init: Unable to allocate gBuffer.\n",dev->name);
        XPCIe_Exit(dev);
    return (CRIT_ERR);
  }
  // Print Write buffer size and address to kernel log
  printk(KERN_INFO"%s: Write Buffer Allocation: %lX->%X\n", dev->name,
           (unsigned long)dev->writeBuffer, (unsigned int)dev->writeAddr);

  //--- END: Allocate Buffers

  //--- START: Register Driver

  // Register with the kernel as a character device.
    if (gDrvrMajor) {
        dev_t devno = MKDEV(gDrvrMajor, gDrvrMinor);
        if (0 > register_chrdev_region(devno, 1, dev->name)) {
            printk(KERN_WARNING"%s: Init: will not register\n", dev->name);
            XPCIe_Exit(dev);
            return (CRIT_ERR);
        }
    } else {
        dev_t devno = 0;
        if (0 > alloc_chrdev_region(&devno, gDrvrMinor, 1, dev->name)) {
            printk(KERN_WARNING"%s: Init: will not register\n", dev->name);
            XPCIe_Exit(dev);
            return (CRIT_ERR);
        }
        gDrvrMajor = MAJOR(devno);
    }
    
    devno = MKDEV(gDrvrMajor, gDrvrMinor);
    cdev_init(&dev->chrDev, &XPCIe_Intf);
    dev->chrDev.owner = THIS_MODULE;
    if (0 > cdev_add(&dev->chrDev, devno, 1)) {
        printk(KERN_WARNING"%s: Init: will not register\n", dev->name);
        XPCIe_Exit(dev);
        return (CRIT_ERR);
    }
    
  printk(KERN_INFO"%s: Init: module registered\n", dev->name);

  //--- END: Register Driver

  // The driver is now successfully loaded.  All HW is initialized, IRQ's assigned, and buffers allocated
  printk("%s driver is loaded\n", dev->name);

    pci_read_config_byte(pci, 8, &version);
    printk(KERN_INFO "%s: Version: %d\n", dev->name, version);

  // Initializing card registers
  XPCIe_InitCard(dev);
    
    pci_set_drvdata(pci, dev);
    
  return 0;
}

//--- XPCIe_InitiatorReset(): Resets the XBMD reference design
//--- Arguments: None
//--- Return Value: None
//--- Detailed Description: Writes a 1 to the DCSR register which resets the XBMD design
void XPCIe_InitiatorReset(xbmd_device *dev)
{
  XPCIe_WriteReg(dev, Reg_DeviceCS, 1);
    // Write: DCSR (offset 0) with value of 1 (Reset Device)
    XPCIe_WriteReg(dev, Reg_DeviceCS, 0);
    // Write: DCSR (offset 0) with value of 0 (Make Active)
}


//--- XPCIe_InitCard(): Initializes XBMD descriptor registers to default values
//--- Arguments: None
//--- Return Value: None
//--- Detailed Description: 1) Resets device
//---                       2) Writes specific values into the XBMD registers inside the EP
void XPCIe_InitCard(xbmd_device *dev)
{
    XPCIe_InitiatorReset(dev);

    XPCIe_WriteReg(dev, Reg_WriteTlpAddress, dev->writeAddr);        // Write: Write DMA TLP Address register with starting address
    XPCIe_WriteReg(dev, Reg_WriteTlpSize, 0x20);                // Write: Write DMA TLP Size register with default value (32dwords)
    XPCIe_WriteReg(dev, Reg_WriteTlpCount, 0x2000);              // Write: Write DMA TLP Count register with default value (2000)
    XPCIe_WriteReg(dev, Reg_WriteTlpPattern, 0x00000000);          // Write: Write DMA TLP Pattern register with default value (0x0)

    XPCIe_WriteReg(dev, Reg_ReadTlpPattern, 0xfeedbeef);          // Write: Read DMA Expected Data Pattern with default value (feedbeef)
    XPCIe_WriteReg(dev, Reg_ReadTlpAddress, dev->readAddr);         // Write: Read DMA TLP Address register with starting address.
    XPCIe_WriteReg(dev, Reg_ReadTlpSize, 0x20);                // Write: Read DMA TLP Size register with default value (32dwords)
    XPCIe_WriteReg(dev, Reg_ReadTlpCount, 0x2000);              // Write: Read DMA TLP Count register with default value (2000)
}

void XPCIe_Remove(struct pci_dev *pci)
{
    XPCIe_Exit(pci_get_drvdata(pci));
}

static const struct pci_device_id XPCIe_ids[] =
{
    {
        PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_XILINX_PCIE,
        PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0
    },
    { 0 }
};

static struct pci_driver XPCIe_drive =
{
    .name = KBUILD_MODNAME,
    .id_table = XPCIe_ids,
    .probe = XPCIe_Probe,
    .remove = XPCIe_Remove,
};

module_pci_driver(XPCIe_drive);

// // Driver Entry Point
// module_init(XPCIe_init);

// // Driver Exit Point
// module_exit(XPCIe_exit);


irqreturn_t XPCIe_IRQHandler(int irq, void *dev_id)
{
    u32 i, regx, msiReg;
    xbmd_device *dev = dev_id;
    
     msiReg = XPCIe_ReadReg(dev, Reg_DeviceMSIControl);
    if (msiReg & 0x80000000) {
        // NOTE(michiel): Our interrupt
    printk(KERN_WARNING"%s: Interrupt Handler Start ..",dev->name);
    
    for (i = 0; i < 32; i++) {
        regx = XPCIe_ReadReg(dev, i);
        printk(KERN_WARNING"%s: REG<%d> : 0x%X\n", dev->name, i, regx);
    }
        
        XPCIe_WriteReg(dev, Reg_DeviceMSIControl, 0x100 | (msiReg & 0xFF));
        XPCIe_ReadReg(dev, Reg_DeviceMSIControl);
        XPCIe_WriteReg(dev, Reg_DeviceMSIControl, (msiReg & 0xFF));
    
    printk(KERN_WARNING"%s: Interrupt Handler End ..\n", dev->name);
    
        return IRQ_HANDLED;
    } else {
        return IRQ_NONE;
    }
}

irqreturn_t XPCIe_IRQMSIHandler(int irq, void *dev_id)
{
    u32 i, regx;
    xbmd_device *dev = dev_id;

    printk(KERN_WARNING"%s: Interrupt Handler Start ..",dev->name);

    for (i = 0; i < 32; i++) {
        regx = XPCIe_ReadReg(dev, i);
        printk(KERN_WARNING"%s : REG<%d> : 0x%X\n", dev->name, i, regx);
    }

    printk(KERN_WARNING"%s: Interrupt Handler End ..\n", dev->name);

    return IRQ_HANDLED;
}

u32 XPCIe_ReadReg(xbmd_device *dev, u32 dw_offset)
{
        u32 ret = 0;
        ret = readl(dev->baseVirtual + (4 * dw_offset));

        return ret;
}

void XPCIe_WriteReg(xbmd_device *dev, u32 dw_offset, u32 val)
{
        writel(val, (dev->baseVirtual + (4 * dw_offset)));
}

#if 0
int XPCIe_ReadMem(char *buf, size_t count)
{
    int ret = 0;
    dma_addr_t dma_addr;

    //make sure passed in buffer is large enough
    if ( count < DMA_BUF_SIZE ) {
      printk("%s: XPCIe_Read: passed in buffer too small.\n", dev->name);
      ret = -1;
      goto exit;
    }

    down(&gSem[SEM_DMA]);

    // pci_map_single return the physical address corresponding to
    // the virtual address passed to it as the 2nd parameter

    dma_addr = pci_map_single(gDev, gReadBuffer, DMA_BUF_SIZE, PCI_DMA_FROMDEVICE);
    if ( 0 == dma_addr )  {
        printk("%s: XPCIe_Read: Map error.\n",dev->name);
        ret = -1;
        goto exit;
    }

    // Now pass the physical address to the device hardware. This is now
    // the destination physical address for the DMA and hence the to be
    // put on Memory Transactions

    // Do DMA transfer here....

    printk("%s: XPCIe_Read: ReadBuf Virt Addr = %lX Phy Addr = %X.\n",
            dev->name, (unsigned long)gReadBuffer, (unsigned int)dma_addr);

    // Unmap the DMA buffer so it is safe for normal access again.
    pci_unmap_single(gDev, dma_addr, DMA_BUF_SIZE, PCI_DMA_FROMDEVICE);

    up(&gSem[SEM_DMA]);

    // Now it is safe to copy the data to user space.
    if ( copy_to_user(buf, gReadBuffer, DMA_BUF_SIZE) )  {
        ret = -1;
        printk("%s: XPCIe_Read: Failed copy to user.\n",dev->name);
        goto exit;
    }
    exit:
      return ret;
}

ssize_t XPCIe_WriteMem(const char *buf, size_t count) {
    int ret = 0;
    dma_addr_t dma_addr;

    if ( (count % 4) != 0 )  {
       printk("%s: XPCIe_Write: Buffer length not dword aligned.\n",dev->name);
       ret = -1;
       goto exit;
    }

    // Now it is safe to copy the data from user space.
    if ( copy_from_user(gWriteBuffer, buf, count) )  {
        ret = -1;
        printk("%s: XPCIe_Write: Failed copy to user.\n",dev->name);
        goto exit;
    }

    //set DMA semaphore if in loopback
    down(&gSem[SEM_DMA]);

    // pci_map_single return the physical address corresponding to
    // the virtual address passed to it as the 2nd parameter

    dma_addr = pci_map_single(gDev, gWriteBuffer, DMA_BUF_SIZE, PCI_DMA_FROMDEVICE);
    if ( 0 == dma_addr )  {
        printk("%s: XPCIe_Write: Map error.\n",dev->name);
        ret = -1;
        goto exit;
    }

    // Now pass the physical address to the device hardware. This is now
    // the source physical address for the DMA and hence the to be
    // put on Memory Transactions

    // Do DMA transfer here....

    printk("%s: XPCIe_Write: WriteBuf Virt Addr = %lXx Phy Addr = %X.\n",
           dev->name, (unsigned long)gReadBuffer, (unsigned int)dma_addr);

    // Unmap the DMA buffer so it is safe for normal access again.
    pci_unmap_single(gDev, dma_addr, DMA_BUF_SIZE, PCI_DMA_FROMDEVICE);

    up(&gSem[SEM_DMA]);

    exit:
      return (ret);
}
#endif

u32 XPCIe_ReadCfgReg(xbmd_device *dev, u32 byte) {
   u32 pciReg;
   if (pci_read_config_dword(dev->pciDev, byte, &pciReg) < 0) {
        printk("%s: XPCIe_ReadCfgReg: Reading PCI interface failed.",dev->name);
        return (-1);
   }
   return (pciReg);
}

u32 XPCIe_WriteCfgReg(xbmd_device *dev, u32 byte, u32 val) {
   if (pci_write_config_dword(dev->pciDev, byte, val) < 0) {
        printk("%s: XPCIe_WriteCfgReg: Writing PCI interface failed.",dev->name);
        return (-1);
   }
   return 1;
}
