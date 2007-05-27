 /*
  * UAE - The Un*x Amiga Emulator
  *
  * A4000T NCR 53C710 SCSI (nothing done yet)
  *
  * (c) 2007 Toni Wilen
  */

#define NCR_LOG 1

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "ncr_scsi.h"
#include "zfile.h"

#define NCRNAME "NCR53C710"
#define NCR_REGS 0x40

#define ROM_VECTOR 0x8080
#define ROM_OFFSET 0x8000
#define ROM_SIZE 32768
#define ROM_MASK (ROM_SIZE - 1)
#define BOARD_SIZE (65536 * 2)

static uae_u8 *rom;
static int configured;
static uae_u8 acmemory[100];

static uae_u8 ncrregs[NCR_REGS];

struct ncrscsi {
    char *name;
    int be, le;
};

static struct ncrscsi regsinfo[] =
{
    "SCNTL0",	 0,  3,
    "SCNTL1",	 1,  2,
    "SDID",	 2,  1,
    "SIEN",	 3,  0,
    "SCID",	 4,  7,
    "SXFER",	 5,  6,
    "SODL",	 6,  5,
    "SOCL",	 7,  4,
    "SFBR",	 8, 11,
    "SIDL",	 9, 10,
    "SBDL",	10, -1,
    "SBCL",	11,  8,
    "DSTAT",	12, 15,
    "SSTAT0",	13, 14,
    "SSTAT1",	14, 13,
    "SSTAT2",	15, 12,
    "DSA0",	16, 19,
    "DSA1",	17, 18,
    "DSA2",	18, 17,
    "DSA3",	19, 16,
    "CTEST0",	20, 23,
    "CTEST1",	21, 22,
    "CTEST2",	22, 21,
    "CTEST3",	23, 20,
    "CTEST4",	24, 27,
    "CTEST5",	25, 26,
    "CTEST6",	26, 25,
    "CTEST7",	27, 24,
    "TEMP0",	28, 31,
    "TEMP1",	29, 30,
    "TEMP2",	30, 29,
    "TEMP3",	31, 28,
    "DFIFO",	32, 35,
    "ISTAT",	33, 34,
    "CTEST8",	34, 33,
    "LCRC",	35, 32,
    "DBC0",	36, 39,
    "DBC1",	37, 38,
    "DBC2",	38, 37,
    "DCMD",	39, 36,
    "DNAD0",	40, 43,
    "DNAD1",	41, 42,
    "DNAD2",	42, 41,
    "DNAD3",	43, 40,
    "DSP0",	44, 47,
    "DSP1",	45, 46,
    "DSP2",	46, 45,
    "DSP3",	47, 44,
    "DSPS0",	48, 51,
    "DSPS1",	49, 50,
    "DSPS2",	50, 49,
    "DSPS3",	51, 48,
    "SCRATCH0",	52, 55,
    "SCRATCH1",	53, 54,
    "SCRATCH2",	54, 53,
    "SCRATCH3",	55, 52,
    "DMODE",	56, 59,
    "DIEN",	57, 58,
    "DWT",	58, 57,
    "DCNTL",	59, 56,
    "ADDER0",	60, 63,
    "ADDER1",	61, 62,
    "ADDER2",	62, 61,
    "ADDER3",	63, 60,
    NULL
};

static char *regname(uaecptr addr)
{
    int i;

    for (i = 0; regsinfo[i].name; i++) {
	if (regsinfo[i].le == addr)
	    return regsinfo[i].name;
    }
    return "?";
}

