 /*
  * UAE - The Un*x Amiga Emulator
  *
  * A590/A2091/A3000/CDTV SCSI expansion (DMAC/SuperDMAC + WD33C93) emulation
  *
  * Copyright 2007 Toni Wilen
  *
  */

#define A2091_DEBUG 0
#define A3000_DEBUG 0
#define WD33C93_DEBUG 0

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "debug.h"
#include "scsi.h"
#include "a2091.h"
#include "blkdev.h"
#include "gui.h"
#include "zfile.h"
#include "filesys.h"
#include "autoconf.h"
#include "cdtv.h"

#define ROM_VECTOR 0x2000
#define ROM_OFFSET 0x2000
#define ROM_SIZE 16384
#define ROM_MASK (ROM_SIZE - 1)

/* SuperDMAC CNTR bits. */
#define SCNTR_TCEN               (1<<5)
#define SCNTR_PREST              (1<<4)
#define SCNTR_PDMD               (1<<3)
#define SCNTR_INTEN              (1<<2)
#define SCNTR_DDIR               (1<<1)
#define SCNTR_IO_DX              (1<<0)
/* DMAC CNTR bits. */
#define CNTR_TCEN               (1<<7)
#define CNTR_PREST              (1<<6)
#define CNTR_PDMD               (1<<5)
#define CNTR_INTEN              (1<<4)
#define CNTR_DDIR               (1<<3)
/* ISTR bits. */
#define ISTR_INTX               (1<<8)
#define ISTR_INT_F              (1<<7)
#define ISTR_INTS               (1<<6)  
#define ISTR_E_INT              (1<<5)
#define ISTR_INT_P              (1<<4)
#define ISTR_UE_INT             (1<<3)  
#define ISTR_OE_INT             (1<<2)
#define ISTR_FF_FLG             (1<<1)
#define ISTR_FE_FLG             (1<<0)  

/* wd register names */
#define WD_OWN_ID		0x00
#define WD_CONTROL		0x01
#define WD_TIMEOUT_PERIOD	0x02
#define WD_CDB_1		0x03
#define WD_CDB_2		0x04
#define WD_CDB_3		0x05
#define WD_CDB_4		0x06
#define WD_CDB_5		0x07
#define WD_CDB_6		0x08
#define WD_CDB_7		0x09
#define WD_CDB_8		0x0a
#define WD_CDB_9		0x0b
#define WD_CDB_10		0x0c
#define WD_CDB_11		0x0d
#define WD_CDB_12		0x0e
#define WD_TARGET_LUN		0x0f
#define WD_COMMAND_PHASE	0x10
#define WD_SYNCHRONOUS_TRANSFER 0x11
#define WD_TRANSFER_COUNT_MSB	0x12
#define WD_TRANSFER_COUNT	0x13
#define WD_TRANSFER_COUNT_LSB	0x14
#define WD_DESTINATION_ID	0x15
#define WD_SOURCE_ID		0x16
#define WD_SCSI_STATUS		0x17
#define WD_COMMAND		0x18
#define WD_DATA			0x19
#define WD_QUEUE_TAG		0x1a
#define WD_AUXILIARY_STATUS	0x1f 
/* WD commands */
#define WD_CMD_RESET		0x00
#define WD_CMD_ABORT		0x01
#define WD_CMD_ASSERT_ATN	0x02
#define WD_CMD_NEGATE_ACK	0x03
#define WD_CMD_DISCONNECT	0x04
#define WD_CMD_RESELECT		0x05
#define WD_CMD_SEL_ATN		0x06
#define WD_CMD_SEL		0x07
#define WD_CMD_SEL_ATN_XFER	0x08
#define WD_CMD_SEL_XFER		0x09
#define WD_CMD_RESEL_RECEIVE	0x0a
#define WD_CMD_RESEL_SEND	0x0b
#define WD_CMD_WAIT_SEL_RECEIVE	0x0c
#define WD_CMD_TRANS_ADDR	0x18
#define WD_CMD_TRANS_INFO	0x20
#define WD_CMD_TRANSFER_PAD	0x21          
#define WD_CMD_SBT_MODE		0x80

#define CSR_MSGIN	    0x20      
#define CSR_SDP		    0x21
#define CSR_SEL_ABORT	    0x22
#define CSR_RESEL_ABORT	    0x25
#define CSR_RESEL_ABORT_AM  0x27   
#define CSR_ABORT	    0x28
/* successful completion interrupts */
#define CSR_RESELECT	    0x10
#define CSR_SELECT	    0x11
#define CSR_SEL_XFER_DONE   0x16
#define CSR_XFER_DONE	    0x18
  /* terminated interrupts */
