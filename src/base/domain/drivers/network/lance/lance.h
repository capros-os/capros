/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#ifndef __lance_h__
#define __lance_h__

#define LANCEADDR(base, off, size)    ((base) + 0x10 + (off) * (size))
#define BIT(x)                        (1 << (x))

/* I/O offsets from 0x10 for lance bus registers, in word units */
enum {
  RDP  = 0x0,      /* Register Data Port - CSRx access through RAP/RDP */
  RAP  = 0x1,      /* Register Address Port */
  RESET= 0x2,      /* Reset */
  BDP  = 0x3,      /* BCR Data Port - BCRx access through RAP/BDP */
};

/* CSR0 bits */
enum {
    INIT = BIT(0),
    STRT = BIT(1),
    STOP = BIT(2),
    TDMD = BIT(3),
    TXON = BIT(4),
    RXON = BIT(5),
    IENA = BIT(6),
    INTR = BIT(7),
    IDON = BIT(8),
    TINT = BIT(9),
    RINT = BIT(10),
    MERR = BIT(11),
    MISS = BIT(12),
    CERR = BIT(13),
    BABL = BIT(14),
    ERR  = BIT(15)
};

/* Ring Descriptor */
struct RingDescr{
  void* addr;         /* Buffer Address*/
  uint16_t  flags1;   /* flags & the BCNT field */
  uint16_t  flags2;   /* RCC & RPC fields */
  char *buf;          /* steal RESVD field to store virtual address */
};


/* CSRx registers */
enum {
  CSR0 = 0,           /* Control & Status Register */
  IblockAddr = 1,     /* Init Block address */
  Imask = 3,          /* Interrupt Masks and Deferral */
  Features = 4,       /* Test and features control */
  SoftStyle = 58,     /* Software Style, alias for BCR20 */
  ChipId = 88,        /* Chip Id */
};


struct InitBlock {
  uint16_t mode;                // mode as in CSR15
  uint8_t rlen;                 // note: upper four bits
  uint8_t tlen;                 // note: upper four bits
  uint8_t padr[6];              // ethernet address
  uint8_t pad1[2];
  uint8_t laddr[8];             // logical address filter
  uint32_t rdra;                  // xmit/rcv ring descrptors - 16-byte aligned
  uint32_t tdra;
};

// ring flags1
enum {
    Own = BIT(31),
    RingErr = BIT(30),
    FrameErr = BIT(29),
    AddFcs = BIT(29),           // set fcs when writing
    OflowErr = BIT(28),
    CrcErr = BIT(27),
    BufErr = BIT(26),
    Start = BIT(25),
    End = BIT(24),
};

#endif /*__lance_h__*/