#define SCNTL0_REG                      0x03
#define         FULL_ARBITRATION        0xc0
#define         PARITY                  0x08
#define         ENABLE_PARITY           0x04
#define         AUTO_ATN                0x02
#define SCNTL1_REG                      0x02
#define         SLOW_BUS                0x80
#define         ENABLE_SELECT           0x20   
#define         ASSERT_RST              0x08
#define         ASSERT_EVEN_PARITY      0x04
#define SDID_REG                        0x01
#define SIEN_REG                        0x00
#define         PHASE_MM_INT            0x80
#define         FUNC_COMP_INT           0x40
#define         SEL_TIMEOUT_INT         0x20
#define         SELECT_INT              0x10
#define         GROSS_ERR_INT           0x08               
#define         UX_DISC_INT             0x04   
#define         RST_INT                 0x02                  
#define         PAR_ERR_INT             0x01
#define SCID_REG                        0x07
#define SXFER_REG                       0x06
#define         ASYNC_OPERATION         0x00                    
#define SODL_REG                        0x05  
#define SOCL_REG                        0x04
#define SFBR_REG                        0x0b
#define SIDL_REG                        0x0a
#define SBDL_REG                        0x0a
#define SBCL_REG                        0x08
#define         SBCL_IO                 0x01                
#define         SYNC_DIV_AS_ASYNC       0x00
#define         SYNC_DIV_1_0            0x01
#define         SYNC_DIV_1_5            0x02
#define         SYNC_DIV_2_0            0x03
#define DSTAT_REG                       0x0e
#define         ILGL_INST_DETECTED      0x01
#define         WATCH_DOG_INTERRUPT     0x02
#define         SCRIPT_INT_RECEIVED     0x04
#define         ABORTED                 0x10
#define SSTAT0_REG                      0x0e
#define         PARITY_ERROR            0x01
#define         SCSI_RESET_DETECTED     0x02
#define         UNEXPECTED_DISCONNECT   0x04
#define         SCSI_GROSS_ERROR        0x08
#define         SELECTED                0x10
#define         SELECTION_TIMEOUT       0x20
#define         FUNCTION_COMPLETE       0x40
#define         PHASE_MISMATCH          0x80                    
#define SSTAT1_REG                      0x0d                        
#define         SIDL_REG_FULL           0x80
#define         SODR_REG_FULL           0x40
#define         SODL_REG_FULL           0x20
#define SSTAT2_REG                      0x0c          
#define CTEST0_REG                      0x17
#define         BTB_TIMER_DISABLE       0x40
#define CTEST1_REG                      0x16
#define CTEST2_REG                      0x15
#define CTEST3_REG                      0x14
#define CTEST4_REG                      0x1b
#define         DISABLE_FIFO            0x00   
#define         SLBE                    0x10
#define         SFWR                    0x08
#define         BYTE_LANE0              0x04
#define         BYTE_LANE1              0x05
#define         BYTE_LANE2              0x06
#define         BYTE_LANE3              0x07
#define         SCSI_ZMODE              0x20
#define         ZMODE                   0x40
#define CTEST5_REG                      0x1a            
#define         MASTER_CONTROL          0x10   
#define         DMA_DIRECTION           0x08                  
#define CTEST7_REG                      0x18
#define         BURST_DISABLE           0x80 /* 710 only */
#define         SEL_TIMEOUT_DISABLE     0x10 /* 710 only */
#define         DFP                     0x08                    
#define         EVP                     0x04      
#define         DIFF                    0x01
#define CTEST6_REG                      0x19
#define TEMP_REG                        0x1C
#define DFIFO_REG                       0x20
#define         FLUSH_DMA_FIFO          0x80
#define         CLR_FIFO                0x40
#define ISTAT_REG                       0x22               
#define         ABORT_OPERATION         0x80
#define         SOFTWARE_RESET_710      0x40
#define         DMA_INT_PENDING         0x01
#define         SCSI_INT_PENDING        0x02
#define         CONNECTED               0x08
#define CTEST8_REG                      0x21
#define         LAST_DIS_ENBL           0x01
#define         SHORTEN_FILTERING       0x04
#define         ENABLE_ACTIVE_NEGATION  0x10
#define         GENERATE_RECEIVE_PARITY 0x20
#define         CLR_FIFO_710            0x04
#define         FLUSH_DMA_FIFO_710      0x08
#define LCRC_REG                        0x20
#define DBC_REG                         0x25
#define DCMD_REG                        0x24
#define DNAD_REG                        0x28
#define DIEN_REG                        0x3a
#define         BUS_FAULT               0x20
#define         ABORT_INT               0x10                                                
#define         INT_INST_INT            0x04                         
#define         WD_INT                  0x02
#define         ILGL_INST_INT           0x01
#define DCNTL_REG                       0x38
#define         SOFTWARE_RESET          0x01            
#define         COMPAT_700_MODE         0x01
#define         SCRPTS_16BITS           0x20
#define         ASYNC_DIV_2_0           0x00
#define         ASYNC_DIV_1_5           0x40
#define         ASYNC_DIV_1_0           0x80
#define         ASYNC_DIV_3_0           0xc0
#define DMODE_710_REG                   0x3b
#define DMODE_700_REG                   0x34
#define         BURST_LENGTH_1          0x00
#define         BURST_LENGTH_2          0x40
#define         BURST_LENGTH_4          0x80
#define         BURST_LENGTH_8          0xC0
#define         DMODE_FC1               0x10
#define         DMODE_FC2               0x20
#define         BW16                    32  
#define         MODE_286                16                 
#define         IO_XFER                 8      
#define         FIXED_ADDR              4                     