#define CSR_INVALID	    0x40
#define CSR_UNEXP_DISC	    0x41
#define CSR_TIMEOUT	    0x42   
#define CSR_PARITY	    0x43   
#define CSR_PARITY_ATN	    0x44
#define CSR_BAD_STATUS	    0x45
#define CSR_UNEXP	    0x48   
  /* service required interrupts */
#define CSR_RESEL	    0x80         
#define CSR_RESEL_AM	    0x81   
#define CSR_DISC	    0x85           
#define CSR_SRV_REQ	    0x88 
/* SCSI Bus Phases */          
#define PHS_DATA_OUT	    0x00
#define PHS_DATA_IN	    0x01
#define PHS_COMMAND	    0x02
#define PHS_STATUS	    0x03
#define PHS_MESS_OUT	    0x06
#define PHS_MESS_IN	    0x07


static int configured;
static uae_u8 dmacmemory[100];
static uae_u8 *rom;

static uae_u32 dmac_istr, dmac_cntr;
static uae_u32 dmac_dawr;
static uae_u32 dmac_acr;
static uae_u32 dmac_wtc;
static int dmac_dma;
static uae_u8 sasr, scmd, auxstatus;
static int wd_used;
static int wd_phase, wd_next_phase, wd_busy;
static int wd_dataoffset, wd_tc;
static uae_u8 wd_data[32];

static int superdmac;
static int scsiirqdelay;

struct scsi_data *scsis[8];

uae_u8 wdregs[32];

static int isirq(void)
{
    if (superdmac) {
	if ((dmac_cntr & SCNTR_INTEN) && (dmac_istr & (ISTR_INTS | ISTR_E_INT)))
	    return 1;
    } else {
        if ((dmac_cntr & CNTR_INTEN) && (dmac_istr & (ISTR_INTS | ISTR_E_INT)))
	    return 1;
    }
    return 0;
}

void rethink_a2091(void)
{
    if (isirq()) {
	uae_int_requested |= 2;
#if A2091_DEBUG > 2 || A3000_DEBUG > 2
	write_log("Interrupt_RETHINK\n");
#endif
    } else {
	uae_int_requested &= ~2;
    }
}

static void INT2(int quick)
{
    int irq = 0;

    if (!(auxstatus & 0x80))
	return;
    dmac_istr |= ISTR_INTS;
    if (isirq()) {
        if (quick)
	    uae_int_requested |= 2;
	else
	    scsiirqdelay = 2;
    }
}

void scsi_hsync(void)
{
    if (scsiirqdelay == 1) {
	scsiirqdelay = 0;
	uae_int_requested |= 2;
#if A2091_DEBUG > 2 || A3000_DEBUG > 2
	write_log("Interrupt\n");
#endif
	return;
    }
    if (scsiirqdelay > 1)
	scsiirqdelay--;
}

static void dmac_start_dma(void)
{
#if A3000_DEBUG > 0 || A2091_DEBUG > 0
    write_log("DMAC DMA started, ADDR=%08X, LEN=%08X words\n", dmac_acr, dmac_wtc);
#endif
    dmac_dma = 1;
}
static void dmac_stop_dma(void)
{
    dmac_dma = 0;
    dmac_istr &= ~ISTR_E_INT;
}

static void dmac_reset(void)
{
#if WD33C93_DEBUG > 0
    if (superdmac)
	write_log("A3000 %s SCSI reset\n", WD33C93);
    else
	write_log("A2091 %s SCSI reset\n", WD33C93);
#endif
}

static void incsasr(int w)
{
    if (sasr == WD_AUXILIARY_STATUS || sasr == WD_DATA || sasr == WD_COMMAND)
	return;
    if (w && sasr == WD_SCSI_STATUS)
	return;
    sasr++;
    sasr &= 0x1f;
}

static void dmac_cint(void)
{
    dmac_istr = 0;
    rethink_a2091();
}

static void set_status(uae_u8 status, int quick)
{
    wdregs[WD_SCSI_STATUS] = status;
    auxstatus |= 0x80;
    if (!currprefs.cs_a2091 && currprefs.cs_mbdmac != 1)
	return;
    INT2(quick);
}

static char *scsitostring(void)
{
    static char buf[200];
    char *p;
    int i;

    p = buf;
    p[0] = 0;
    for (i = 0; i < wd_tc && i < sizeof wd_data; i++) {
	if (i > 0) {
	    strcat(p, ".");
	    p++;
	}
	sprintf(p, "%02X", wd_data[i]);
	p += strlen(p);
    }
    return buf;
}

