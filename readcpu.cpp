/*
* UAE - The Un*x Amiga Emulator
*
* Read 68000 CPU specs from file "table68k"
*
* Copyright 1995,1996 Bernd Schmidt
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include <ctype.h>

#include "readcpu.h"

int nr_cpuop_funcs;

struct mnemolookup lookuptab[] = {
	{ i_ILLG, L"ILLEGAL" },
	{ i_OR, L"OR" },
	{ i_CHK, L"CHK" },
	{ i_CHK2, L"CHK2" },
	{ i_AND, L"AND" },
	{ i_EOR, L"EOR" },
	{ i_ORSR, L"ORSR" },
	{ i_ANDSR, L"ANDSR" },
	{ i_EORSR, L"EORSR" },
	{ i_SUB, L"SUB" },
	{ i_SUBA, L"SUBA" },
	{ i_SUBX, L"SUBX" },
	{ i_SBCD, L"SBCD" },
	{ i_ADD, L"ADD" },
	{ i_ADDA, L"ADDA" },
	{ i_ADDX, L"ADDX" },
	{ i_ABCD, L"ABCD" },
	{ i_NEG, L"NEG" },
	{ i_NEGX, L"NEGX" },
	{ i_NBCD, L"NBCD" },
	{ i_CLR, L"CLR" },
	{ i_NOT, L"NOT" },
	{ i_TST, L"TST" },
	{ i_BTST, L"BTST" },
	{ i_BCHG, L"BCHG" },
	{ i_BCLR, L"BCLR" },
	{ i_BSET, L"BSET" },
	{ i_CMP, L"CMP" },
	{ i_CMPM, L"CMPM" },
	{ i_CMPA, L"CMPA" },
	{ i_MVPRM, L"MVPRM" },
	{ i_MVPMR, L"MVPMR" },
	{ i_MOVE, L"MOVE" },
	{ i_MOVEA, L"MOVEA" },
	{ i_MVSR2, L"MVSR2" },
	{ i_MV2SR, L"MV2SR" },
	{ i_SWAP, L"SWAP" },
	{ i_EXG, L"EXG" },
	{ i_EXT, L"EXT" },
	{ i_MVMEL, L"MVMEL", L"MOVEM" },
	{ i_MVMLE, L"MVMLE", L"MOVEM" },
	{ i_TRAP, L"TRAP" },
	{ i_MVR2USP, L"MVR2USP" },
	{ i_MVUSP2R, L"MVUSP2R" },
	{ i_NOP, L"NOP" },
	{ i_RESET, L"RESET" },
	{ i_RTE, L"RTE" },
	{ i_RTD, L"RTD" },
	{ i_LINK, L"LINK" },
	{ i_UNLK, L"UNLK" },
	{ i_RTS, L"RTS" },
	{ i_STOP, L"STOP" },
	{ i_TRAPV, L"TRAPV" },
	{ i_RTR, L"RTR" },
	{ i_JSR, L"JSR" },
	{ i_JMP, L"JMP" },
	{ i_BSR, L"BSR" },
	{ i_Bcc, L"Bcc" },
	{ i_LEA, L"LEA" },
	{ i_PEA, L"PEA" },
	{ i_DBcc, L"DBcc" },
	{ i_Scc, L"Scc" },
	{ i_DIVU, L"DIVU" },
	{ i_DIVS, L"DIVS" },
	{ i_MULU, L"MULU" },
	{ i_MULS, L"MULS" },
	{ i_ASR, L"ASR" },
	{ i_ASL, L"ASL" },
	{ i_LSR, L"LSR" },
	{ i_LSL, L"LSL" },
	{ i_ROL, L"ROL" },
	{ i_ROR, L"ROR" },
	{ i_ROXL, L"ROXL" },
	{ i_ROXR, L"ROXR" },
	{ i_ASRW, L"ASRW" },
	{ i_ASLW, L"ASLW" },
	{ i_LSRW, L"LSRW" },
	{ i_LSLW, L"LSLW" },
	{ i_ROLW, L"ROLW" },
	{ i_RORW, L"RORW" },
	{ i_ROXLW, L"ROXLW" },
	{ i_ROXRW, L"ROXRW" },

	{ i_MOVE2C, L"MOVE2C", L"MOVEC" },
	{ i_MOVEC2, L"MOVEC2", L"MOVEC" },
	{ i_CAS, L"CAS" },
	{ i_CAS2, L"CAS2" },
	{ i_MULL, L"MULL" },
	{ i_DIVL, L"DIVL" },
	{ i_BFTST, L"BFTST" },
	{ i_BFEXTU, L"BFEXTU" },
	{ i_BFCHG, L"BFCHG" },
	{ i_BFEXTS, L"BFEXTS" },
	{ i_BFCLR, L"BFCLR" },
	{ i_BFFFO, L"BFFFO" },
	{ i_BFSET, L"BFSET" },
	{ i_BFINS, L"BFINS" },
	{ i_PACK, L"PACK" },
	{ i_UNPK, L"UNPK" },
	{ i_TAS, L"TAS" },
	{ i_BKPT, L"BKPT" },
	{ i_CALLM, L"CALLM" },
	{ i_RTM, L"RTM" },
	{ i_TRAPcc, L"TRAPcc" },
	{ i_MOVES, L"MOVES" },
	{ i_FPP, L"FPP" },
	{ i_FDBcc, L"FDBcc" },
	{ i_FScc, L"FScc" },
	{ i_FTRAPcc, L"FTRAPcc" },
	{ i_FBcc, L"FBcc" },
	{ i_FBcc, L"FBcc" },
	{ i_FSAVE, L"FSAVE" },
	{ i_FRESTORE, L"FRESTORE" },

	{ i_CINVL, L"CINVL" },
	{ i_CINVP, L"CINVP" },
	{ i_CINVA, L"CINVA" },
	{ i_CPUSHL, L"CPUSHL" },
	{ i_CPUSHP, L"CPUSHP" },
	{ i_CPUSHA, L"CPUSHA" },
	{ i_MOVE16, L"MOVE16" },

	{ i_MMUOP030, L"MMUOP030" },
	{ i_PFLUSHN, L"PFLUSHN" },
	{ i_PFLUSH, L"PFLUSH" },
	{ i_PFLUSHAN, L"PFLUSHAN" },
	{ i_PFLUSHA, L"PFLUSHA" },

	{ i_PLPAR, L"PLPAR" },
	{ i_PLPAW, L"PLPAW" },
	{ i_PTESTR, L"PTESTR" },
	{ i_PTESTW, L"PTESTW" },

	{ i_LPSTOP, L"LPSTOP" },
	{ i_ILLG, L"" },
};

struct instr *table68k;

static int specialcase (uae_u16 opcode, int cpu_lev)
{
	int mode = (opcode >> 3) & 7;
	int reg = opcode & 7;

	if (cpu_lev >= 2)
		return cpu_lev;
	/* TST.W A0, TST.L A0, TST.x (d16,PC) and TST.x (d8,PC,Xn) are 68020+ only */
	if ((opcode & 0xff00) == 0x4a00) {
		if (mode == 7 && (reg == 4 || reg == 2 || reg == 3))
			return 2;
		if (mode == 1) /* Ax */
			return 2;
	}
	/* CMPI.W #x,(d16,PC) and CMPI.W #x,(d8,PC,Xn) are 68020+ only */
	if ((opcode & 0xff00) == 0x0c00) {
		if (mode == 7 && (reg == 2 || reg == 3))
			return 2;
	}
	return cpu_lev;
}

