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

   References:
   https://github.com/json-c/json-c
   https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/

   $Id: $
*/

#include "local-def-ajg.h"
#include <dirent.h>
#include <string.h>
#include <time.h>



// verify we can read/write in session dir
PUBLIC AJG_ERROR sessionCheckdir (AJG_session *session) {

   int err;


   // in case session dir would not exist create one
   if (verbose) fprintf (stderr, "AlsaJson: Checking session dir [%s]\n", session->config->sessiondir);
   mkdir(session->config->sessiondir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

   // change for session directory
   err = chdir(session->config->sessiondir);
   if (err) {
     fprintf(stderr,"AlsaJson: Fail to chdir to %s error=%s\n", session->config->sessiondir, strerror(err));
     return err;
   }

   // verify we can write session in directory
   json_object *dummy= json_object_new_object();
   json_object_object_add (dummy, "checked"  , json_object_new_int (getppid()));
   err = json_object_to_file ("./AJG-probe.json", dummy);
   if (err < 0) return err;

   return AJG_SUCCESS;
}

// let's return only sessions files
STATIC int fileSelect (const struct dirent *entry) {
   return (strstr (entry->d_name, ".ajg") != NULL);
}

// create a session in current directory
PUBLIC json_object *sessionList (AJG_session *session) {
    json_object *sessionsJ, *ajgResponse;
    struct stat fstat;
    struct dirent **namelist;
    int    count;

    count = scandir(session->config->sessiondir, &namelist, fileSelect, alphasort);
    if (count < 0) {
        return (jsonNewMessage (AJG_FATAL,"Fail to scan sessions directory", session->config->sessiondir));
    }

    if (count == 0) return NULL;

    // loop on each session file, retrieve its date and push it into json response object
    sessionsJ = json_object_new_object();
    while (count--) {
         char timestamp [64];
         char *filename;
         struct tm lastupdate;

         filename = namelist[count]->d_name;

         printf("%s\n", filename);
         stat(filename,&fstat);

         strftime (timestamp, sizeof(timestamp), "%c", localtime (&fstat.st_mtime));
         json_object_object_add (sessionsJ,filename , json_object_new_string (timestamp));

         free(namelist[count]);
    }

    // free scandir structure
    free(namelist);

    // everything is OK let's build final response
    ajgResponse = json_object_new_object();
    json_object_object_add (ajgResponse, "ajgtype"   , jsonNewAjgType());
    json_object_object_add (ajgResponse, "status" , jsonNewError(AJG_SUCCESS));
    json_object_object_add (ajgResponse, "data"   , sessionsJ);

    return (ajgResponse);
}

// push Json session object to disk
PUBLIC json_object * sessionToDisk (AJG_session *session, AJG_request *request, json_object *jsonSession) {
   static json_object *AJG_JSON_DONE;
   char filename [256];
   time_t rawtime;
   struct tm * timeinfo;
   int err;

   // add file extention to session name
   strncpy (filename, request->args, sizeof(filename));
   strncat (filename, ".ajg", sizeof(filename));


   json_object_object_add(jsonSession, "ajgtype", json_object_new_string ("AJG_session"));

   // add a timestamp and store session on disk
   time ( &rawtime );  timeinfo = localtime ( &rawtime );
   // A copy of the string is made and the memory is managed by the json_object
   json_object_object_add (jsonSession, "timestamp", json_object_new_string (asctime (timeinfo)));

   err = json_object_to_file (filename, jsonSession);
   json_object_put   (jsonSession);    // decrease reference count to free the json object

   // if OK we do not return anything
   if (err < 0) return jsonNewMessage (AJG_FATAL,"Fail save session = [%s] to disk", filename);
   return jsonNewMessage (AJG_SUCCESS,"Session= [%s] saved on disk", filename);
}

// push Json session object to disk
PUBLIC json_object *sessionFromDisk (AJG_session *session, AJG_request *request) {
    json_object * jsonSession;

   // just upload json object and return without any further processing
   jsonSession = json_object_from_file (request->args);

   return jsonSession;
}