static void wd_cmd_sel_xfer(void)
{
    int phase = wdregs[WD_COMMAND_PHASE];
#if WD33C93_DEBUG > 0
    write_log("* %s select and transfer, phase=%02X\n", WD33C93, phase);
#endif
    if (phase >= 0x46) {
	phase = 0x50;
	wdregs[WD_TARGET_LUN] = SCSIID->status;
    }
    wdregs[WD_COMMAND_PHASE] = phase;
    wd_phase = CSR_XFER_DONE | PHS_MESS_IN;
    SCSIID->buffer[0] = 0;
    set_status(wd_phase, 1);
}

static void do_dma(void)
{
    if (currprefs.cs_cdtvscsi)
	cdtv_getdmadata(&dmac_acr);
    if (SCSIID->direction == 0) {
	write_log("%s DMA but no data!?\n");
    } else if (SCSIID->direction < 0) {
	for (;;) {
	    uae_u8 v;
	    int status = scsi_receive_data(SCSIID, &v);
	    put_byte(dmac_acr, v);
	    if (wd_dataoffset < sizeof wd_data)
		wd_data[wd_dataoffset++] = v; 
	    dmac_acr++;
	    if (status)
		break;
	}
    } else if (SCSIID->direction > 0) {
	for (;;) {
	    int status;
	    uae_u8 v = get_byte(dmac_acr);
	    if (wd_dataoffset < sizeof wd_data)
		wd_data[wd_dataoffset++] = v;
	    status = scsi_send_data(SCSIID, v);
	    dmac_acr++;
	    if (status)
		break;
	}
    }
}

static void wd_do_transfer_out(void)
{
#if WD33C93_DEBUG > 0
    write_log("%s SCSI O %d/%d %s\n", WD33C93, wd_dataoffset, wd_tc, scsitostring());
#endif
    if (wdregs[WD_COMMAND_PHASE] == 0x11) {
	wdregs[WD_COMMAND_PHASE] = 0x20;
	wd_phase = CSR_XFER_DONE | PHS_COMMAND;
    } else if (wdregs[WD_COMMAND_PHASE] == 0x30) {
	/* command was sent */
	SCSIID->direction = scsi_data_dir(SCSIID);
	if (SCSIID->direction > 0) {
	    /* if write command, need to wait for data */
	    wd_phase = CSR_XFER_DONE | PHS_DATA_OUT;
	    wdregs[WD_COMMAND_PHASE] = 0x46;
	} else {
	    scsi_emulate_cmd(SCSIID);
	    if (SCSIID->data_len <= 0 || SCSIID->status != 0 || SCSIID->direction == 0) {
	        wd_phase = CSR_XFER_DONE | PHS_STATUS;
		wdregs[WD_COMMAND_PHASE] = 0x47;
	    } else {
	        wd_phase = CSR_XFER_DONE | PHS_DATA_IN;
		wdregs[WD_COMMAND_PHASE] = 0x3f;
	    }
	}
    } else if (wdregs[WD_COMMAND_PHASE] == 0x46) {
	if (SCSIID->direction > 0) {
	    /* data was sent */
	    scsi_emulate_cmd(SCSIID);
	    wd_phase = CSR_XFER_DONE | PHS_STATUS;
	}
        wdregs[WD_COMMAND_PHASE] = 0x47;
    }
    wd_dataoffset = 0;
    set_status(wd_phase, SCSIID->direction ? 0 : 1);
    wd_busy = 0;
}

static void wd_do_transfer_in(void)
{
#if WD33C93_DEBUG > 0
    write_log("%s SCSI I %d/%d %s\n", WD33C93, wd_dataoffset, wd_tc, scsitostring());
#endif
    wd_dataoffset = 0;
    if (wdregs[WD_COMMAND_PHASE] == 0x3f) {
	wdregs[WD_COMMAND_PHASE] = 0x47;
	wd_phase = CSR_XFER_DONE | PHS_STATUS;
    } else if (wdregs[WD_COMMAND_PHASE] == 0x47) {
	wdregs[WD_COMMAND_PHASE] = 0x50;
	wd_phase = CSR_XFER_DONE | PHS_MESS_IN;
    } else if (wdregs[WD_COMMAND_PHASE] == 0x50) {
	wdregs[WD_COMMAND_PHASE] = 0x60;
	wd_phase = CSR_DISC;
    }
    set_status(wd_phase, 1);
    wd_busy = 0;
    SCSIID->direction = 0;
}