static amodes mode_from_str (const TCHAR *str)
{
	if (_tcsncmp (str, L"Dreg", 4) == 0) return Dreg;
	if (_tcsncmp (str, L"Areg", 4) == 0) return Areg;
	if (_tcsncmp (str, L"Aind", 4) == 0) return Aind;
	if (_tcsncmp (str, L"Apdi", 4) == 0) return Apdi;
	if (_tcsncmp (str, L"Aipi", 4) == 0) return Aipi;
	if (_tcsncmp (str, L"Ad16", 4) == 0) return Ad16;
	if (_tcsncmp (str, L"Ad8r", 4) == 0) return Ad8r;
	if (_tcsncmp (str, L"absw", 4) == 0) return absw;
	if (_tcsncmp (str, L"absl", 4) == 0) return absl;
	if (_tcsncmp (str, L"PC16", 4) == 0) return PC16;
	if (_tcsncmp (str, L"PC8r", 4) == 0) return PC8r;
	if (_tcsncmp (str, L"Immd", 4) == 0) return imm;
	abort ();
	return 0;
}

STATIC_INLINE amodes mode_from_mr (int mode, int reg)
{
	switch (mode) {
	case 0: return Dreg;
	case 1: return Areg;
	case 2: return Aind;
	case 3: return Aipi;
	case 4: return Apdi;
	case 5: return Ad16;
	case 6: return Ad8r;
	case 7:
		switch (reg) {
		case 0: return absw;
		case 1: return absl;
		case 2: return PC16;
		case 3: return PC8r;
		case 4: return imm;
		case 5:
		case 6:
		case 7: return am_illg;
		}
	}
	abort ();
	return 0;
}

