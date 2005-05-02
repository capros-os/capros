#ifndef __ps2_h__
#define __ps2_h__

#define VERBOSE 1
/* This file defines the APIish info for using the PS2 driver in EROS apps.
   Each command is defined by an opcode.  Comments for each
   command define the EROS invocation arguments needed for each
   command.  This driver has only been tested with VMWare Workstation
   2.4 on RedHat Linux 7.1 & natively */

/* After constructing the driver, need to pass your start key so that 
   the driver can call your app to pass mouse & keyboard events 
   Pass Start Key in msg.snd_key1 */
#define OC_builder_key     21

/* The driver calls the app to pass key strokes with this OpCode. The 
   character is passed in msg.rcv_w1 */
#define OC_queue_keyevent   11

/* The driver calls the app to pass mouse clicks and mouse motion. The
   button mask is passed in msg.rcv_w1. X & Y motion in msg.rcv_w2 &
   msg.rcv_w3 respectively */
#define OC_queue_mouseevent 22

/* Arbitrary hex values for LEFT,RIGHT & MIDDLE mouse button mask */
#define LEFT   0x1u
#define RIGHT  0x2u
#define MIDDLE 0x4u


/* Error codes if drivers dies. Check log with #define DEBUG */
#define RC_kbd_init_failure    11
#define RC_kbd_timeout         12
#define RC_kbd_alloc_failure   13
#define RC_mouse_init_failure  21
#define RC_mouse_alloc_failure 22
#define RC_helper_init_failure 31
#define RC_unknown_error       100

/* For mouseclient.c */
#define OC_ps2_mouse         6
#define OC_ps2reader_key     10

/* For keyb.c */
#define OC_set_leds       5 
#define OC_read_ps2kbd    6
#define OC_read_ps2mouse  7 
#define OC_init_ps2       8
#define OC_id_keyboard    9
#define OC_kbd_flush      10
#define OC_irq_arrived    12

/* For the Helpers: helper1.c helper12.c */
#define IRQ1  1
#define IRQ12 12

/* Types of data seen in the ps2 Buffer*/
#define KEYBOARD_DATA   1
#define MOUSE_DATA      2 
#define INVALID_DATA    3
#define VALID_DATA      4

#endif /* __ps2_h__ */




