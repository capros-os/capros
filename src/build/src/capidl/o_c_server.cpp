/*
 * Copyright (C) 2003, The EROS Group, LLC.
 * Copyright (C) 2009, Strawberry Development Group.
 * Copyright (C) 2022, Charles Landau.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/xmalloc.h>
#include <applib/Intern.h>
#include <applib/Diag.h>
#include <applib/PtrVec.h>
#include <applib/path.h>
#include <applib/buffer.h>
#include "SymTab.h"
#include "util.h"
#include "backend.h"
#include "o_c_util.h"

#define max(a,b) ((a > b) ? (a) : (b))

static Buffer *preamble;


static unsigned
compute_direct_bytes(PtrVec *symVec)
{
  unsigned i;
  unsigned sz = 0;

  for(i = 0; i < vec_len(symVec); i++) {
    Symbol *sym = vec_fetch(symVec, i);
    sz = round_up(sz, symbol_alignof(sym));
    sz += symbol_directSize(sym);
  }

  return sz;
}

static unsigned
compute_indirect_bytes(PtrVec *symVec)
{
  unsigned i;
  unsigned sz = 0;

  for(i = 0; i < vec_len(symVec); i++) {
    Symbol *sym = vec_fetch(symVec, i);
    sz = round_up(sz, symbol_alignof(sym));
    sz += symbol_indirectSize(sym);
  }

  return sz;
}

static void
emit_pass_from_reg(FILE *outFile, Symbol *arg, unsigned regCount)
{
  Symbol *argType = symbol_ResolveRef(arg->type);
  Symbol *argBaseType = symbol_ResolveType(argType);
  unsigned bits = mpz_get_ui(argBaseType->v.i);
  unsigned bitsInput = 0;

  /* If the quantity is larger than a single register, we need to
     decode the rest of it: */

  while (bitsInput < bits) {
    if (bitsInput)
      fprintf(outFile, " | (");

    fprintf(outFile, "((");
    output_c_type(argType, outFile, 0);
    fprintf(outFile, ") msg.rcv_w%d)", regCount);

    if (bitsInput)
      fprintf(outFile, "<< %d)", bitsInput);

    bitsInput += REGISTER_BITS;
    regCount++;
  }
}

static void
emit_return_via_reg(FILE *outFile, Symbol *arg, int indent, unsigned regCount)
{
  Symbol *argType = symbol_ResolveRef(arg->type);
  Symbol *argBaseType = symbol_ResolveType(argType);
  unsigned bits = mpz_get_ui(argBaseType->v.i);
  unsigned bitsOutput = 0;

  /* If the quantity is larger than a single register, we need to
     decode the rest of it: */

  while (bitsOutput < bits) {
    do_indent(outFile, indent);
    fprintf(outFile, "msg->snd_w%d = ", regCount);

    if (bitsOutput)
      fprintf(outFile, " (");

    fprintf(outFile, "%s", arg->name);

    if (bitsOutput)
      fprintf(outFile, ">> %d)", bitsOutput);

    fprintf(outFile, ";\n", bitsOutput);

    bitsOutput += REGISTER_BITS;
    regCount++;
  }
}