static void wd_cmd_trans_info(void)
{
    if (wdregs[WD_COMMAND_PHASE] == 0x47)
	SCSIID->buffer[0] = SCSIID->status;
    if (wdregs[WD_COMMAND_PHASE] == 0x20)
	wdregs[WD_COMMAND_PHASE] = 0x30;
    wd_busy = 1;
    wd_tc = wdregs[WD_TRANSFER_COUNT_LSB] | (wdregs[WD_TRANSFER_COUNT] << 8) | (wdregs[WD_TRANSFER_COUNT_MSB] << 16);
    if (wdregs[WD_COMMAND] & 0x80)
	wd_tc = 1;
    wd_dataoffset = 0;
#if WD33C93_DEBUG > 0
    write_log("* %s transfer info phase=%02x len=%d dma=%d\n", WD33C93, wdregs[WD_COMMAND_PHASE], wd_tc, wdregs[WD_CONTROL] >> 6);
#endif
    scsi_start_transfer(SCSIID, wd_tc);
    if ((wdregs[WD_CONTROL] >> 6) == 2) {
        do_dma();
	if (SCSIID->direction < 0)
	    wd_do_transfer_in();
	else if (SCSIID->direction > 0)
	    wd_do_transfer_out();
	SCSIID->direction = 0;
	dmac_dma = 0;
    }
}

static void wd_cmd_sel_atn(void)
{
#if WD33C93_DEBUG > 0
    write_log("* %s select with atn, ID=%d\n", WD33C93, wdregs[WD_DESTINATION_ID]);
#endif
    wd_phase = 0;
    wdregs[WD_COMMAND_PHASE] = 0;
    if (SCSIID) {
	wd_phase = CSR_SELECT;
	set_status(wd_phase, 1);
	wdregs[WD_COMMAND_PHASE] = 0x10;
	return;
    }
    set_status(CSR_TIMEOUT, 0);
}

static void wd_cmd_reset(void)
{
#if WD33C93_DEBUG > 0
    write_log("%s reset\n", WD33C93);
#endif
    set_status(1, 0);
}

static void wd_cmd_abort(void)
{
#if WD33C93_DEBUG > 0
    write_log("%s abort\n", WD33C93);
#endif
    set_status(CSR_SEL_ABORT, 0);
}

static int writeonlyreg(int reg)
{
    if (reg == WD_SCSI_STATUS)
	return 1;
    return 0;
}

void wdscsi_put(uae_u8 d)
{
#if WD33C93_DEBUG > 1
    if (sasr != WD_DATA)
	write_log("W %s REG %02.2X (%d) = %02.2X (%d) PC=%08X\n", WD33C93, sasr, sasr, d, d, M68K_GETPC);
#endif
    if (!writeonlyreg(sasr))
	wdregs[sasr] = d;
    if (!wd_used) {
	wd_used = 1;
	write_log("%s in use\n", WD33C93);
    }
    if (sasr == WD_DATA) {
	if (wd_dataoffset < sizeof wd_data)
	    wd_data[wd_dataoffset] = wdregs[sasr];
	wd_dataoffset++;
	if (scsi_send_data(SCSIID, wdregs[sasr]))
	    wd_do_transfer_out();
    } else if (sasr == WD_COMMAND) {
	switch (d & 0x7f)
	{
	    case WD_CMD_RESET:
		wd_cmd_reset();
	    break;
	    break;
	    case WD_CMD_SEL_ATN:
	        wd_cmd_sel_atn();
	    break;
	    case WD_CMD_SEL_XFER:
		wd_cmd_sel_xfer();
	    break;
	    case WD_CMD_TRANS_INFO:
		wd_cmd_trans_info();
	    break;
	    default:
		write_log("%s unimplemented/unknown command %02.X\n", WD33C93, d);
	    break;
	}
    }
    incsasr(1);
}

void wdscsi_sasr(uae_u8 b)
{
    sasr = b;
}
uae_u8 wdscsi_getauxstatus(void)
{
    return (auxstatus & 0x80) | (wd_busy ? 0x20 : 0) | (wd_busy ? 0x01 : 0);
}

uae_u8 wdscsi_get(void)
{
    uae_u8 v, osasr = sasr;
    
    v = wdregs[sasr];
    if (sasr == WD_DATA) {
	int status = scsi_receive_data(SCSIID, &v);
	if (wd_dataoffset < sizeof wd_data)
	    wd_data[wd_dataoffset] = v;
	wd_dataoffset++;
	wdregs[sasr] = v;
	if (status)
	    wd_do_transfer_in();
    } else if (sasr == WD_SCSI_STATUS) {
	uae_int_requested &= ~2;
	auxstatus &= ~0x80;
	dmac_istr &= ~ISTR_INTS;
	if (wdregs[WD_COMMAND_PHASE] == 0x10) {
	    wdregs[WD_COMMAND_PHASE] = 0x11;
	    wd_phase = CSR_SRV_REQ | PHS_MESS_OUT;
	    set_status(wd_phase, 1);
	}
    }
    incsasr(0);
#if WD33C93_DEBUG > 1
    if (osasr != WD_DATA)
	write_log("R %s REG %02.2X (%d) = %02.2X (%d) PC=%08X\n", WD33C93, osasr, osasr, v, v, M68K_GETPC);
#endif
    return v;
}

