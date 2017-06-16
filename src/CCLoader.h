
/******************************************************************************
* DEFINES
*/
// Start addresses on DUP (Increased buffer size improves performance)
#define ADDR_BUF0                   0x0000	// Buffer (512 bytes)
#define ADDR_DMA_DESC_0             0x0200	// DMA descriptors (8 bytes)
#define ADDR_DMA_DESC_1             (ADDR_DMA_DESC_0 + 8)

// DMA channels used on DUP
#define CH_DBG_TO_BUF0              0x01	// Channel 0
#define CH_BUF0_TO_FLASH            0x02	// Channel 1

// Debug commands
#define CMD_CHIP_ERASE              0x10
#define CMD_WR_CONFIG               0x19
#define CMD_RD_CONFIG               0x24
#define CMD_READ_STATUS             0x30
#define CMD_RESUME                  0x4C
#define CMD_DEBUG_INSTR_1B          (0x54|1)
#define CMD_DEBUG_INSTR_2B          (0x54|2)
#define CMD_DEBUG_INSTR_3B          (0x54|3)
#define CMD_BURST_WRITE             0x80
#define CMD_GET_CHIP_ID             0x68

// Debug status bitmasks
#define STATUS_CHIP_ERASE_BUSY_BM   0x80	// New debug interface
#define STATUS_PCON_IDLE_BM         0x40
#define STATUS_CPU_HALTED_BM        0x20
#define STATUS_PM_ACTIVE_BM         0x10
#define STATUS_HALT_STATUS_BM       0x08
#define STATUS_DEBUG_LOCKED_BM      0x04
#define STATUS_OSC_STABLE_BM        0x02
#define STATUS_STACK_OVERFLOW_BM    0x01

// DUP registers (XDATA space address)
#define DUP_DBGDATA                 0x6260	// Debug interface data buffer
#define DUP_FCTL                    0x6270	// Flash controller
#define DUP_FADDRL                  0x6271	// Flash controller addr
#define DUP_FADDRH                  0x6272	// Flash controller addr
#define DUP_FWDATA                  0x6273	// Clash controller data buffer
#define DUP_CLKCONSTA               0x709E	// Sys clock status
#define DUP_CLKCONCMD               0x70C6	// Sys clock configuration
#define DUP_MEMCTR                  0x70C7	// Flash bank xdata mapping
#define DUP_DMA1CFGL                0x70D2	// Low byte, DMA config ch. 1
#define DUP_DMA1CFGH                0x70D3	// Hi byte , DMA config ch. 1
#define DUP_DMA0CFGL                0x70D4	// Low byte, DMA config ch. 0
#define DUP_DMA0CFGH                0x70D5	// Low byte, DMA config ch. 0
#define DUP_DMAARM                  0x70D6	// DMA arming register

// Utility macros
//! Low nibble of 16bit variable
#define LOBYTE(w)           ((unsigned char)(w))
//! High nibble of 16bit variable
#define HIBYTE(w)           ((unsigned char)(((unsigned short)(w) >> 8) & 0xFF))
//! Convert XREG register declaration to an XDATA integer address
//#define XREG(addr)       ((unsigned char volatile __xdata *) 0)[addr]
//#define FCTL            XREG( 0x6270 )
//#define XREG_TO_INT(a)      ((unsigned short)(&(a)))

// Commands to Bootloader
#define SBEGIN                0x01
#define SDATA                 0x02
#define SRSP                  0x03
#define SEND                  0x04
#define ERRO                 0x05
#define WAITING               0x00
#define RECEIVING             0x01

/******************************************************************************
 VARIABLES*/
//! DUP DMA descriptor
const unsigned char dma_desc_0[8] = {
	// Debug Interface -> Buffer
	HIBYTE(DUP_DBGDATA),	// src[15:8]
	LOBYTE(DUP_DBGDATA),	// src[7:0]
	HIBYTE(ADDR_BUF0),	// dest[15:8]
	LOBYTE(ADDR_BUF0),	// dest[7:0]
	0,			// len[12:8] - filled in later
	0,			// len[7:0]
	31,			// trigger: DBG_BW
	0x11			// increment destination
};

//! DUP DMA descriptor
const unsigned char dma_desc_1[8] = {
	// Buffer -> Flash controller
	HIBYTE(ADDR_BUF0),	// src[15:8]
	LOBYTE(ADDR_BUF0),	// src[7:0]
	HIBYTE(DUP_FWDATA),	// dest[15:8]
	LOBYTE(DUP_FWDATA),	// dest[7:0]
	0,			// len[12:8] - filled in later
	0,			// len[7:0]
	18,			// trigger: FLASH
	0x42,			// increment source
};

// program arguments
struct args {
	int DD;
	int DC;
	int RESET;
	int verify;
	int retries;
	char *fName;
};