static void
emit_op_dispatcher(Symbol *s, FILE *outFile)
{
  unsigned i;

  unsigned snd_regcount = FIRST_REG;
  unsigned needRegs = 0;	// This is probably wrong;
	// just initialize it to prevent a compiler warning.

  PtrVec *rcvRegs = extract_registerizable_arguments(s, sc_formal);
  PtrVec *sndRegs = extract_registerizable_arguments(s, sc_outformal);
  PtrVec *rcvString = extract_string_arguments(s, sc_formal);
  PtrVec *sndString = extract_string_arguments(s, sc_outformal);

  unsigned sndOffset = 0;
  unsigned rcvOffset = 0;
  unsigned rcvDirect = compute_direct_bytes(rcvString);
  unsigned sndDirect = compute_direct_bytes(sndString);

  /* Computing indirect bytes on rcvString/sndString vector will
     generate the wrong number, but a non-zero number tells us that
     there ARE some indirect bytes to consider. */
  unsigned rcvIndirect = compute_indirect_bytes(rcvString);
  unsigned sndIndirect = compute_indirect_bytes(sndString);

  rcvDirect = round_up(rcvDirect, 8);
  sndDirect = round_up(sndDirect, 8);

  fprintf(outFile, "\n");
  fprintf(outFile, "static void\n");
  fprintf(outFile, "DISPATCH_OP_%s(Message *msg, IfInfo *info)\n", 
	  symbol_QualifiedName(s, '_'));
  fprintf(outFile, "{\n");

  if (rcvIndirect || sndIndirect) {
    if (rcvIndirect) {
      do_indent(outFile, 2);
      fprintf(outFile, "unsigned rcvIndir = %u;\n", rcvDirect);
    }
    if (sndIndirect) {
      do_indent(outFile, 2);
      fprintf(outFile, "unsigned sndIndir = %u;\n", sndDirect);
    }

    fprintf(outFile, "\n");
  }

  do_indent(outFile, 2);
  fprintf(outFile, "/* Emit OP %s */\n", symbol_QualifiedName(s, '_'));

  /* Pass 1: emit the declarations for the direct variables */
  if (vec_len(rcvRegs) || vec_len(rcvString)) {
    do_indent(outFile, 2);
    fprintf(outFile, "/* Incoming arguments */\n");

    if (vec_len(rcvRegs)) {
      unsigned rcv_regcount = FIRST_REG;

      for (i = 0; i < vec_len(rcvRegs); i++) {
	Symbol *arg = symvec_fetch(rcvRegs, i);
	Symbol *argType = symbol_ResolveRef(arg->type);

	do_indent(outFile, 2);
	output_c_type(argType, outFile, 0);
	fprintf(outFile, " %s = ", arg->name);

	emit_pass_from_reg(outFile, arg, rcv_regcount);
	fprintf(outFile, ";\n");
	rcv_regcount += needRegs;
      }
    }

    if (vec_len(rcvString)) {
      for (i = 0; i < vec_len(rcvString); i++) {
	Symbol *arg = symvec_fetch(rcvString, i);
	Symbol *argType = symbol_ResolveRef(arg->type);
	Symbol *argBaseType = symbol_ResolveType(argType);

	rcvOffset = round_up(rcvOffset, symbol_alignof(argBaseType));

	do_indent(outFile, 2);
	output_c_type(argType, outFile, 0);
	fprintf(outFile, " *%s = (", arg->name);
	output_c_type(argType, outFile, 0);
	fprintf(outFile, " *) (msg->rcv_data + %d);\n", rcvOffset);

	rcvOffset += symbol_directSize(argBaseType);
      }
    }

    fprintf(outFile, "\n");
  }

  if (vec_len(sndString) || vec_len(sndRegs)) {
    do_indent(outFile, 2);
    fprintf(outFile, "/* Outgoing arguments */\n");

    /* For the outbound registerizables, we need to declare variables so
       that we can pass pointers of the expected type. Casting the word
       fields as in "(char *) &msg.rcv_w0" could create problems on
       depending on byte sex */
    if (vec_len(sndRegs)) {
      for (i = 0; i < vec_len(sndRegs); i++) {
	Symbol *arg = symvec_fetch(sndRegs, i);
	Symbol *argType = symbol_ResolveRef(arg->type);
	Symbol *argBaseType = symbol_ResolveType(argType);

	do_indent(outFile, 2);
	output_c_type(argType, outFile, 0);
	fprintf(outFile, " %s;\n", arg->name);

	sndOffset += symbol_directSize(argBaseType);
      }
    }

    if (vec_len(sndString)) {
      for (i = 0; i < vec_len(sndString); i++) {
	Symbol *arg = symvec_fetch(sndString, i);
	Symbol *argType = symbol_ResolveRef(arg->type);
	Symbol *argBaseType = symbol_ResolveType(argType);

	sndOffset = round_up(sndOffset, symbol_alignof(argBaseType));

	do_indent(outFile, 2);
	output_c_type(argType, outFile, 0);
	fprintf(outFile, " *%s = (", arg->name);
	output_c_type(argType, outFile, 0);
	fprintf(outFile, " *) (msg->snd_data + %d);\n", sndOffset);

	sndOffset += symbol_directSize(argBaseType);
      }
    }

    fprintf(outFile, "\n");
  }

  /* Pass 2: patch the indirect arg data pointers */
  for (i = 0; i < vec_len(rcvString); i++) {
    Symbol *arg = symvec_fetch(rcvString, i);
    Symbol *argType = symbol_ResolveRef(arg->type);
    Symbol *argBaseType = symbol_ResolveType(argType);

    if (symbol_IsVarSequenceType(argBaseType)) {
      do_indent(outFile, 2);
      fprintf(outFile, "%s->data = (", arg->name);

      output_c_type(argBaseType->type, outFile, 0);

      fprintf(outFile, " *) (msg->rcv_data + rcvIndir)\n");

      do_indent(outFile, 2);
      fprintf(outFile, "rcvIndir += (%s->len * sizeof(*%s->data));\n",
	      arg->name, arg->name);
    }
  }

  /* Pass 3: make the actual call */
  do_indent(outFile, 2);
  fprintf(outFile, "msg->snd_code = implement_%s(", symbol_QualifiedName(s, '_'));

  {
    unsigned rcv_regcount = FIRST_REG;

    for (const auto eachChild : s->children) {
      Symbol *argType = symbol_ResolveRef(eachChild->type);
      Symbol *argBaseType = symbol_ResolveType(argType);

      if (i > 0)
	fprintf(outFile, ", ");
      
      if (eachChild->cls == sc_formal) {
	if ((needRegs = can_registerize(argBaseType, rcv_regcount))) {
	  fprintf(outFile, "%s", eachChild->name);
	}
	else {
	  fprintf(outFile, "*%s", eachChild->name);
	}
      }
      else {
	if ((needRegs = can_registerize(argBaseType, snd_regcount))) {
	  fprintf(outFile, "/* OUT */ &%s", eachChild->name);
	  snd_regcount += needRegs;
	}
	else {
	  fprintf(outFile, "/* OUT */ %s", eachChild->name);
	}
      }
    }
  }
  fprintf(outFile, ");\n");

  do_indent(outFile, 2);
  fprintf(outFile, "msg->snd_len = 0; /* Until otherwise proven */\n");

  fprintf(outFile, "\n");

  /* Pass 4: pack the outgoing return string */
  if (vec_len(sndRegs) || vec_len(sndString)) {
    unsigned snd_regcount = FIRST_REG;
    unsigned curAlign = 0xfu;

    do_indent(outFile, 2);
    fprintf(outFile, "if (msg->snd_code == RC_OK) {\n");

    for (const auto eachChild : s->children) {
      Symbol *argType = symbol_ResolveRef(eachChild->type);
      Symbol *argBaseType = symbol_ResolveType(argType);

      if (eachChild->cls == sc_formal)
	continue;

      if ((needRegs = can_registerize(argBaseType, snd_regcount))) {
	emit_return_via_reg(outFile, eachChild, 4, snd_regcount);
	snd_regcount += needRegs;
      }

      else if (symbol_IsVarSequenceType(argBaseType)) {
	unsigned elemAlign = symbol_alignof(argBaseType);

	curAlign = emit_symbol_align("sndIndir", outFile, 4,
				     elemAlign, curAlign);
	do_indent(outFile, 4);
	fprintf(outFile, 
		"__builtin_memcpy(msg.snd_data + sndIndir, "
		"%s->data, %s->len * sizeof(*%s->data));\n",
		eachChild->name, eachChild->name, eachChild->name);

	do_indent(outFile, 4);
	fprintf(outFile, 
		"sndIndir += (%s->len * sizeof(*%s->data));\n",
		eachChild->name, eachChild->name);
      }
    }

    if (vec_len(sndString)) {
      do_indent(outFile, 4);

      if (sndIndirect)
	fprintf(outFile, "msg->snd_len = sndIndir;\n");
      else
	fprintf(outFile, "msg->snd_len = %d;\n", sndDirect);
    }

    do_indent(outFile, 2);
    fprintf(outFile, "}\n");
  }

  fprintf(outFile, "}\n");
}