static uae_u32 dmac_bget2 (uaecptr addr)
{
    uae_u32 v = 0;

    if (addr < 0x40)
	return dmacmemory[addr];
    if (addr >= ROM_OFFSET) {
	//write_log("%08x %08x\n", addr, M68K_GETPC);
	if (rom)
	    return rom[addr & ROM_MASK];
	return 0;
    }

    switch (addr)
    {
	case 0x41:
	v = dmac_istr;
	if (v)
	    v |= ISTR_INT_P;
	dmac_istr &= ~0xf;
	break;
	case 0x43:
	v = dmac_cntr;
	break;
	case 0x91:
	v = wdscsi_getauxstatus();
	break;
	case 0x93:
	v = wdscsi_get();
	break;
	/* XT IO */
	case 0xa1:
	case 0xa3:
	case 0xa5:
	case 0xa7:
	v = 0xff;
	break;
	case 0xe0:
	case 0xe1:
	if (!dmac_dma)
	    dmac_start_dma();
	break;
	case 0xe2:
	case 0xe3:
	dmac_stop_dma();
	break;
	case 0xe4:
	case 0xe5:
	dmac_cint();
	break;
	case 0xe8:
	case 0xe9:
	/* FLUSH */
	dmac_istr |= ISTR_FE_FLG;
	break;
    }
#if A2091_DEBUG > 0
    write_log ("dmac_bget %04.4X=%02.2X PC=%08.8X\n", addr, v, M68K_GETPC);
#endif
    return v;
}

static void dmac_bput2 (uaecptr addr, uae_u32 b)
{
    if (addr < 0x40)
	return;
    if (addr >= ROM_OFFSET)
	return;

    switch (addr)
    {
	case 0x43:
	dmac_cntr = b;
	if (dmac_cntr & CNTR_PREST)
	    dmac_reset ();
	break;
	case 0x80:
	dmac_wtc &= 0x00ffffff;
	dmac_wtc |= b << 24;
	break;
	case 0x81:
	dmac_wtc &= 0xff00ffff;
	dmac_wtc |= b << 16;
	break;
	case 0x82:
	dmac_wtc &= 0xffff00ff;
	dmac_wtc |= b << 8;
	break;
	case 0x83:
	dmac_wtc &= 0xffffff00;
	dmac_wtc |= b << 0;
	break;
	case 0x84:
	dmac_acr &= 0x00ffffff;
	dmac_acr |= b << 24;
	break;
	case 0x85:
	dmac_acr &= 0xff00ffff;
	dmac_acr |= b << 16;
	break;
	case 0x86:
	dmac_acr &= 0xffff00ff;
	dmac_acr |= b << 8;
	break;
	case 0x87:
	dmac_acr &= 0xffffff01;
	dmac_acr |= (b & ~ 1) << 0;
	break;
	case 0x8e:
	dmac_dawr &= 0x00ff;
	dmac_dawr |= b << 8;
	break;
	case 0x8f:
	dmac_dawr &= 0xff00;
	dmac_dawr |= b << 0;
	break;
	case 0x91:
	wdscsi_sasr(b);
	break;
	case 0x93:
	wdscsi_put(b);
	break;
	case 0xe0:
	case 0xe1:
	if (!dmac_dma)
	    dmac_start_dma();
	break;
	case 0xe2:
	case 0xe3:
	dmac_stop_dma();
	break;
	case 0xe4:
	case 0xe5:
	dmac_cint();
	break;
	case 0xe8:
	case 0xe9:
	/* FLUSH */
	dmac_istr |= ISTR_FE_FLG;
	break;
    }
#if A2091_DEBUG > 0
    write_log ("dmac_bput %04.4X=%02.2X PC=%08.8X\n", addr, b & 255, M68K_GETPC);
#endif
}



static uae_u32 REGPARAM2 dmac_lget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = dmac_bget2 (addr) << 24;
    v |= dmac_bget2 (addr + 1) << 16;
    v |= dmac_bget2 (addr + 2) << 8;
    v |= dmac_bget2 (addr + 3);
#ifdef A2091_DEBUG
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("dmac_lget %08.8X=%08.8X PC=%08.8X\n", addr, v, M68K_GETPC);
#endif
    return v;
}

