/*
 * Copyright (C) 2009-2010, Strawberry Development Group.
 * All rights reserved.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* uploaderHTTP: Handle a request from HTTP to run a confined program. */

#include <string.h>
#include <eros/Invoke.h>

#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/File.h>
#include <idl/capros/FileServer.h>
#include <idl/capros/HTTPRequestHandler.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpaceDS.h>

#include "elf.h"

#include "constituents.h"

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

#define KR_OSTREAM	KR_APP(0)
#define KR_ZS           KR_APP(1)
#define KR_FS           KR_APP(2) // a FileServer
#define KR_File         KR_APP(3) // a File
#define KR_GPT          KR_APP(4)
#define KR_Proc         KR_APP(5)

#define dbg_init    0x1
#define dbg_file    0x2
#define dbg_errors  0x4

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define SegAddr ((uint8_t *)(1 << 22))

/************************  Stuff for loading the object file **************/

bool	// false if successful
ReadFile(capros_File_fileLocation at, void * to, uint32_t len)
{
  result_t result;
  uint32_t lenRead;

  // Before receiving a string into a VCS, we have to manifest the pages:
  uint8_t * p = to;
  uint8_t * end = p + len;
  for (; p < end; p = (uint8_t *)(((uint32_t)p | (EROS_PAGE_SIZE - 1)) + 1)) {
    *p = 0;
  }

  result = capros_File_readLong(KR_File, at, len, (uint8_t *)to, &lenRead);
  assert(result == RC_OK);
  if (lenRead != len) {
    DEBUG(errors) kprintf(KR_OSTREAM, "uploaderHttp: file too short\n");
    capros_key_destroy(KR_File);	// done with the File
    return true;
  }
  return false;
}

void
ProcessFile(void)
{
  int i;
  DEBUG(file) kprintf(KR_OSTREAM, "uploaderHttp: ProcessFile called.\n");

  // KR_File has the uploaded ELF object file. Load it into the FS.
  Elf32_Ehdr exehdr;
  if (ReadFile(0, &exehdr, sizeof(exehdr)))
    return;
  if (memcmp(exehdr.e_ident, ELFMAG, SELFMAG) != 0) {
    DEBUG(errors) kprintf(KR_OSTREAM, "uploaderHttp: not ELF\n");
    capros_key_destroy(KR_File);	// done with the File
    return;	// not ELF
  }

  // Load all the LOAD program headers.
  for (i = 0; i < exehdr.e_phnum; i++) {
    Elf32_Phdr phdr;
    if (ReadFile(exehdr.e_phoff + exehdr.e_phentsize * i, &phdr, sizeof(phdr)))
      return;
    if (phdr.p_type == PT_LOAD) {
      DEBUG(file) kprintf(KR_OSTREAM, "Loading %#x bytes from %#x to %#x\n",
                          phdr.p_filesz, phdr.p_offset, phdr.p_vaddr);
      if (ReadFile(phdr.p_offset, SegAddr + phdr.p_vaddr, phdr.p_filesz))
        return;
    }
  }

  capros_key_destroy(KR_File);	// done with the File

  // Build the process for the confined program.
  capros_Process_swapSchedule(KR_Proc, KR_SCHED, KR_VOID);
  capros_Node_getSlot(KR_CONSTIT, KC_DemoSym, KR_TEMP0);
  capros_Process_swapSymSpace(KR_Proc, KR_TEMP0, KR_VOID);
  capros_Node_getSlot(KR_CONSTIT, KC_DemoConstit, KR_TEMP0);
  capros_Process_swapKeyReg(KR_Proc, KR_CONSTIT, KR_TEMP0, KR_VOID);
  capros_Process_swapKeyReg(KR_Proc, KR_SELF, KR_Proc, KR_VOID);
  capros_Process_swapKeyReg(KR_Proc, KR_CREATOR, KR_CREATOR, KR_VOID);
  capros_Process_swapKeyReg(KR_Proc, KR_BANK, KR_BANK, KR_VOID);
  capros_Process_swapKeyReg(KR_Proc, KR_SCHED, KR_SCHED, KR_VOID);
  capros_Process_swapAddrSpaceAndPC32(KR_Proc, KR_ZS, exehdr.e_entry,
                                      KR_VOID);

  // Start it.
  capros_Process_makeResumeKey(KR_Proc, KR_TEMP0);
  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0
  };
  SEND(&Msg);
}

/****************************  main server loop  ***************************/

void
Sepuku(result_t retCode)
{
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);

  /* Invoke the protospace to destroy us and return. */
  protospace_destroy_small(KR_TEMP0, retCode);
  // Does not return here.
}

#define bufSize 4096
uint32_t buf[bufSize / sizeof(uint32_t)];

