#include <linux/ioctl.h>

#define KILOBYTES(x)  ((x) * 1024L)
#define MEGABYTES(x)  (KILOBYTES(x) * 1024L)


#define SUCCESS    0
#define CRIT_ERR  -1

#define VERBOSE    1

#define DMA_BUF_SIZE   MEGABYTES(4)

typedef enum XbmDmaControlReg
{
    Reg_DeviceCS = 0, // NOTE(michiel): Control Status
    Reg_DeviceDMACS,  // NOTE(michiel): DMA Control Status
    Reg_WriteTlpAddress,
    Reg_WriteTlpSize,
    Reg_WriteTlpCount,
    Reg_WriteTlpPattern,
    Reg_ReadTlpPattern,
    Reg_ReadTlpAddress,
    Reg_ReadTlpSize,
    Reg_ReadTlpCount,
    Reg_WriteDMAPerf,
    Reg_ReadDMAPerf,
    Reg_ReadComplStatus,
    Reg_ComplWithData,
    Reg_ComplSize,
    Reg_DeviceLinkWidth,
    Reg_DeviceLinkTlpSize,
    Reg_DeviceMiscControl,
    Reg_DeviceMSIControl,
    Reg_DeviceDirectedLinkChange,
    Reg_DeviceFCControl,
    Reg_DeviceFCPostedInfo,
    Reg_DeviceFCNonPostedInfo,
    Reg_DeviceFCCompletionInfo,
    
} XbmDmaControlReg;

#define XBMD_IOC_MAGIC          '!'

#define XBMD_IOC_INITCARD       _IO(XBMD_IOC_MAGIC, 0)
#define XBMD_IOC_RESET          _IO(XBMD_IOC_MAGIC, 1)
#define XBMD_IOC_DISP_REGS      _IO(XBMD_IOC_MAGIC, 2)
#define XBMD_IOC_READ_CTRL      _IOR(XBMD_IOC_MAGIC, 3, u32)
#define XBMD_IOC_READ_DMA_CTRL  _IOR(XBMD_IOC_MAGIC, 4, u32)
#define XBMD_IOC_READ_WR_ADDR   _IOR(XBMD_IOC_MAGIC, 5, u32)
#define XBMD_IOC_READ_WR_LEN    _IOR(XBMD_IOC_MAGIC, 6, u32)
#define XBMD_IOC_READ_WR_COUNT  _IOR(XBMD_IOC_MAGIC, 7, u32)
#define XBMD_IOC_READ_WR_PTRN   _IOR(XBMD_IOC_MAGIC, 8, u32)
#define XBMD_IOC_READ_RD_PTRN   _IOR(XBMD_IOC_MAGIC, 9, u32)
#define XBMD_IOC_READ_RD_ADDR   _IOR(XBMD_IOC_MAGIC, 10, u32)
#define XBMD_IOC_READ_RD_LEN    _IOR(XBMD_IOC_MAGIC, 11, u32)
#define XBMD_IOC_READ_RD_COUNT  _IOR(XBMD_IOC_MAGIC, 12, u32)
#define XBMD_IOC_READ_WR_PERF   _IOR(XBMD_IOC_MAGIC, 13, u32)
#define XBMD_IOC_READ_RD_PERF   _IOR(XBMD_IOC_MAGIC, 14, u32)
#define XBMD_IOC_READ_CMPL      _IOR(XBMD_IOC_MAGIC, 15, u32)
#define XBMD_IOC_READ_CWDATA    _IOR(XBMD_IOC_MAGIC, 16, u32)
#define XBMD_IOC_READ_CSIZE     _IOR(XBMD_IOC_MAGIC, 17, u32)
#define XBMD_IOC_READ_LINKWDTH  _IOR(XBMD_IOC_MAGIC, 18, u32)
#define XBMD_IOC_READ_LINKLEN   _IOR(XBMD_IOC_MAGIC, 19, u32)
#define XBMD_IOC_READ_MISC_CTL  _IOR(XBMD_IOC_MAGIC, 20, u32)
#define XBMD_IOC_READ_INTRPT    _IOR(XBMD_IOC_MAGIC, 21, u32)
#define XBMD_IOC_READ_DIR_LINK  _IOR(XBMD_IOC_MAGIC, 22, u32)
#define XBMD_IOC_READ_FC_CTRL   _IOR(XBMD_IOC_MAGIC, 23, u32)
#define XBMD_IOC_READ_FC_POST   _IOR(XBMD_IOC_MAGIC, 24, u32)
#define XBMD_IOC_READ_FC_NPOST  _IOR(XBMD_IOC_MAGIC, 25, u32)
#define XBMD_IOC_READ_FC_CMPL   _IOR(XBMD_IOC_MAGIC, 26, u32)
#define XBMD_IOC_WRITE_DMA_CTRL _IOW(XBMD_IOC_MAGIC, 27, u32)
#define XBMD_IOC_WRITE_WR_LEN   _IOW(XBMD_IOC_MAGIC, 28, u32)
#define XBMD_IOC_WRITE_WR_COUNT _IOW(XBMD_IOC_MAGIC, 29, u32)
#define XBMD_IOC_WRITE_WR_PTRN  _IOW(XBMD_IOC_MAGIC, 30, u32)
#define XBMD_IOC_WRITE_RD_LEN   _IOW(XBMD_IOC_MAGIC, 31, u32)
#define XBMD_IOC_WRITE_RD_COUNT _IOW(XBMD_IOC_MAGIC, 32, u32)
#define XBMD_IOC_WRITE_RD_PTRN  _IOW(XBMD_IOC_MAGIC, 33, u32)
#define XBMD_IOC_WRITE_MISC_CTL _IOW(XBMD_IOC_MAGIC, 34, u32)
#define XBMD_IOC_WRITE_DIR_LINK _IOW(XBMD_IOC_MAGIC, 35, u32)
#define XBMD_IOC_RD_BMD_REG     _IOWR(XBMD_IOC_MAGIC, 36, u32)
#define XBMD_IOC_RD_CFG_REG     _IOWR(XBMD_IOC_MAGIC, 37, u32)
#define XBMD_IOC_WR_BMD_REG     _IOW(XBMD_IOC_MAGIC, 38, u64)
#define XBMD_IOC_WR_CFG_REG     _IOW(XBMD_IOC_MAGIC, 39, u64)