static uae_u32 REGPARAM2 dmac_wget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = dmac_bget2 (addr) << 8;
    v |= dmac_bget2 (addr + 1);
#if A2091_DEBUG > 0
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("dmac_wget %08.8X=%04.4X PC=%08.8X\n", addr, v, M68K_GETPC);
#endif
    return v;
}

static uae_u32 REGPARAM2 dmac_bget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = dmac_bget2 (addr);
    if (!configured)
	return v;
    return v;
}

static void REGPARAM2 dmac_lput (uaecptr addr, uae_u32 l)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr &= 65535;
#if A2091_DEBUG > 0
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("dmac_lput %08.8X=%08.8X PC=%08.8X\n", addr, l, M68K_GETPC);
#endif
    dmac_bput2 (addr, l >> 24);
    dmac_bput2 (addr + 1, l >> 16);
    dmac_bput2 (addr + 2, l >> 8);
    dmac_bput2 (addr + 3, l);
}

static void REGPARAM2 dmac_wput (uaecptr addr, uae_u32 w)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    addr &= 65535;
#if A2091_DEBUG > 0
    if (addr >= 0x40 && addr < ROM_OFFSET)
	write_log ("dmac_wput %04.4X=%04.4X PC=%08.8X\n", addr, w & 65535, M68K_GETPC);
#endif
    dmac_bput2 (addr, w >> 8);
    dmac_bput2 (addr + 1, w);
}

static void REGPARAM2 dmac_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    b &= 0xff;
    addr &= 65535;
    if (addr == 0x48) {
	map_banks (&dmaca2091_bank, b, 0x10000 >> 16, 0x10000);
	write_log ("A590/A2091 autoconfigured at %02.2X0000\n", b);
	configured = 1;
	expamem_next();
	return;
    }
    if (addr == 0x4c) {
	write_log ("A590/A2091 DMAC AUTOCONFIG SHUT-UP!\n");
	configured = 1;
	expamem_next();
	return;
    }
    if (!configured)
	return;
    dmac_bput2 (addr, b);
}

static uae_u32 REGPARAM2 dmac_wgeti (uaecptr addr)
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
static uae_u32 REGPARAM2 dmac_lgeti (uaecptr addr)
{
    uae_u32 v = 0xffff;
#ifdef JIT
    special_mem |= S_READ;
#endif
    addr &= 65535;
    v = (dmac_wgeti(addr) << 16) | dmac_wgeti(addr + 2);
    return v;
}

addrbank dmaca2091_bank = {
    dmac_lget, dmac_wget, dmac_bget,
    dmac_lput, dmac_wput, dmac_bput,
    default_xlate, default_check, NULL, "A2091/A590",
    dmac_lgeti, dmac_wgeti, ABFLAG_IO
};

static void dmacreg_write(uae_u32 *reg, int addr, uae_u32 val, int size)
{
    addr = (size - 1) - addr;
    (*reg) &= ~(0xff << (addr * 8));
    (*reg) |= (val & 0xff) << (addr * 8);
}
static uae_u32 dmacreg_read(uae_u32 val, int addr, int size)
{
    addr = (size - 1) - addr;
    return (val >> (addr * 8)) & 0xff;
}

static void mbdmac_write (uae_u32 addr, uae_u32 val, int mode)
{
    if (currprefs.cs_mbdmac > 1)
	return;
#if A3000_DEBUG > 1
    write_log ("DMAC_WRITE %08.8X=%02.2X PC=%08.8X\n", addr, val & 0xff, M68K_GETPC);
#endif
    addr &= 0xffff;
    switch (addr)
    {
	case 0x02:
	case 0x03:
	dmacreg_write(&dmac_dawr, addr - 0x02, val, 2);
	break;
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	dmacreg_write(&dmac_wtc, addr - 0x04, val, 4);
	break;
	case 0x0a:
	case 0x0b:
	dmacreg_write(&dmac_cntr, addr - 0x0a, val, 2);
	if (dmac_cntr & SCNTR_PREST)
	    dmac_reset();
	break;
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
	dmacreg_write(&dmac_acr, addr - 0x0c, val, 4);
	break;
	case 0x12:
	case 0x13:
	if (!dmac_dma)
	    dmac_start_dma();
	break;
	case 0x16:
	case 0x17:
	/* FLUSH */
	dmac_istr |= ISTR_FE_FLG;
	break;
	case 0x1a:
	case 0x1b:
	dmac_cint();
	break;
	case 0x1e:
	case 0x1f:
	/* ISTR */
	break;
	case 0x3e:
	case 0x3f:
	dmac_stop_dma();
	break;
	case 0x41:
	if ((mode & 0x10) || ((mode & 0x70) > 0x10 && (mode & 0x0f) == 1))
	    sasr = val;
	break;
	case 0x49:
	sasr = val;
	break;
	case 0x43:
	wdscsi_put(val);
	break;
    }
}