static void
emit_if_decoder(Symbol *s, FILE *outFile)
{
  if (!s->isActiveUOC)
    return;

  fprintf(outFile, "\n");
  fprintf(outFile, "static void\n");
  fprintf(outFile, "DISPATCH_IF_%s(Message *msg, IfInfo *info)\n", 
	  symbol_QualifiedName(s, '_'));
  fprintf(outFile, "{\n");

  do_indent(outFile, 2);
  fprintf(outFile, "switch(msg->rcv_code) {\n");

  while (s) {
    fprintf(outFile, "\n");
    do_indent(outFile, 2);
    fprintf(outFile, "/* IF %s */\n", symbol_QualifiedName(s, '_'));
    fprintf(outFile, "\n");

    for (const auto eachChild : s->children) {
      if (eachChild->cls != sc_operation)
	continue;

      do_indent(outFile, 2);
      fprintf(outFile, "case OC_%s:\n", eachChild->QualifiedName('_'));
      do_indent(outFile, 4);
      fprintf(outFile, "DISPATCH_OP_%s(msg, info);\n", 
	      symbol_QualifiedName(s, '_'));
    }

    s = s->baseType ? symbol_ResolveRef(s->baseType) : NULL;
  }

  fprintf(outFile, "\n");
  do_indent(outFile, 2);
  fprintf(outFile, "default" ":\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg->snd_code = RC_capros_key_UnknownRequest;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "return;\n");

  do_indent(outFile, 2);
  fprintf(outFile, "}\n");

  fprintf(outFile, "}\n");
}

static void
emit_decoders(Symbol *s, FILE *outFile)
{
  if (s->mark)
    return;

  s->mark = true;

  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      if (s->baseType)
	emit_decoders(symbol_ResolveRef(s->baseType), outFile);

      for (const auto eachChild : s->children)
	emit_decoders(eachChild, outFile);

      emit_if_decoder(s, outFile);

      return;
    }

  case sc_operation:
    {
      emit_op_dispatcher(s, outFile);

      return;
    }

  default:
    return;
  }

  return;
}