static void build_insn (int insn)
{
	int find = -1;
	int variants;
	int isjmp = 0;
	struct instr_def id;
	const TCHAR *opcstr;
	int i;

	int flaglive = 0, flagdead = 0;

	id = defs68k[insn];

	/* Note: We treat anything with unknown flags as a jump. That
	is overkill, but "the programmer" was lazy quite often, and
	*this* programmer can't be bothered to work out what can and
	can't trap. Usually, this will be overwritten with the gencomp
	based information, anyway. */

	for (i = 0; i < 5; i++) {
		switch (id.flaginfo[i].flagset){
		case fa_unset: break;
		case fa_isjmp: isjmp = 1; break;
		case fa_isbranch: isjmp = 1; break;
		case fa_zero: flagdead |= 1 << i; break;
		case fa_one: flagdead |= 1 << i; break;
		case fa_dontcare: flagdead |= 1 << i; break;
		case fa_unknown: isjmp = 1; flagdead = -1; goto out1;
		case fa_set: flagdead |= 1 << i; break;
		}
	}

out1:
	for (i = 0; i < 5; i++) {
		switch (id.flaginfo[i].flaguse) {
		case fu_unused: break;
		case fu_isjmp: isjmp = 1; flaglive |= 1 << i; break;
		case fu_maybecc: isjmp = 1; flaglive |= 1 << i; break;
		case fu_unknown: isjmp = 1; flaglive |= 1 << i; break;
		case fu_used: flaglive |= 1 << i; break;
		}
	}

	opcstr = id.opcstr;
	for (variants = 0; variants < (1 << id.n_variable); variants++) {
		int bitcnt[lastbit];
		int bitval[lastbit];
		int bitpos[lastbit];
		int i;
		uae_u16 opc = id.bits;
		uae_u16 msk, vmsk;
		int pos = 0;
		int mnp = 0;
		int bitno = 0;
		TCHAR mnemonic[10];

		wordsizes sz = sz_long;
		int srcgather = 0, dstgather = 0;
		int usesrc = 0, usedst = 0;
		int srctype = 0;
		int srcpos = -1, dstpos = -1;

		amodes srcmode = am_unknown, destmode = am_unknown;
		int srcreg = -1, destreg = -1;

		for (i = 0; i < lastbit; i++)
			bitcnt[i] = bitval[i] = 0;

		vmsk = 1 << id.n_variable;

		for (i = 0, msk = 0x8000; i < 16; i++, msk >>= 1) {
			if (!(msk & id.mask)) {
				int currbit = id.bitpos[bitno++];
				int bit_set;
				vmsk >>= 1;
				bit_set = variants & vmsk ? 1 : 0;
				if (bit_set)
					opc |= msk;
				bitpos[currbit] = 15 - i;
				bitcnt[currbit]++;
				bitval[currbit] <<= 1;
				bitval[currbit] |= bit_set;
			}
		}

		if (bitval[bitj] == 0) bitval[bitj] = 8;
		/* first check whether this one does not match after all */
		if (bitval[bitz] == 3 || bitval[bitC] == 1)
			continue;
		if (bitcnt[bitI] && (bitval[bitI] == 0x00 || bitval[bitI] == 0xff))
			continue;

		/* bitI and bitC get copied to biti and bitc */
		if (bitcnt[bitI]) {
			bitval[biti] = bitval[bitI]; bitpos[biti] = bitpos[bitI];
		}
		if (bitcnt[bitC])
			bitval[bitc] = bitval[bitC];

		pos = 0;
		while (opcstr[pos] && !_istspace(opcstr[pos])) {
			if (opcstr[pos] == '.') {
				pos++;
				switch (opcstr[pos]) {

				case 'B': sz = sz_byte; break;
				case 'W': sz = sz_word; break;
				case 'L': sz = sz_long; break;
				case 'z':
					switch (bitval[bitz]) {
					case 0: sz = sz_byte; break;
					case 1: sz = sz_word; break;
					case 2: sz = sz_long; break;
					default: abort();
					}
					break;
				default: abort();
				}
			} else {
				mnemonic[mnp] = opcstr[pos];
				if (mnemonic[mnp] == 'f') {
					find = -1;
					switch (bitval[bitf]) {
					case 0: mnemonic[mnp] = 'R'; break;
					case 1: mnemonic[mnp] = 'L'; break;
					default: abort();
					}
				}
				mnp++;
			}
			pos++;
		}
		mnemonic[mnp] = 0;

		/* now, we have read the mnemonic and the size */
		while (opcstr[pos] && _istspace(opcstr[pos]))
			pos++;

		/* A goto a day keeps the D******a away. */
		if (opcstr[pos] == 0)
			goto endofline;

		/* parse the source address */
		usesrc = 1;
		switch (opcstr[pos++]) {
		case 'D':
			srcmode = Dreg;
			switch (opcstr[pos++]) {
			case 'r': srcreg = bitval[bitr]; srcgather = 1; srcpos = bitpos[bitr]; break;
			case 'R': srcreg = bitval[bitR]; srcgather = 1; srcpos = bitpos[bitR]; break;
			default: abort();
			}
			break;
		case 'A':
			srcmode = Areg;
			switch (opcstr[pos++]) {
			case 'r': srcreg = bitval[bitr]; srcgather = 1; srcpos = bitpos[bitr]; break;
			case 'R': srcreg = bitval[bitR]; srcgather = 1; srcpos = bitpos[bitR]; break;
			default: abort();
			}
			switch (opcstr[pos]) {
			case 'p': srcmode = Apdi; pos++; break;
			case 'P': srcmode = Aipi; pos++; break;
			case 'a': srcmode = Aind; pos++; break;
			}
			break;
		case 'L':
			srcmode = absl;
			break;
		case '#':
			switch (opcstr[pos++]) {
			case 'z': srcmode = imm; break;
			case '0': srcmode = imm0; break;
			case '1': srcmode = imm1; break;
			case '2': srcmode = imm2; break;
			case 'i': srcmode = immi; srcreg = (uae_s32)(uae_s8)bitval[biti];
				if (CPU_EMU_SIZE < 4) {
					/* Used for branch instructions */
					srctype = 1;
					srcgather = 1;
					srcpos = bitpos[biti];
				}
				break;
			case 'j': srcmode = immi; srcreg = bitval[bitj];
				if (CPU_EMU_SIZE < 3) {
					/* 1..8 for ADDQ/SUBQ and rotshi insns */
					srcgather = 1;
					srctype = 3;
					srcpos = bitpos[bitj];
				}
				break;
			case 'J': srcmode = immi; srcreg = bitval[bitJ];
				if (CPU_EMU_SIZE < 5) {
					/* 0..15 */
					srcgather = 1;
					srctype = 2;
					srcpos = bitpos[bitJ];
				}
				break;
			case 'k': srcmode = immi; srcreg = bitval[bitk];
				if (CPU_EMU_SIZE < 3) {
					srcgather = 1;
					srctype = 4;
					srcpos = bitpos[bitk];
				}
				break;
			case 'K': srcmode = immi; srcreg = bitval[bitK];
				if (CPU_EMU_SIZE < 5) {
					/* 0..15 */
					srcgather = 1;
					srctype = 5;
					srcpos = bitpos[bitK];
				}
				break;
			case 'p': srcmode = immi; srcreg = bitval[bitK];
				if (CPU_EMU_SIZE < 5) {
					/* 0..3 */
					srcgather = 1;
					srctype = 7;
					srcpos = bitpos[bitp];
				}
				break;
			default: abort();
			}
			break;
		case 'd':
			srcreg = bitval[bitD];
			srcmode = mode_from_mr(bitval[bitd],bitval[bitD]);
			if (srcmode == am_illg)
				continue;
			if (CPU_EMU_SIZE < 2 &&
				(srcmode == Areg || srcmode == Dreg || srcmode == Aind
				|| srcmode == Ad16 || srcmode == Ad8r || srcmode == Aipi
				|| srcmode == Apdi))
			{
				srcgather = 1; srcpos = bitpos[bitD];
			}
			if (opcstr[pos] == '[') {
				pos++;
				if (opcstr[pos] == '!') {
					/* exclusion */
					do {
						pos++;
						if (mode_from_str(opcstr+pos) == srcmode)
							goto nomatch;
						pos += 4;
					} while (opcstr[pos] == ',');
					pos++;
				} else {
					if (opcstr[pos+4] == '-') {
						/* replacement */
						if (mode_from_str(opcstr+pos) == srcmode)
							srcmode = mode_from_str(opcstr+pos+5);
						else
							goto nomatch;
						pos += 10;
					} else {
						/* normal */
						while(mode_from_str(opcstr+pos) != srcmode) {
							pos += 4;
							if (opcstr[pos] == ']')
								goto nomatch;
							pos++;
						}
						while(opcstr[pos] != ']') pos++;
						pos++;
						break;
					}
				}
			}
			/* Some addressing modes are invalid as destination */
			if (srcmode == imm || srcmode == PC16 || srcmode == PC8r)
				goto nomatch;
			break;
		case 's':
			srcreg = bitval[bitS];
			srcmode = mode_from_mr(bitval[bits],bitval[bitS]);

			if (srcmode == am_illg)
				continue;
			if (CPU_EMU_SIZE < 2 &&
				(srcmode == Areg || srcmode == Dreg || srcmode == Aind
				|| srcmode == Ad16 || srcmode == Ad8r || srcmode == Aipi
				|| srcmode == Apdi))
			{
				srcgather = 1; srcpos = bitpos[bitS];
			}
			if (opcstr[pos] == '[') {
				pos++;
				if (opcstr[pos] == '!') {
					/* exclusion */
					do {
						pos++;
						if (mode_from_str(opcstr+pos) == srcmode)
							goto nomatch;
						pos += 4;
					} while (opcstr[pos] == ',');
					pos++;
				} else {
					if (opcstr[pos+4] == '-') {
						/* replacement */
						if (mode_from_str(opcstr+pos) == srcmode)
							srcmode = mode_from_str(opcstr+pos+5);
						else
							goto nomatch;
						pos += 10;
					} else {
						/* normal */
						while(mode_from_str(opcstr+pos) != srcmode) {
							pos += 4;
							if (opcstr[pos] == ']')
								goto nomatch;
							pos++;
						}
						while(opcstr[pos] != ']') pos++;
						pos++;
					}
				}
			}
			break;
		default: abort();
		}
		/* safety check - might have changed */
		if (srcmode != Areg && srcmode != Dreg && srcmode != Aind
			&& srcmode != Ad16 && srcmode != Ad8r && srcmode != Aipi
			&& srcmode != Apdi && srcmode != immi)
		{
			srcgather = 0;
		}
		if (srcmode == Areg && sz == sz_byte)
			goto nomatch;

		if (opcstr[pos] != ',')
			goto endofline;
		pos++;

		/* parse the destination address */
		usedst = 1;
		switch (opcstr[pos++]) {
		case 'D':
			destmode = Dreg;
			switch (opcstr[pos++]) {
			case 'r': destreg = bitval[bitr]; dstgather = 1; dstpos = bitpos[bitr]; break;
			case 'R': destreg = bitval[bitR]; dstgather = 1; dstpos = bitpos[bitR]; break;
			default: abort();
			}
			if (dstpos < 0 || dstpos >= 32)
				abort ();
			break;
		case 'A':
			destmode = Areg;
			switch (opcstr[pos++]) {
			case 'r': destreg = bitval[bitr]; dstgather = 1; dstpos = bitpos[bitr]; break;
			case 'R': destreg = bitval[bitR]; dstgather = 1; dstpos = bitpos[bitR]; break;
			case 'x': destreg = 0; dstgather = 0; dstpos = 0; break;
			default: abort();
			}
			if (dstpos < 0 || dstpos >= 32)
				abort ();
			switch (opcstr[pos]) {
			case 'p': destmode = Apdi; pos++; break;
			case 'P': destmode = Aipi; pos++; break;
			}
			break;
		case 'L':
			destmode = absl;
			break;
		case '#':
			switch (opcstr[pos++]) {
			case 'z': destmode = imm; break;
			case '0': destmode = imm0; break;
			case '1': destmode = imm1; break;
			case '2': destmode = imm2; break;
			case 'i': destmode = immi; destreg = (uae_s32)(uae_s8)bitval[biti]; break;
			case 'j': destmode = immi; destreg = bitval[bitj]; break;
			case 'J': destmode = immi; destreg = bitval[bitJ]; break;
			case 'k': destmode = immi; destreg = bitval[bitk]; break;
			case 'K': destmode = immi; destreg = bitval[bitK]; break;
			default: abort();
			}
			break;
		case 'd':
			destreg = bitval[bitD];
			destmode = mode_from_mr(bitval[bitd],bitval[bitD]);
			if (destmode == am_illg)
				continue;
			if (CPU_EMU_SIZE < 1 &&
				(destmode == Areg || destmode == Dreg || destmode == Aind
				|| destmode == Ad16 || destmode == Ad8r || destmode == Aipi
				|| destmode == Apdi))
			{
				dstgather = 1; dstpos = bitpos[bitD];
			}

			if (opcstr[pos] == '[') {
				pos++;
				if (opcstr[pos] == '!') {
					/* exclusion */
					do {
						pos++;
						if (mode_from_str(opcstr+pos) == destmode)
							goto nomatch;
						pos += 4;
					} while (opcstr[pos] == ',');
					pos++;
				} else {
					if (opcstr[pos+4] == '-') {
						/* replacement */
						if (mode_from_str(opcstr+pos) == destmode)
							destmode = mode_from_str(opcstr+pos+5);
						else
							goto nomatch;
						pos += 10;
					} else {
						/* normal */
						while(mode_from_str(opcstr+pos) != destmode) {
							pos += 4;
							if (opcstr[pos] == ']')
								goto nomatch;
							pos++;
						}
						while(opcstr[pos] != ']') pos++;
						pos++;
						break;
					}
				}
			}
			/* Some addressing modes are invalid as destination */
			if (destmode == imm || destmode == PC16 || destmode == PC8r)
				goto nomatch;
			break;
		case 's':
			destreg = bitval[bitS];
			destmode = mode_from_mr(bitval[bits],bitval[bitS]);

			if (destmode == am_illg)
				continue;
			if (CPU_EMU_SIZE < 1 &&
				(destmode == Areg || destmode == Dreg || destmode == Aind
				|| destmode == Ad16 || destmode == Ad8r || destmode == Aipi
				|| destmode == Apdi))
			{
				dstgather = 1; dstpos = bitpos[bitS];
			}

			if (opcstr[pos] == '[') {
				pos++;
				if (opcstr[pos] == '!') {
					/* exclusion */
					do {
						pos++;
						if (mode_from_str(opcstr+pos) == destmode)
							goto nomatch;
						pos += 4;
					} while (opcstr[pos] == ',');
					pos++;
				} else {
					if (opcstr[pos+4] == '-') {
						/* replacement */
						if (mode_from_str(opcstr+pos) == destmode)
							destmode = mode_from_str(opcstr+pos+5);
						else
							goto nomatch;
						pos += 10;
					} else {
						/* normal */
						while(mode_from_str(opcstr+pos) != destmode) {
							pos += 4;
							if (opcstr[pos] == ']')
								goto nomatch;
							pos++;
						}
						while(opcstr[pos] != ']') pos++;
						pos++;
					}
				}
			}
			break;
		default: abort();
		}
		/* safety check - might have changed */
		if (destmode != Areg && destmode != Dreg && destmode != Aind
			&& destmode != Ad16 && destmode != Ad8r && destmode != Aipi
			&& destmode != Apdi)
		{
			dstgather = 0;
		}

		if (destmode == Areg && sz == sz_byte)
			goto nomatch;
#if 0
		if (sz == sz_byte && (destmode == Aipi || destmode == Apdi)) {
			dstgather = 0;
		}
#endif
endofline:
		/* now, we have a match */
		if (table68k[opc].mnemo != i_ILLG)
			;//write_log (L"Double match: %x: %s\n", opc, opcstr);
		if (find == -1) {
			for (find = 0;; find++) {
				if (_tcscmp (mnemonic, lookuptab[find].name) == 0) {
					table68k[opc].mnemo = lookuptab[find].mnemo;
					break;
				}
				if (_tcslen (lookuptab[find].name) == 0)
					abort();
			}
		}
		else {
			table68k[opc].mnemo = lookuptab[find].mnemo;
		}
		table68k[opc].cc = bitval[bitc];
		if (table68k[opc].mnemo == i_BTST
			|| table68k[opc].mnemo == i_BSET
			|| table68k[opc].mnemo == i_BCLR
			|| table68k[opc].mnemo == i_BCHG)
		{
			sz = destmode == Dreg ? sz_long : sz_byte;
		}
		table68k[opc].size = sz;
		table68k[opc].sreg = srcreg;
		table68k[opc].dreg = destreg;
		table68k[opc].smode = srcmode;
		table68k[opc].dmode = destmode;
		table68k[opc].spos = srcgather ? srcpos : -1;
		table68k[opc].dpos = dstgather ? dstpos : -1;
		table68k[opc].suse = usesrc;
		table68k[opc].duse = usedst;
		table68k[opc].stype = srctype;
		table68k[opc].plev = id.plevel;
		table68k[opc].clev = specialcase(opc, id.cpulevel);

#if 0
		for (i = 0; i < 5; i++) {
			table68k[opc].flaginfo[i].flagset = id.flaginfo[i].flagset;
			table68k[opc].flaginfo[i].flaguse = id.flaginfo[i].flaguse;
		}
#endif
		table68k[opc].flagdead = flagdead;
		table68k[opc].flaglive = flaglive;
		table68k[opc].isjmp = isjmp;
nomatch:
		/* FOO! */;
	}
}


