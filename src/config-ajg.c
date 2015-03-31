/*
   alsajson-gw -- provide a REST/HTTP interface to ALSA-Mixer

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

   References: https://www.gnu.org/software/libmicrohttpd/manual/html_node/index.html#Top

   $Id: $
*/


#include "local-def-ajg.h"

PUBLIC int verbose;

/* ------------------------------------------------------------------------------
 * Get localtime and return in a string
 * ------------------------------------------------------------------------------ */

PUBLIC char * configTime (void) {
  static char reqTime [26];
  time_t tt;
  struct tm *rt;

  /* Get actual Date and Time */
  time (&tt);
  rt = localtime (&tt);

  strftime (reqTime, sizeof (reqTime), "(%d-%b %H:%M)",rt);

  // return pointer on static data
  return (reqTime);
}

PUBLIC AJG_session *configInit () {

  AJG_session *session;
  AJG_config  *config;

  session = malloc (sizeof (AJG_session));
  memset (session,0, sizeof (AJG_session));

  // create config handle
  config = malloc (sizeof (AJG_config));
  memset (config,0, sizeof (AJG_config));

  // set some config default
  config->console = "/tmp/output-ajg.log";

  // stack config handle into session
  session->config = config;

  return (session);
}