static size_t
msg_size(Symbol *s, SymClass sc)
{
  size_t sz = 0;

  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      for (const auto eachChild : s->children)
	sz = max(sz, msg_size(eachChild, sc));

#if 0
      fprintf(outFile, "IF %s size for %s is %d\n", 
	     (sc == sc_formal) ? "Receive" : "Send",
	     symbol_QualifiedName(s, '_'), sz);
#endif

      return sz;
    }

  case sc_operation:
    {
      size_t rcvsz;
      size_t sndsz;

      PtrVec *rcvString = extract_string_arguments(s, sc_formal);
      PtrVec *sndString = extract_string_arguments(s, sc_outformal);

      rcvsz = compute_direct_bytes(rcvString);
      sndsz = compute_direct_bytes(sndString);

      rcvsz = round_up(rcvsz, 8);
      sndsz = round_up(sndsz, 8);

      rcvsz += compute_indirect_bytes(rcvString);
      sndsz += compute_indirect_bytes(sndString);

      sz = (sc == sc_outformal) ? sndsz : rcvsz;

#if 0
      fprintf(outFile, "  OP %s size for %s is %d (rcv %d, snd %d)\n", 
	     (sc == sc_formal) ? "Receive" : "Send",
	     symbol_QualifiedName(s, '_'), sz, rcvsz, sndsz);
#endif

      return sz;
    }

  default:
    return sz;
  }

  return 0;
}