void read_table68k (void)
{
	int i;

	free (table68k);
	table68k = xmalloc (struct instr, 65536);
	for (i = 0; i < 65536; i++) {
		table68k[i].mnemo = i_ILLG;
		table68k[i].handler = -1;
	}
	for (i = 0; i < n_defs68k; i++) {
		build_insn (i);
	}
}

static int imismatch;

static void handle_merges (long int opcode)
{
	uae_u16 smsk;
	uae_u16 dmsk;
	int sbitdst, dstend;
	int srcreg, dstreg;

	if (table68k[opcode].spos == -1) {
		sbitdst = 1; smsk = 0;
	} else {
		switch (table68k[opcode].stype) {
		case 0:
			smsk = 7; sbitdst = 8; break;
		case 1:
			smsk = 255; sbitdst = 256; break;
		case 2:
			smsk = 15; sbitdst = 16; break;
		case 3:
			smsk = 7; sbitdst = 8; break;
		case 4:
			smsk = 7; sbitdst = 8; break;
		case 5:
			smsk = 63; sbitdst = 64; break;
		case 7:
			smsk = 3; sbitdst = 4; break;
		default:
			smsk = 0; sbitdst = 0;
			abort();
			break;
		}
		smsk <<= table68k[opcode].spos;
	}
	if (table68k[opcode].dpos == -1) {
		dstend = 1; dmsk = 0;
	} else {
		dmsk = 7 << table68k[opcode].dpos;
		dstend = 8;
	}
	for (srcreg=0; srcreg < sbitdst; srcreg++) {
		for (dstreg=0; dstreg < dstend; dstreg++) {
			uae_u16 code = (uae_u16)opcode;

			code = (code & ~smsk) | (srcreg << table68k[opcode].spos);
			code = (code & ~dmsk) | (dstreg << table68k[opcode].dpos);

			/* Check whether this is in fact the same instruction.
			* The instructions should never differ, except for the
			* Bcc.(BW) case. */
			if (table68k[code].mnemo != table68k[opcode].mnemo
				|| table68k[code].size != table68k[opcode].size
				|| table68k[code].suse != table68k[opcode].suse
				|| table68k[code].duse != table68k[opcode].duse)
			{
				imismatch++; continue;
			}
			if (table68k[opcode].suse
				&& (table68k[opcode].spos != table68k[code].spos
				|| table68k[opcode].smode != table68k[code].smode
				|| table68k[opcode].stype != table68k[code].stype))
			{
				imismatch++; continue;
			}
			if (table68k[opcode].duse
				&& (table68k[opcode].dpos != table68k[code].dpos
				|| table68k[opcode].dmode != table68k[code].dmode))
			{
				imismatch++; continue;
			}

			if (code != opcode)
				table68k[code].handler = opcode;
		}
	}
}

void do_merges (void)
{
	long int opcode;
	int nr = 0;
	imismatch = 0;
	for (opcode = 0; opcode < 65536; opcode++) {
		if (table68k[opcode].handler != -1 || table68k[opcode].mnemo == i_ILLG)
			continue;
		nr++;
		handle_merges (opcode);
	}
	nr_cpuop_funcs = nr;
}

int get_no_mismatches (void)
{
	return imismatch;
}
