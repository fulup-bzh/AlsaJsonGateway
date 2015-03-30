/*
   alsa-gateway -- provide a REST/HTTP interface to ALSA-Mixer

   Copyright (C) 2015, Fulup Ar Foll

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   References: http://alsa-lib.sourcearchive.com/documentation/1.0.20/modules.html

   $Id: $
*/


#include "local-def.h"


/* --------------------------------------------------------------
 *  create a handle to display test command result 
 * --------------------------------------------------------------*/
PUBLIC AJG_display *displayOpen (WX_config *config) {

  WX_display *display;

  display = malloc (sizeof (WX_display));
  if (display == NULL) return FATAL;
 
  display->file     = stdout;
  return display;
}


/* --------------------------------------------------------------
 *  Flush & Close display
 * --------------------------------------------------------------*/
void displayClose (WX_display *display) {

  if (display == NULL) return;  // if not display backend return

  fflush (display->file);
  free   (display);
}

