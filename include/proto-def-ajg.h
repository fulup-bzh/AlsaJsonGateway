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

   $Id: $
*/

// Alsa proto
PUBLIC json_object* alsaFindCards       (AJG_session *session, AJG_request *request);
PUBLIC json_object* alsaFindControls    (AJG_session *session, AJG_request *request);
PUBLIC json_object *alsaSetControls     (AJG_session *session, AJG_request *request);
PUBLIC json_object *alsaProbeCard       (AJG_session *session, AJG_request *request);
PUBLIC json_object *alsaDownloadSession (AJG_session *session, AJG_request *request);
PUBLIC json_object *alsaUploadSession   (AJG_session *session, AJG_request *request, json_object  *jsonSession);


// Session handling
PUBLIC AJG_ERROR sessionCheckdir             (AJG_session *session);
PUBLIC json_object *sessionList        (AJG_session *session);
PUBLIC json_object *sessionToDisk      (AJG_session *session, AJG_request *request, json_object *jsonSession);
PUBLIC json_object *sessionFromDisk    (AJG_session *session, AJG_request *request);


// Httpd proto
PUBLIC AJG_ERROR httpdStart          (AJG_session *session);
PUBLIC AJG_ERROR httpdLoop           (AJG_session *session);
PUBLIC void  httpdStop           (AJG_session *session);



// config proto
PUBLIC char *configTime        (void);
PUBLIC AJG_session *configInit (void);
PUBLIC json_object *jsonNewMessage (AJG_ERROR level, char* format, ...);
PUBLIC json_object *jsonNewError (AJG_ERROR level);
PUBLIC json_object *jsonNewAjgType (void);