static uae_u32 mbdmac_read (uae_u32 addr, int mode)
{
    uae_u32 vaddr = addr;
    uae_u32 v = 0xffffffff;
    
    if (currprefs.cs_mbdmac > 1)
	return 0;

    addr &= 0xffff;
    switch (addr)
    {
	case 0x02:
	case 0x03:
	v = dmacreg_read(dmac_dawr, addr - 0x02, 2);
	break;
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	v = 0xff;
	break;
	case 0x0a:
	case 0x0b:
	v = dmacreg_read(dmac_cntr, addr - 0x0a, 2);
	break;
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
	v = dmacreg_read(dmac_acr, addr - 0x0c, 4);
	break;
	case 0x12:
	case 0x13:
	if (!dmac_dma)
	    dmac_start_dma();
	v = 0;
	break;
	case 0x1a:
	case 0x1b:
	dmac_cint();
	v = 0;
	break;;
	case 0x1e:
	case 0x1f:
	v = dmacreg_read(dmac_istr, addr - 0x1e, 2);
	if (v & ISTR_INTS)
	    v |= ISTR_INT_P;
	dmac_istr &= ~15;
	break;
	case 0x3e:
	case 0x3f:
	dmac_stop_dma();
	v = 0;
	break;
	case 0x41:
	case 0x49:
	v = wdscsi_getauxstatus();
	break;
	case 0x43:
	v = wdscsi_get();
	break;
    }
#if A3000_DEBUG > 1
    write_log ("DMAC_READ %08.8X=%02.2X PC=%X\n", vaddr, v & 0xff, M68K_GETPC);
#endif
    return v;
}


static uae_u32 REGPARAM3 mbdmac_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 mbdmac_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 mbdmac_bget (uaecptr) REGPARAM;
static void REGPARAM3 mbdmac_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 mbdmac_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 mbdmac_bput (uaecptr, uae_u32) REGPARAM;

static uae_u32 REGPARAM2 mbdmac_lget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    v =  mbdmac_read (addr, 0x40 | 0) << 24;
    v |= mbdmac_read (addr + 1, 0x40 | 1) << 16;
    v |= mbdmac_read (addr + 2, 0x40 | 2) << 8;
    v |= mbdmac_read (addr + 3, 0x40 | 3);
    return v;
}
static uae_u32 REGPARAM2 mbdmac_wget (uaecptr addr)
{
    uae_u32 v;
#ifdef JIT
    special_mem |= S_READ;
#endif
    v =  mbdmac_read (addr, 0x40 | 0) << 8;
    v |= mbdmac_read (addr + 1, 0x40 | 1) << 0;
    return v;
}
static uae_u32 REGPARAM2 mbdmac_bget (uaecptr addr)
{
#ifdef JIT
    special_mem |= S_READ;
#endif
    return mbdmac_read (addr, 0x10);
}
static void REGPARAM2 mbdmac_lput (uaecptr addr, uae_u32 l)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    mbdmac_write (addr + 0, l >> 24, 0x40 | 0);
    mbdmac_write (addr + 1, l >> 16, 0x40 | 1);
    mbdmac_write (addr + 2, l >> 8, 0x40 | 2);
    mbdmac_write (addr + 3, l, 0x40 | 3);
}
static void REGPARAM2 mbdmac_wput (uaecptr addr, uae_u32 w)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    mbdmac_write (addr + 0, w >> 8, 0x20 | 0);
    mbdmac_write (addr + 1, w >> 0, 0x20 | 1);
}
static void REGPARAM2 mbdmac_bput (uaecptr addr, uae_u32 b)
{
#ifdef JIT
    special_mem |= S_WRITE;
#endif
    mbdmac_write (addr, b, 0x10 | 0);
}

addrbank mbdmac_a3000_bank = {
    mbdmac_lget, mbdmac_wget, mbdmac_bget,
    mbdmac_lput, mbdmac_wput, mbdmac_bput,
    default_xlate, default_check, NULL, "A3000 DMAC",
    dummy_lgeti, dummy_wgeti, ABFLAG_IO
};

static void ew (int addr, uae_u32 value)
{
    addr &= 0xffff;
    if (addr == 00 || addr == 02 || addr == 0x40 || addr == 0x42) {
	dmacmemory[addr] = (value & 0xf0);
	dmacmemory[addr + 2] = (value & 0x0f) << 4;
    } else {
	dmacmemory[addr] = ~(value & 0xf0);
	dmacmemory[addr + 2] = ~((value & 0x0f) << 4);
    }
}

