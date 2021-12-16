/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <ncurses.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>


#include <disk/DiskKey.h>
#include <disk/PagePot.hxx>

#include <erosimg/App.hxx>
#include <erosimg/Parse.hxx>
#include <erosimg/ExecImage.hxx>
#include <erosimg/Volume.h>
#include <erosimg/DiskDescrip.h>

#define NUM_ENTRIES 7
#define ENTRY_WIDTH 11
#define MENU_OFFSET 3

class App App("divmgr");
Volume vol;
  
main(int /* argc */, char *argv[]){

  /* int validentry=0;
   * int inputkey=0;
   */

  int loopvar=0;
  int end_program=0;

  int topmenuloc=0;
  char topmenu[NUM_ENTRIES][ENTRY_WIDTH];
  char tips[NUM_ENTRIES][30];

  WINDOW *divwin;
  /*  char *divout; */
  typedef struct{
    int start;
    int end;
    char *typeName;
  } DIVTYPE;
  DIVTYPE divlist[65];
  const char* targname;
  
  initscr();
  cbreak();
  noecho();
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE);

  
  strcpy(topmenu[0],"  Delete  ");
  strcpy(topmenu[1],"   Page   ");
  strcpy(topmenu[2],"  Capage  ");
  strcpy(topmenu[3],"  Kernel  ");
  strcpy(topmenu[4],"   Log    ");
  strcpy(topmenu[5],"  Write   ");
  strcpy(topmenu[6],"   Quit   ");
  strcpy(tips[0],"Delete a division           ");
  strcpy(tips[1],"Create a page range         ");
  strcpy(tips[2],"Create a capage range       ");
  strcpy(tips[3],"Create a kernel range       ");
  strcpy(tips[4],"Create a log range          ");
  strcpy(tips[5],"Write changes and quit      ");
  strcpy(tips[6],"Quit without writing changes");

  move(1,28);
  addstr("EROS Division Manager v 0.1");
  attron(A_REVERSE);
  move(20,MENU_OFFSET);
  addstr(topmenu[0]);
  attroff(A_REVERSE);
  move(22,MENU_OFFSET);
  addstr(tips[0]);
  for(loopvar=1;loopvar<NUM_ENTRIES;loopvar++){
    move(20,loopvar*10+MENU_OFFSET);
    addstr(topmenu[loopvar]);
  }
  move(3,1);
  addstr("Division  Start   End   Size  Type        Info");
  move(0,0);
  refresh();
  divwin=newwin(15,76,5,1);

  wrefresh(divwin);

  targname = *argv;
  vol.Open(targname, false);
      
  for (loopvar = 0; loopvar < vol.MaxDiv(); loopvar++) {
    const Division& d = vol.GetDivision(loopvar);
    divlist[loopvar].start=d.start;
    divlist[loopvar].end=d.end;
    strcpy(divlist[loopvar].typeName, d.TypeName());
  }

  while(!end_program){
    switch (getch()){
    case KEY_LEFT:
      if(topmenuloc!=0){
	attroff(A_REVERSE);
	move(20,topmenuloc*10+MENU_OFFSET);
	addstr(topmenu[topmenuloc]);
	topmenuloc--;
	move(20,topmenuloc*10+MENU_OFFSET);
	attron(A_REVERSE);
	addstr(topmenu[topmenuloc]);
	attroff(A_REVERSE);
	move(22,MENU_OFFSET);
	addstr(tips[topmenuloc]);
      }
      break;
    case KEY_RIGHT:
      if(topmenuloc!=NUM_ENTRIES-1){
	attroff(A_REVERSE);
	move(20,topmenuloc*10+MENU_OFFSET);
	addstr(topmenu[topmenuloc]);
	topmenuloc++;
	move(20,topmenuloc*10+MENU_OFFSET);
	attron(A_REVERSE);
	addstr(topmenu[topmenuloc]);
	attroff(A_REVERSE);
	move(22,MENU_OFFSET);
	addstr(tips[topmenuloc]);
      }
      break;
    case 13:  /* Return key */
      switch(topmenuloc){
      case 0:
	break;
      case 1:
	break;
      case 2:
	break;
      case 3:
	break;
      case 4:
	break;
      case 5:
	break;
      case 6:
	end_program=TRUE;
	break;
      } 
      break;
    }
    move(0,0);
    refresh();
  }
  endwin();
}