static size_t
size_server_buffer(Symbol *scope, SymClass sc)
{
  size_t bufSz = 0;

  /* Export subordinate packages first! */
  for (const auto eachChild : scope->children) {
    if (eachChild->cls != sc_package && eachChild->isActiveUOC)
      bufSz = max(bufSz, msg_size(eachChild, sc));

    if (eachChild->cls == sc_package) {
      size_t msgsz = size_server_buffer(eachChild, sc);
      bufSz = max(bufSz, msgsz);
    }
  }

  return bufSz;
}

static void
calc_sym_depend(Symbol *s, PtrVec *vec)
{
  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      symbol_ComputeDependencies(s, vec);

      {
	Symbol *targetUoc = symbol_UnitOfCompilation(s);
	if (!ptrvec_contains(vec, targetUoc))
	  ptrvec_append(vec, targetUoc);
      }

      for (const auto eachChild : s->children)
	calc_sym_depend(eachChild, vec);
    }

  case sc_operation:
    {
      symbol_ComputeDependencies(s, vec);
      return;
    }

  default:
    return;
  }

  return;
}

static void
compute_server_dependencies(Symbol *scope, PtrVec *vec)
{
  /* Export subordinate packages first! */
  for (const auto eachChild : scope->children) {
    if (eachChild->cls != sc_package && eachChild->isActiveUOC)
      calc_sym_depend(eachChild, vec);

    if (eachChild->cls == sc_package)
      compute_server_dependencies(eachChild, vec);
  }

  return;
}

static void
emit_server_decoders(Symbol *scope, FILE *outFile)
{
  /* Export subordinate packages first! */
  for (const auto eachChild : scope->children) {
    if (eachChild->cls != sc_package && eachChild->isActiveUOC)
      emit_decoders(eachChild, outFile);

    if (eachChild->cls == sc_package)
      emit_server_decoders(eachChild, outFile);
  }

  return;
}

