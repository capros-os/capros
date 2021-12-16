
/* see bochs-cvs/iodev/vga.{h,cc} for this interface.  
 * note that the VBE interface is largely seperate from the VGA interface
 */

#define BOCHS_VBE_DISPI_MAX_XRES              1024
#define BOCHS_VBE_DISPI_MAX_YRES              768

#define BOCHS_VBE_DISPI_IOPORT_INDEX          0xFF80
#define BOCHS_VBE_DISPI_IOPORT_DATA           0xFF81

#define BOCHS_VBE_DISPI_INDEX_ID              0x0
#define BOCHS_VBE_DISPI_INDEX_XRES            0x1
#define BOCHS_VBE_DISPI_INDEX_YRES            0x2
#define BOCHS_VBE_DISPI_INDEX_BPP             0x3
#define BOCHS_VBE_DISPI_INDEX_ENABLE          0x4
#define BOCHS_VBE_DISPI_INDEX_BANK            0x5
#define BOCHS_VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define BOCHS_VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define BOCHS_VBE_DISPI_INDEX_X_OFFSET        0x8
#define BOCHS_VBE_DISPI_INDEX_Y_OFFSET        0x9

#define BOCHS_VBE_DISPI_ID0                   0xB0C0
#define BOCHS_VBE_DISPI_ID1                   0xB0C1
#define BOCHS_VBE_DISPI_ID2                   0xB0C2

#define BOCHS_VBE_DISPI_BPP_4                 0x04
#define BOCHS_VBE_DISPI_BPP_8                 0x08
#define BOCHS_VBE_DISPI_BPP_15                0x0F
#define BOCHS_VBE_DISPI_BPP_16                0x10
#define BOCHS_VBE_DISPI_BPP_24                0x18
#define BOCHS_VBE_DISPI_BPP_32                0x20

#define BOCHS_VBE_DISPI_DISABLED              0x00
#define BOCHS_VBE_DISPI_ENABLED               0x01
#define BOCHS_VBE_DISPI_NOCLEARMEM            0x80

#define VBE_DISPI_LFB_PHYSICAL_ADDRESS  0xE0000000