int
main(void)
{
  Message Msg;
  Message * msg = &Msg;
  result_t result;
  capros_File_fileLocation fileCursor = 0;
  bool gotBody = false;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  // Make a new memory tree root:
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_GPT);
  if (result != RC_OK)
    goto destroy0;
  result = capros_GPT_setL2v(KR_GPT, 22);
  assert(result == RC_OK);

  // Create the segment for the program's address space:
  capros_Node_getSlot(KR_CONSTIT, KC_ZSC, KR_TEMP0);
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED, KR_VOID,
                                      KR_ZS);
  if (result != RC_OK)
    goto destroy1;

  // Create a FileServer and a File for the object file.
  capros_Node_getSlot(KR_CONSTIT, KC_FileSrvC, KR_TEMP0);
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED, KR_VOID,
                                      KR_FS);
  if (result != RC_OK)
    goto destroy2;
  result = capros_FileServer_createFile(KR_FS, KR_File);
  if (result != RC_OK)
    goto destroy3;

  result = capros_ProcCre_createProcess(KR_CREATOR, KR_BANK, KR_Proc);
  if (result != RC_OK) {
  destroy4:
    capros_key_destroy(KR_File);
  destroy3:
    capros_key_destroy(KR_FS);
  destroy2:
    capros_key_destroy(KR_ZS);
  destroy1:
    capros_SpaceBank_free1(KR_BANK, KR_GPT);
  destroy0:
    Sepuku(result);
  }

  // Build the new address space:
  result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_GPT_setSlot(KR_GPT, 0, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_GPT_setSlot(KR_GPT, 1, KR_ZS);
  assert(result == RC_OK);
  // Set new address space:
  result = capros_Process_swapAddrSpace(KR_SELF, KR_GPT, KR_VOID);
  assert(result == RC_OK);

  static char responseHeader[] = 
    "";
  char * responseHeaderCursor = responseHeader;
  // Do not include the terminating NUL in the length:
  int responseHeaderLength = sizeof(responseHeader) - 1;

  DEBUG(init) kprintf(KR_OSTREAM, "uploaderHTTP: initialized\n");

  // Return a start key to self.
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  assert(result == RC_OK);
  
  msg->snd_invKey = KR_RETURN;
  msg->snd_key0 = KR_TEMP0;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  msg->snd_code = 0;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  msg->rcv_key0 = KR_VOID;
  msg->rcv_key1 = KR_VOID;
  msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_data = buf;
  msg->rcv_limit = bufSize;

  for(;;) {
    RETURN(msg);

    // Set default return values:
    msg->snd_invKey = KR_RETURN;
    msg->snd_key0 = KR_VOID;
    msg->snd_key1 = KR_VOID;
    msg->snd_key2 = KR_VOID;
    msg->snd_rsmkey = KR_VOID;
    msg->snd_len = 0;
    msg->snd_code = RC_OK;
    msg->snd_w1 = 0;
    msg->snd_w2 = 0;
    msg->snd_w3 = 0;

    switch (msg->rcv_code) {

    default:
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_HTTPRequestHandler;
      break;

    case 0:	// OC_capros_HTTPRequestHandler_headers
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Ignore the headers.
      msg->snd_w1 = bufSize;
      break;

    case 1:	// OC_capros_HTTPRequestHandler_body
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Append body data to the File.
      uint32_t len;
      result = capros_File_write(KR_File, fileCursor,
                 msg->rcv_sent, (uint8_t *)buf, &len);
      assert(result == RC_OK);
      if (len < msg->rcv_sent) {
        kdprintf(KR_OSTREAM,
                 "uploaderHTTP has %d bytes but could only write %d!\n",
                 msg->rcv_sent, len);
        // This is probably due to uploading a very large file.
        // Terminate the upload.
        result = RC_capros_SpaceBank_LimitReached;
        goto destroy4;
      }
      fileCursor += msg->rcv_sent;
      gotBody = true;
      msg->snd_w1 = bufSize;
      break;

    case 2:	// OC_capros_HTTPRequestHandler_trailer
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Ignore the trailers.
      msg->snd_w1 = bufSize;
      break;

    case OC_capros_HTTPRequestHandler_getResponseStatus:
      msg->snd_w1 = 204;	// No Content
      msg->snd_w2 = capros_HTTPRequestHandler_TransferEncoding_none;
      break;

    case 3:	// OC_capros_HTTPRequestHandler_getResponseHeaderData
    {
      uint32_t dataLimit = msg->rcv_w1;
      if (responseHeaderLength < dataLimit)	// take min
        dataLimit = responseHeaderLength;
      msg->snd_data = responseHeaderCursor;
      msg->snd_len = dataLimit;
      responseHeaderCursor += dataLimit;
      responseHeaderLength -= dataLimit;
      break;
    }

    // Since we responded with TransferEncoding_none and 204 No Content,
    // we should not get OC_capros_HTTPRequestHandler_getResponseBody

    case OC_capros_key_destroy:
      if (gotBody)
        ProcessFile();
      else {
        DEBUG(errors) kprintf(KR_OSTREAM, "uploaderHttp: no body\n");
      }
      Sepuku(RC_OK);
      /* NOTREACHED */
    }
  }
}