static void INT2(void)
{
    if (ncrregs[SIEN_REG] == 0)
	return;
    INTREQ_f(0x8000 | 0x0008);
    write_log("IRQ\n");
}


static uae_u8 read_rom(uaecptr addr)
{
    int off;
    uae_u8 v;
    
    addr -= 0x8080;
    off = (addr & (BOARD_SIZE - 1)) / 4;
    off += 0x80;

    if ((addr & 2))
	v = (rom[off] & 0x0f) << 4;
    else
	v = (rom[off] & 0xf0);
    write_log("%08.8X:%04.4X = %02.2X PC=%08X\n", addr, off, v, M68K_GETPC);
    return v;
}

void ncr_bput2(uaecptr addr, uae_u32 val)
{
    addr &= 0xffff;
    if (addr >= NCR_REGS)
	return;
    switch (addr)
    {
	case SBCL_REG:
	break;
	case SCNTL1_REG:
	break;
	case ISTAT_REG:
	ncrregs[ISTAT_REG] = 0;
	break;
	case SCID_REG:
    	ncrregs[ISTAT_REG] |= 2;
	//ncrregs[SSTAT1_REG] |= 1 << 4; 
	//INT2();
	break;


    }
    write_log("%s write %04.4X (%s) = %02.2X PC=%08.8X\n", NCRNAME, addr, regname(addr), val & 0xff, M68K_GETPC);
    ncrregs[addr] = val;
}

uae_u32 ncr_bget2(uaecptr addr)
{
    uae_u32 v;

    addr &= 0xffff;
    if (rom && addr >= ROM_OFFSET)
	return read_rom(addr);
    if (addr >= NCR_REGS)
	return 0;
    v = ncrregs[addr];
    switch (addr)
    {
	case SSTAT2_REG:
	v &= ~7;
	v |= ncrregs[SBCL_REG] & 7;
	break;
	case SSTAT0_REG:
	v |= 0x20;
	break;
	case ISTAT_REG:
	if (v)
	    v |= 2;
	INT2();
	break;
	case CTEST8_REG:
	v &= 0x0f; // revision 0
	break;
    }

    write_log("%s read  %04.4X (%s) = %02.2X PC=%08.8X\n", NCRNAME, addr, regname(addr), v, M68K_GETPC);
    return v;
}

static addrbank ncr_bank;

static uae_u32 REGPARAM2 ncr_lget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = (ncr_bget2 (addr) << 24) | (ncr_bget2 (addr + 1) << 16) |
	(ncr_bget2 (addr + 2) << 8) | (ncr_bget2 (addr + 3));
#ifdef NCR_DEBUG
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("ncr_lget %08.8X=%08.8X PC=%08.8X\n", addr, v, M68K_GETPC);
#endif
    return v;
}

static uae_u32 REGPARAM2 ncr_wget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = (ncr_bget2 (addr) << 8) | ncr_bget2 (addr + 1);
#if NCR_DEBUG > 0
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("ncr_wget %08.8X=%04.4X PC=%08.8X\n", addr, v, M68K_GETPC);
#endif
    return v;
}

static uae_u32 REGPARAM2 ncr_bget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    if (!configured) {
	if (addr >= sizeof acmemory)
	    return 0;
	return acmemory[addr];
    }
    v = ncr_bget2 (addr);
    return v;
}

