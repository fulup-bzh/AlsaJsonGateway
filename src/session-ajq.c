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

*/

#include "local-def-ajg.h"
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define AJG_SESSION_JTYPE "AJG_session"
#define AJG_SESSION_JLIST "AJG_sessions"
#define AJG_SESSION_JINFO "AJG_infos"

#define AJG_CURRENT_SESSION "active-session"  // file link name within sndcard dir
#define AJG_DEFAULT_SESSION "current-session" // should be in sync with UI




// verify we can read/write in session dir
PUBLIC AJG_ERROR sessionCheckdir (AJG_session *session) {

   int err;

   // in case session dir would not exist create one
   if (verbose) fprintf (stderr, "AJG:notice checking session dir [%s]\n", session->config->sessiondir);
   mkdir(session->config->sessiondir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

   // change for session directory
   err = chdir(session->config->sessiondir);
   if (err) {
     fprintf(stderr,"AJG: Fail to chdir to %s error=%s\n", session->config->sessiondir, strerror(err));
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

STATIC  json_object *checkCardDirExit (AJG_session *session, AJG_request *request ) {
    int  sessionDir, cardDir;

    // card name should be more than 5 character long !!!!
    if (strlen (request->cardname) < 5) {
       return (jsonNewMessage (AJG_FAIL,"Fail invalid sndcard cardid=%s cardname [%s]", request->cardid, request->cardname));
    }

    // open session directory
    sessionDir = open (session->config->sessiondir, O_DIRECTORY);
    if (sessionDir < 0) {
          return (jsonNewMessage (AJG_FAIL,"Fail to open directory [%s] error=%s", session->config->sessiondir, strerror(sessionDir)));
    }

   // create session sndcard directory if it does not exit
    cardDir = openat (sessionDir, request->cardname,  O_DIRECTORY);
    if (cardDir < 0) {
          cardDir  = mkdirat (sessionDir, request->cardname, O_RDWR | S_IRWXU | S_IRGRP);
          if (cardDir < 0) {
              return (jsonNewMessage (AJG_FAIL,"Fail to create directory [%s/%s] error=%s", session->config->sessiondir, request->cardname, strerror(cardDir)));
          }
    }
    close (sessionDir);
    return NULL;
}

// create a session in current directory
PUBLIC json_object *sessionList (AJG_session *session, AJG_request *request) {
    json_object *sessionsJ, *ajgResponse;
    struct stat fstat;
    struct dirent **namelist;
    int  count, sessionDir;

    // if directory for card's sessions does not exist create it
    ajgResponse = checkCardDirExit (session, request);
    if (ajgResponse != NULL) return ajgResponse;

    // open session directory
    sessionDir = open (session->config->sessiondir, O_DIRECTORY);
    if (sessionDir < 0) {
          return (jsonNewMessage (AJG_FAIL,"Fail to open directory [%s] error=%s", session->config->sessiondir, strerror(sessionDir)));
    }

    count = scandirat (sessionDir, request->cardname, &namelist, fileSelect, alphasort);
    close (sessionDir);

    if (count < 0) {
        return (jsonNewMessage (AJG_FAIL,"Fail to scan sessions directory [%s/%s] error=%s", session->config->sessiondir, request->cardname, strerror(sessionDir)));
    }
    if (count == 0) return (jsonNewMessage (AJG_EMPTY,"[%s] no session at [%s]", request->cardname, session->config->sessiondir));

    // loop on each session file, retrieve its date and push it into json response object
    sessionsJ = json_object_new_array();
    while (count--) {
         json_object *sessioninfo;
         char timestamp [64];
         char *filename;

         // extract file name and last modification date
         filename = namelist[count]->d_name;
         printf("%s\n", filename);
         stat(filename,&fstat);
         strftime (timestamp, sizeof(timestamp), "%c", localtime (&fstat.st_mtime));
         filename[strlen(filename)-4] = '\0'; // remove .ajg extension from filename

         // create an object by session with last update date
         sessioninfo = json_object_new_object();
         json_object_object_add (sessioninfo, "date" , json_object_new_string (timestamp));
         json_object_object_add (sessioninfo, "session" , json_object_new_string (filename));
         json_object_array_add (sessionsJ, sessioninfo);

         free(namelist[count]);
    }

    // free scandir structure
    free(namelist);

    // everything is OK let's build final response
    ajgResponse = json_object_new_object();
    json_object_object_add (ajgResponse, "ajgtype" , json_object_new_string (AJG_SESSION_JLIST));
    json_object_object_add (ajgResponse, "status"  , jsonNewStatus(AJG_SUCCESS));
    json_object_object_add (ajgResponse, "data"    , sessionsJ);

    return (ajgResponse);
}

// Create a link toward last used sessionname within sndcard directory
STATIC void makeSessionLink (const char *cardname, const char *sessionname) {
   char linkname [256], filename [256];
   int err;
   // create a link to keep track of last uploaded sessionname for this card
   strncpy (filename, sessionname, sizeof(filename));
   strncat (filename, ".ajg", sizeof(filename));

   strncpy (linkname, cardname, sizeof(linkname));
   strncat (linkname, "/", sizeof(filename));
   strncat (linkname, AJG_CURRENT_SESSION, sizeof(linkname));
   strncat (linkname, ".ajg", sizeof(filename));
   unlink (linkname); // remove previous link if any
   err = symlink (filename, linkname);
   if (err < 0) fprintf (stderr, "Fail to create link %s->%s error=%s\n", linkname, filename, strerror(errno));
}

// Load Json session object from disk
PUBLIC json_object *sessionFromDisk (AJG_session *session, AJG_request *request) {
    json_object *jsonSession, *ajgtype, *response;
    const char *ajglabel;
    char filename [256];
    int defsession;

    if (request->args == NULL) {
        return  (jsonNewMessage (AJG_FATAL,"session name missing &session=MySessionName", filename));
    }

    // check for current session request
    defsession = (strcmp (request->args, AJG_DEFAULT_SESSION) ==0);

    // if directory for card's sessions does not exist create it
    response = checkCardDirExit (session, request);
    if (response != NULL) return response;

    // add cardname and file extension to session name
    strncpy (filename, request->cardname, sizeof(filename));
    strncat (filename, "/", sizeof(filename));
    if (defsession) strncat (filename, AJG_CURRENT_SESSION, sizeof(filename)-1);
    else strncat (filename, request->args, sizeof(filename)-1);
    strncat (filename, ".ajg", sizeof(filename));

    // just upload json object and return without any further processing
    jsonSession = json_object_from_file (filename);

    if (jsonSession == NULL)  return (jsonNewMessage (AJG_EMPTY,"File [%s] not found", filename));

    // verify that file is a JSON ALSA session type
    if (!json_object_object_get_ex (jsonSession, "ajgtype", &ajgtype)) {
        json_object_put   (jsonSession);
        return  (jsonNewMessage (AJG_EMPTY,"File [%s] 'ajgtype' descriptor not found", filename));
    }

    // check type value is AJG_SESSION_JTYPE
    ajglabel = json_object_get_string (ajgtype);
    if (strcmp (AJG_SESSION_JTYPE, ajglabel)) {
       json_object_put   (jsonSession);
       return  (jsonNewMessage (AJG_FATAL,"File [%s] ajgtype=[%s] != [%s]", filename, ajglabel, AJG_SESSION_JTYPE));
    }

    // create a link to keep track of last uploaded session for this card
    if (!defsession) makeSessionLink (request->cardname, request->args);

    return (jsonSession);
}

// push Json session object to disk
PUBLIC json_object * sessionToDisk (AJG_session *session, AJG_request *request, json_object *jsonSession) {
   char filename [256];
   time_t rawtime;
   struct tm * timeinfo;
   int err, defsession;
   static json_object *response;

   // we should have a session name
   if (request->args == NULL) return (jsonNewMessage (AJG_FATAL,"session name missing &session=MySessionName", filename));

   // check for current session request
   defsession = (strcmp (request->args, AJG_DEFAULT_SESSION) ==0);

   // if directory for card's sessions does not exist create it
   response = checkCardDirExit (session, request);
   if (response != NULL) return response;

   // add cardname and file extension to session name
   strncpy (filename, request->cardname, sizeof(filename));
   strncat (filename, "/", sizeof(filename));
   if (defsession) strncat (filename, AJG_CURRENT_SESSION, sizeof(filename)-1);
   else strncat (filename, request->args, sizeof(filename)-1);
   strncat (filename, ".ajg", sizeof(filename)-1);


   json_object_object_add(jsonSession, "ajgtype", json_object_new_string (AJG_SESSION_JTYPE));

   // add a timestamp and store session on disk
   time ( &rawtime );  timeinfo = localtime ( &rawtime );
   // A copy of the string is made and the memory is managed by the json_object
   json_object_object_add (jsonSession, "timestamp", json_object_new_string (asctime (timeinfo)));


   // do we have extra session info ?
   if (request->data) {
       static json_object *info, *ajgtype;
       const char  *ajglabel;

       // extract session info from args
       info = json_tokener_parse (request->data);
       if (!info) {
            response = jsonNewMessage (AJG_FATAL,"sndcard=%s session=%s invalid json args=%s", request->cardname, request->args, request->data);
            goto OnErrorExit;
       }

       // info is a valid AJG_info type
       if (!json_object_object_get_ex (info, "ajgtype", &ajgtype)) {
            response = jsonNewMessage (AJG_EMPTY,"sndcard=%s session=%s No 'AJG_type' args=%s", request->cardname, request->args, request->data);
            goto OnErrorExit;
       }

       // check type value is AJG_INFO_JTYPE
       ajglabel = json_object_get_string (ajgtype);
       if (strcmp (AJG_SESSION_JINFO, ajglabel)) {
              json_object_put   (info); // release info json object
              response = jsonNewMessage (AJG_FATAL,"File [%s] ajgtype=[%s] != [%s] data=%s", filename, ajglabel, AJG_SESSION_JTYPE, request->data);
              goto OnErrorExit;
       }

       // this is valid info data for our session
       json_object_object_add (jsonSession, "info", info);
   }

   // Finally save session on disk
   err = json_object_to_file (filename, jsonSession);
   if (err < 0) {
        response = jsonNewMessage (AJG_FATAL,"Fail save session = [%s] to disk", filename);
        goto OnErrorExit;
   }


   // create a link to keep track of last uploaded session for this card
   if (!defsession) makeSessionLink (request->cardname, request->args);

   // we're donne let's return status message
   response = jsonNewMessage (AJG_SUCCESS,"Session= [%s] saved on disk", filename);
   json_object_put (jsonSession);
   return (response);

OnErrorExit:
   json_object_put (jsonSession);
   return response;
}