static void freescsi(struct scsi_data *sd)
{
    hdf_hd_close(sd->hfd);
    scsi_free(sd);
}

int addscsi(int ch, char *path, int blocksize, int readonly,
		       char *devname, int sectors, int surfaces, int reserved,
		       int bootpri, char *filesys)
{
    struct hd_hardfiledata *hfd;
    freescsi(scsis[ch]);
    scsis[ch] = NULL;
    hfd = xcalloc(sizeof(struct hd_hardfiledata), 1);
    if (!hdf_hd_open(hfd, path, blocksize, readonly, devname, sectors, surfaces, reserved, bootpri, filesys))
	return 0;
    hfd->ansi_version = 2;
    scsis[ch] = scsi_alloc(ch, hfd);
    return scsis[ch] ? 1 : 0;
}

static void addnativescsi(void)
{
    int i, j;
    int devices[MAX_TOTAL_DEVICES];
    int types[MAX_TOTAL_DEVICES];
    struct device_info dis[MAX_TOTAL_DEVICES];

    i = 0;
    while (i < MAX_TOTAL_DEVICES) {
	types[i] = -1;
	devices[i] = -1;
	if (sys_command_open (DF_SCSI, i)) {
	    if (sys_command_info (DF_SCSI, i, &dis[i])) {
		devices[i] = i;
		types[i] = 100 - i;
		if (dis[i].type == INQ_ROMD)
		    types[i] = 1000 - i;
	    }
	    sys_command_close (DF_SCSI, i);
	}
	i++;
    }
    i = 0;
    while (devices[i] >= 0) {
	j = i + 1;
	while (devices[j] >= 0) {
	    if (types[i] > types[j]) {
		int tmp = types[i];
		types[i] = types[j];
		types[j] = tmp;
	    }
	    j++;
	}
	i++;
    }
    i = 0; j = 0;
    while (devices[i] >= 0 && j < 7) {
	if (scsis[j] == NULL) {
	    scsis[j] = scsi_alloc_native(j, devices[i]);
	    write_log("SCSI: %d:'%s'\n", j, dis[i].label);
	    i++;
	}
	j++;
    }
}

int a3000_add_scsi_unit(int ch, char *path, int blocksize, int readonly,
		       char *devname, int sectors, int surfaces, int reserved,
		       int bootpri, char *filesys)
{
    return addscsi(ch, path, blocksize, readonly, devname, sectors, surfaces, reserved, bootpri, filesys);
}

void a3000scsi_reset(void)
{
    map_banks (&mbdmac_a3000_bank, 0xDD, 1, 0);
}

void a3000scsi_free(void)
{
    int i;
    for (i = 0; i < 7; i++)
	freescsi(scsis[i]);
}

int a2091_add_scsi_unit(int ch, char *path, int blocksize, int readonly,
		       char *devname, int sectors, int surfaces, int reserved,
		       int bootpri, char *filesys)
{
    return addscsi(ch, path, blocksize, readonly, devname, sectors, surfaces, reserved, bootpri, filesys);
}


void a2091_free (void)
{
    int i;
    for (i = 0; i < 7; i++)
	freescsi(scsis[i]);
    xfree(rom);
    rom = NULL;
}

void a2091_reset (void)
{
    configured = 0;
    wd_used = 0;
    superdmac = 0;
    superdmac = currprefs.cs_mbdmac ? 1 : 0;
    if (currprefs.scsi == 2)
	addnativescsi();
}

void a2091_init (void)
{
    struct zfile *z;
    int roms[4];
    struct romlist *rl;

    configured = 0;
    memset (dmacmemory, 0xff, 100);
    ew (0x00, 0xc0 | 0x01 | 0x10);
    /* A590/A2091 hardware id */
    ew (0x04, 0x03);
    /* commodore's manufacturer id */
    ew (0x10, 0x02);
    ew (0x14, 0x02);
    /* rom vector */
    ew (0x28, ROM_VECTOR >> 8);
    ew (0x2c, ROM_VECTOR);

    roms[0] = 55;
    roms[1] = 54;
    roms[2] = 53;
    roms[3] = -1;

    rl = getromlistbyids(roms);
    if (rl) {
	write_log("A590/A2091 BOOT ROM '%s' %d.%d ", rl->path, rl->rd->ver, rl->rd->rev);
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
    map_banks (&dmaca2091_bank, 0xe80000 >> 16, 0x10000 >> 16, 0x10000);
}