static void 
emit_server_dispatcher(Symbol *scope, FILE *outFile)
{
  size_t rcvSz = size_server_buffer(scope, sc_formal);
  size_t sndSz = size_server_buffer(scope, sc_outformal);

  do_indent(outFile, 2);
  fprintf(outFile, "\nvoid\nmainloop(void *globalState)\n{\n");
  do_indent(outFile, 2);
  fprintf(outFile, "Message msg;\n");
  do_indent(outFile, 2);
  fprintf(outFile, "IfInfo info;\n");
  do_indent(outFile, 2);
  fprintf(outFile, "extern bool if_demux(Message *pMsg, IfInfo *);\n");

  do_indent(outFile, 2);
  fprintf(outFile, "size_t sndSz = %d;\n", sndSz);
  do_indent(outFile, 2);
  fprintf(outFile, "size_t rcvSz = %d;\n", rcvSz);
  do_indent(outFile, 2);
  fprintf(outFile, "void *sndBuf = alloca(sndSz);\n");
  do_indent(outFile, 2);
  fprintf(outFile, "void *rcvBuf = alloca(rcvSz);\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 2);
  fprintf(outFile, "info.globalState = globalState;\n");
  do_indent(outFile, 2);
  fprintf(outFile, "info.done = false;\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 2);
  fprintf(outFile, "__builtin_memset(&msg,0,sizeof(msg));\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.rcv_limit = rcvSz;\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.rcv_data = rcvBuf;\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.rcv_key0 = KR_APP(0);\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.rcv_key1 = KR_APP(1);\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.rcv_key2 = KR_APP(2);\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.rcv_rsmkey = KR_RETURN;\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.snd_data = sndBuf;\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 2);
  fprintf(outFile, "/* Initial return to void in order to become available\n");
  do_indent(outFile, 2);
  fprintf(outFile, "   Note that memset() above has set send length to zero. */\n");
  do_indent(outFile, 2);
  fprintf(outFile, "msg.snd_invKey = KR_VOID;\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 2);
  fprintf(outFile, "do {\n");
  do_indent(outFile, 4);
  fprintf(outFile, "RETURN(&msg);\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_invKey = KR_RETURN;\t/* Until proven otherwise */\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 4);
  fprintf(outFile, "/* Get some help from the server implementer to choose decoder\n");
  do_indent(outFile, 4);
  fprintf(outFile, "   based on input key info field. The if_demux() routine is\n");
  do_indent(outFile, 4);
  fprintf(outFile, "   also responsible for fetching register values and keys\n");
  do_indent(outFile, 4);
  fprintf(outFile, "   that may have been stashed in a wrapper somewhere. */\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_len = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_key0 = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_key1 = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_key2 = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_w0 = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_w1 = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_w2 = 0;\n");
  do_indent(outFile, 4);
  fprintf(outFile, "msg.snd_code = RC_capros_key_UnknownRequest;\t/* Until otherwise proven */\n");

  fprintf(outFile, "\n");

  do_indent(outFile, 4);
  fprintf(outFile, "if (demux_if(&msg, &info))\n");
  do_indent(outFile, 6);
  fprintf(outFile, "info.if_proc(&msg, &info);\n");
  do_indent(outFile, 2);
  fprintf(outFile, "} while (!info.done);\n");
  fprintf(outFile, "}\n");
}

void 
output_c_server(Symbol *universalScope, BackEndFn fn)
{
  extern const char *outputFileName;
  extern const char *cHeaderName;
  FILE *outFile;
  unsigned i;

  PtrVec *vec = ptrvec_create();

  if (outputFileName == 0)
    diag_fatal(1, "Need to provide output file name.\n");

  if (strcmp(outputFileName, "-") == 0 )
    outFile = stdout;
  else {
    outFile = fopen(outputFileName, "w");
    if (outFile == NULL)
      diag_fatal(1, "Could not open output file \"%s\" -- %s\n",
		 outputFileName, strerror(errno));
  }

  compute_server_dependencies(universalScope, vec);

  preamble = buffer_create();

  buffer_appendString(preamble, "#include <stdbool.h>\n");
  buffer_appendString(preamble, "#include <stddef.h>\n");
  buffer_appendString(preamble, "#include <alloca.h>\n");
  buffer_appendString(preamble, "#include <eros/target.h>\n");
  buffer_appendString(preamble, "#include <eros/Invoke.h>\n");
  buffer_appendString(preamble, "#include <domain/Runtime.h>\n");

  for (i = 0; i < vec_len(vec); i++) {
    buffer_appendString(preamble, "#include <idl/");
    buffer_appendString(preamble, symbol_QualifiedName(symvec_fetch(vec,i), '/'));
    buffer_appendString(preamble, ".h>\n");
  }

  buffer_appendString(preamble, "#include \"");
  buffer_appendString(preamble, cHeaderName);
  buffer_appendString(preamble, "\"\n");

  buffer_freeze(preamble);

  {
    BufferChunk bc;
    off_t pos = 0;
    off_t end = buffer_length(preamble);

    while (pos < end) {
      bc = buffer_getChunk(preamble, pos, end - pos);
      fwrite(bc.ptr, 1, bc.len, outFile);
      pos += bc.len;
    }
  }

  fprintf(outFile, "\ntypedef struct IfInfo {\n");
  do_indent(outFile, 2);
  fprintf(outFile, "void (*if_proc)(Message *pMsg, struct IfInfo *ifInfo);\n");
  do_indent(outFile, 2);
  fprintf(outFile, "void *invState;\t/* passed to handler */\n");
  do_indent(outFile, 2);
  fprintf(outFile, "void *globalState;\t/* passed to handler */\n");
  do_indent(outFile, 2);
  fprintf(outFile, "bool done;\t/* set true by handler when exiting */\n");
  fprintf(outFile, "} IfInfo;\n");

  symbol_ClearAllMarks(universalScope);

  emit_server_decoders(universalScope, outFile);

  emit_server_dispatcher(universalScope, outFile);

  if (outFile != stdout) 
    fclose(outFile);
}