static void REGPARAM2 ncr_lput (uaecptr addr, uae_u32 l)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr &= 65535;
#if NCR_DEBUG > 0
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("ncr_lput %08.8X=%08.8X PC=%08.8X\n", addr, l, M68K_GETPC);
#endif
    ncr_bput2 (addr, l >> 24);
    ncr_bput2 (addr + 1, l >> 16);
    ncr_bput2 (addr + 2, l >> 8);
    ncr_bput2 (addr + 3, l);
}

static void REGPARAM2 ncr_wput (uaecptr addr, uae_u32 w)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr &= 65535;
#if NCR_DEBUG > 0
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("ncr_wput %04.4X=%04.4X PC=%08.8X\n", addr, w & 65535, M68K_GETPC);
#endif
    ncr_bput2 (addr, w >> 8);
    ncr_bput2 (addr + 1, w);
}

static void REGPARAM2 ncr_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    b &= 0xff;
    addr &= 65535;
    if (addr == 0x48) {
	map_banks (&ncr_bank, b, BOARD_SIZE >> 16, BOARD_SIZE);
	write_log ("A4091 autoconfigured at %02.2X0000\n", b);
	configured = 1;
	expamem_next();
	return;
    }
    if (addr == 0x4c) {
	write_log ("A4091 AUTOCONFIG SHUT-UP!\n");
	configured = 1;
	expamem_next();
	return;
    }
    if (!configured)
	return;
    ncr_bput2 (addr, b);
}

static uae_u32 REGPARAM2 ncr_wgeti (uaecptr addr)
{
    uae_u32 v = 0xffff;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    if (addr >= ROM_OFFSET)
	v = (rom[addr & ROM_MASK] << 8) | rom[(addr + 1) & ROM_MASK];
    return v;
}
static uae_u32 REGPARAM2 ncr_lgeti (uaecptr addr)
{
    uae_u32 v = 0xffff;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = (ncr_wgeti(addr) << 16) | ncr_wgeti(addr + 2);
    return v;
}

static addrbank ncr_bank = {
    ncr_lget, ncr_wget, ncr_bget,
    ncr_lput, ncr_wput, ncr_bput,
    default_xlate, default_check, NULL, "A4091",
    ncr_lgeti, ncr_wgeti, ABFLAG_IO
};

static void ew (int addr, uae_u32 value)
{
    addr &= 0xffff;
    if (addr == 00 || addr == 02 || addr == 0x40 || addr == 0x42) {
	acmemory[addr] = (value & 0xf0);
	acmemory[addr + 2] = (value & 0x0f) << 4;
    } else {
	acmemory[addr] = ~(value & 0xf0);
	acmemory[addr + 2] = ~((value & 0x0f) << 4);
    }
}

void ncr_free (void)
{
}

void ncr_reset (void)
{
    configured = 0; 
}

void ncr_init (void)
{
    struct zfile *z;
    int roms[3];
    struct romlist *rl;

    configured = 0;
    memset (acmemory, 0xff, 100);
    ew (0x00, 0xc0 | 0x02 | 0x10);
    /* A4091 hardware id */
    ew (0x04, 0x54);
    /* commodore's manufacturer id */
    ew (0x10, 0x02);
    ew (0x14, 0x02);
    /* rom vector */
    ew (0x28, ROM_VECTOR >> 8);
    ew (0x2c, ROM_VECTOR);

    roms[0] = 57;
    roms[1] = 56;
    roms[2] = -1;

    rl = getromlistbyids(roms);
    if (rl) {
	write_log("A4091 BOOT ROM '%s' %d.%d ", rl->path, rl->rd->ver, rl->rd->rev);
	z = zfile_fopen(rl->path, "rb");
	if (z) {
	    write_log("loaded\n");
	    rom = xmalloc (ROM_SIZE);
	    zfile_fread (rom, ROM_SIZE, 1, z);
	    zfile_fclose(z);
	} else {
	    write_log("failed to load\n");
	}
    } else {
	romwarning(roms);
    }
    map_banks (&ncr_bank, 0xe80000 >> 16, 0x10000 >> 16, 0x10000);
}
