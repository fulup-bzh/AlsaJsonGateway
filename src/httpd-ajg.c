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

   Object:
    1) REST Gateway with Alsa Mixer C-API.
    2) Minimalist web server to serve HTML partials and JS files to HTML5 UI.

   Syntax:  ./daemon-ajg --verbose --httpdport=1234 --rootdir=$HOME/public

   Features/Restriction:
    - handle ETAG to limit upload to modified/new files [cache default 3600s]
    - handles redirect to index.htlm when path is a directory [code 301]
    - only support GET method
    - does not follow link.

   References: https://www.gnu.org/software/libmicrohttpd/manual/html_node/index.html#Top
   http://libmicrohttpd.sourcearchive.com/documentation/0.4.2/microhttpd_8h.html
   https://gnunet.org/svn/libmicrohttpd/src/examples/fileserver_example_external_select.c
   https://github.com/json-c/json-c
   POST https://www.gnu.org/software/libmicrohttpd/manual/html_node/microhttpd_002dpost.html#microhttpd_002dpost
*/

#include <microhttpd.h>
#include <sys/stat.h>

// proto missing from GCC
char *strcasestr(const char *haystack, const char *needle);


#include "local-def-ajg.h"
#define BANNER "<html><head><title>Alsa Json Gateway</title></head><body>Alsa Json Gateway</body></html>"

#define JSON_CONTENT  "application/json"
#define MAX_POST_SIZE  4096   // maximum size for POST data


static  json_object * Request2Commands = NULL;
//  List of HTTP Query Commands
#define GATEWAY_PING   1
#define CARD_GET_ALL   2
#define CARD_GET_ONE   3
#define CTRL_GET_ALL   4
#define CTRL_GET_ONE   5
#define CTRL_SET_ONE   6
#define CTRL_SET_MANY  7
#define SESSION_LIST   8
#define SESSION_STORE  9
#define SESSION_LOAD   10

static int rqtcount  = 0;  // dummy request rqtcount to make each message be different
static int postcount = 0;

// use json lib hash table capabilities to handle command parsing
STATIC void initService (AJG_session *session) {
    rqtcount = 0;
    Request2Commands = json_object_new_object();

    json_object_object_add(Request2Commands, "ping-get"     , json_object_new_int (GATEWAY_PING));
    json_object_object_add(Request2Commands, "card-get-all" , json_object_new_int (CARD_GET_ALL));
    json_object_object_add(Request2Commands, "card-get-one" , json_object_new_int (CARD_GET_ONE));
    json_object_object_add(Request2Commands, "ctrl-get-all" , json_object_new_int (CTRL_GET_ALL));
    json_object_object_add(Request2Commands, "ctrl-get-one" , json_object_new_int (CTRL_GET_ONE));
    json_object_object_add(Request2Commands, "ctrl-set-one" , json_object_new_int (CTRL_SET_ONE));
    json_object_object_add(Request2Commands, "ctrl-set-many", json_object_new_int (CTRL_SET_MANY));
    json_object_object_add(Request2Commands, "session-list" , json_object_new_int (SESSION_LIST));
    json_object_object_add(Request2Commands, "session-store", json_object_new_int (SESSION_STORE));
    json_object_object_add(Request2Commands, "session-load" , json_object_new_int (SESSION_LOAD));
}

STATIC  json_object *gatewayPing (void) {
    json_object * pingJson = jsonNewMessage (AJG_SUCCESS,"%d", rqtcount);
    return (pingJson);
}


// Because of POST call multiple time requestApi we need to free POST handle here
static void endRequest (void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
  AJG_HttpPost *posthandle = *con_cls;

  // if post handle was used let's free everything
  if (posthandle) {
     if (verbose) fprintf (stderr, "End Post Request UID=%d\n", posthandle->uid);
     free (posthandle->data);
     free (posthandle);
  }
}


// process rest API query
STATIC int requestApi (struct MHD_Connection *connection, AJG_session *session, const char *method,  const char* url
                      , const char *upload_data, size_t *upload_data_size, void **con_cls) {
  const char  *query, *param;
  json_object *cmd;
  int ret;
  json_object *jsonResponse, *errMessage;
  struct MHD_Response  *response;
  AJG_request request;
  const char *serialized;

  // clean up session
  rqtcount++;
  memset (&request, 0, sizeof (request));
  jsonResponse=NULL;

  // Process POST action. Libmicrohttpd POST handling is everything except simple!!! Not only the logic is less
  // than obvious to understand. But furthermore documentation and samples are almost each of them more impossible
  // that the other. In AJG it's even worse as we use JSON contend type that is not supported by Libmicrohttpd
  // PostPossessor API. https://www.gnu.org/software/libmicrohttpd/manual/html_node/microhttpd_002dpost.html#microhttpd_002dpost
  if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {
    const char *encoding;
    int    contentlen=-1;
    AJG_HttpPost *posthandle = *con_cls;

    // Let make sure we have the right encoding and a valid length
    encoding = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
    param    = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
    if (param) sscanf (param,"%i",&contentlen);

    // POST datas may come in multiple chunk. Even when it never happen on AJG, we still have to handle the case
    if (strcasestr (encoding, JSON_CONTENT) == 0) {
        errMessage = jsonNewMessage (AJG_FATAL, "Post Date wrong type encoding=%s != %s", encoding, JSON_CONTENT);
        goto ExitOnError;
    }

    if (contentlen > MAX_POST_SIZE) {
        errMessage = jsonNewMessage (AJG_FATAL, "Post Date to big %d > %d", contentlen, MAX_POST_SIZE);
        goto ExitOnError;
    }

    // In POST mode 1st call is only design to establish POST processor.
    // As JSON content is not supported out of box, but must provide something equivalent.
    if (posthandle == NULL) {
       posthandle = malloc (sizeof (AJG_HttpPost));   // allocate application POST processor handle
       posthandle->uid = postcount ++;                // build a UID for DEBUG
       posthandle->len = 0;                           // effective length within POST handler
       posthandle->data= malloc (contentlen +1);      // allocate memory for full POST data + 1 for '\0' enf of string
       *con_cls = posthandle;                         // attache POST handle to current HTTP session

       if (verbose) fprintf (stderr, "Create Post Request UID=%d\n", posthandle->uid);
       return MHD_YES;
    }

    // This time we receive partial/all Post data. Note that even if we get all POST data. We should nevertheless
    // return MHD_YES and not process the request directly. Otherwise Libmicrohttpd is unhappy and fails with
    // 'Internal application error, closing connection'.
    if (*upload_data_size) {
        if (verbose) fprintf (stderr, "Update Post Request UID=%d\n", posthandle->uid);

        memcpy (&posthandle->data[posthandle->len], upload_data, *upload_data_size);
        posthandle->len = posthandle->len + *upload_data_size;
        *upload_data_size = 0;
        return MHD_YES;
    }

    // We should only start to process DATA after Libmicrohttpd call or application handler with *upload_data_size==0
    // At this level we're may verify that we got everything and process DATA
    if (posthandle->len != contentlen) {
        errMessage = jsonNewMessage (AJG_FATAL, "Post Data Incomplete UID=%d Len %d != %s", posthandle->uid, contentlen, posthandle->len);
        goto ExitOnError;
    }

    // Before processing data, make sure buffer string is properly ended
    posthandle->data[posthandle->len] = '\0';
    request.data = posthandle->data;

    if (verbose == 0) fprintf (stderr, "Post Data Buffer=%s UID=%d\n", request.data, posthandle->uid);

  // process GET method and ignore any other
  } else if (strcmp (method, MHD_HTTP_METHOD_GET) != 0) {
       errMessage = jsonNewMessage (AJG_FATAL, "Not a POST/GET method=%s", method);
       goto ExitOnError;

  }

  // extract request query attribute from URL through ApiCmd
  query = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "request");
  if (query == NULL) {
    errMessage = jsonNewMessage (AJG_FATAL, "Invalid AJG REST request &request=xxxxx& missing ");
    goto ExitOnError;
  }
  // extract command value from json object and process it
  (void) json_object_object_get_ex (Request2Commands, query, &cmd);

  request.cardid = NULL; // no default card
  request.cardid = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "cardid");

  param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "quiet");
  if (param && ! sscanf (param, "%d", &request.quiet)) {
    errMessage = jsonNewMessage (AJG_FATAL, "Query=%s Quiet not integer &quiet=%s&", query, param);
    goto ExitOnError;
  }

  request.numid = -1;  // no default
  param = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "numid");
  if (param && ! sscanf (param, "%d", &request.numid)) {
    errMessage = jsonNewMessage (AJG_FATAL, "Query=%s NumID not integer &numid=%s&", query, param);
    goto ExitOnError;
  }


  switch (json_object_get_int(cmd)) {


  	case GATEWAY_PING: // http://localhost:1234/jsonapi?request=ping-get [&sndcard=0]
  	    if (verbose) fprintf (stderr, "%d: alsajson GATEWAY_PING\n", rqtcount);

        if (request.cardid == NULL)  jsonResponse = gatewayPing ();
        else jsonResponse = alsaProbeCard (session, &request);
  	    break;

  	case CARD_GET_ALL: // http://localhost:1234/jsonapi?request=card-get-all
  	    if (verbose)  fprintf (stderr, "%d: alsajson CARD_GET_ALL\n", rqtcount ++);
  	    jsonResponse = alsaFindCard (session, &request); // sndcard = -1
  	    break;

  	case CARD_GET_ONE: // http://localhost:1234/jsonapi?request=card-get-one&sndcard=0
  	    if (verbose)  fprintf (stderr, "%d: alsajson CARD_GET_ONE cardid=%s\n", rqtcount ++, request.cardid );
   	    if (request.cardid == NULL) {
            errMessage = jsonNewMessage (AJG_FAIL, "CARD_GET_ONE Query=%s Missing &SndCard=xxxx&\n", query);
   	        goto ExitOnError;
   	    }
  	    jsonResponse = alsaFindCard (session, &request);
  	    break;

  	case CTRL_GET_ALL: // http://localhost:1234/jsonapi?request=ctrl-get-all&sndcard=0
  	    if (verbose)  fprintf (stderr, "%d: alsajson CTRL_GET_ALL\n", rqtcount ++);
  	    request.numid = -1; // force list-all
  	    jsonResponse = alsaGetControl (session, &request);  // numid == -1
  	    break;

  	case CTRL_GET_ONE: // http://localhost:1234/jsonapi?request=ctrl-get-one&cardid=hw:0&numid=5&quiet=0
  	    if (verbose)  fprintf (stderr, "%d: alsajson CTRL_GET_ONE cardid=%s numid=%d\n", rqtcount ++, request.cardid ,request.numid);
        jsonResponse = alsaGetControl (session, &request);
 	    break;

  	case CTRL_SET_ONE: {// http://localhost:1234/jsonapi?request=ctrl-set-one&cardid=hw:0&quiet=1&numid=128&value=10,5
  	    if (verbose)  fprintf (stderr, "%d: alsajson processing CTRL_SET_ONE cardid=%s numid=%d args=%s\n", rqtcount ++, request.cardid ,request.numid, request.args);
        request.args   = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "value");
        jsonResponse = alsaSetOneCtrl (session, &request);
 	    break;
 	    }

  	case CTRL_SET_MANY: {// http://localhost:1234/jsonapi?request=ctrl-set-one&cardid=hw:0&quiet=1&numids=10,12,13&args=10
 	    // if data where not found in POST try to get them from GET [do not forget URL size constrains]
        if (request.data == NULL) request.data = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "numids");
        request.args   = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "value");

  	    if (verbose)  fprintf (stderr, "%d: alsajson processing CTRL_SET_MANY cardid=%s numids=%s value=%s\n", rqtcount ++, request.cardid ,request.data, request.args);

        jsonResponse = alsaSetManyCtrl (session, &request);
 	    break;
 	    }

    case SESSION_LIST:  {// http://localhost:1234/jsonapi?request=session-list&sndcard=0
       jsonResponse = alsaListSession (session, &request); // list session for requested sndcard
       break;
    }

  	case SESSION_LOAD: { // http://localhost:1234/jsonapi?request=session-load&cardid=hw:0&args=sessionname

       request.args   = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "session");
       if (verbose)  fprintf (stderr, "%d: alsajson SESSION_LOAD cardid=%s session=%s\n", rqtcount ++, request.cardid, request.args);

       jsonResponse = alsaLoadSession (session, &request);  // push session to alsa board

       break;
   	}

  	case SESSION_STORE: {// http://localhost:1234/jsonapi?request=session-store&cardid=hw:0&session=sessionname

       request.args   = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "session");

  	   // if data where not found in POST try to get them from GET [do not forget URL size constrains]
   	   if (request.data == NULL) request.data = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "info");

       if (verbose)  fprintf (stderr, "%d: alsajson SESSION_STORE cardid=%s session=%s\n", rqtcount ++, request.cardid, request.args );
       jsonResponse = alsaStoreSession (session, &request);  // push session to alsa board
       break;
   	}

  	default:
       errMessage = jsonNewMessage (AJG_FAIL, "%d:unknown Request=%s Cardid=%s NumId=%d\n", rqtcount ++, query, request.cardid ,request.numid);
  	   goto ExitOnError;
   }

   // send response to client with a http AJG_SUCCESS status code
   // [note we need to copy serialize object because libmicrohttpd does not provide adequate free callback
   if (jsonResponse == NULL) {
       errMessage = jsonNewMessage (AJG_FATAL,"Request:%d Query=%s SndCard=%s NumId=%d Response=>NULL [please report bug]\n", rqtcount ++, query, request.cardid ,request.numid);
       goto ExitOnError;
   }

   serialized = json_object_to_json_string(jsonResponse);
   response = MHD_create_response_from_buffer (strlen (serialized), (void*)serialized, MHD_RESPMEM_MUST_COPY);

   ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
   MHD_destroy_response (response);
   json_object_put (jsonResponse); // decrease reference rqtcount to free the json object
   if (request.cardname) free (request.cardname); // cardname need to be free
   return ret;

ExitOnError:
   serialized = json_object_to_json_string(errMessage);
   response = MHD_create_response_from_buffer (strlen (serialized), (void*)serialized, MHD_RESPMEM_MUST_COPY);
   ret = MHD_queue_response (connection, MHD_HTTP_BAD_REQUEST, response);
   MHD_destroy_response (response);
   json_object_put (errMessage); // decrease reference rqtcount to free the json object
   return ret;
}

// Create check etag value
STATIC void computeEtag (char *etag, int maxlen, struct stat *sbuf) {
   int time;

   time = sbuf->st_mtim.tv_sec;
   snprintf (etag, maxlen, "%d", time);
}

// minimal httpd file server for static HTML,JS,CSS,etc...
STATIC int requestFile (struct MHD_Connection *connection, AJG_session *session, const char* url) {
    int fd;
    int ret;
    struct stat sbuf;
    struct MHD_Response  *response;
    char filepath [521];

    // build full path from rootdir + url
    strncpy (filepath, session->config->rootdir, sizeof (filepath));
    strncat (filepath, url, sizeof (filepath));

    // try to open file and get its size
    if ( (-1 == (fd = open (filepath, O_RDONLY))) || (0 != fstat (fd, &sbuf)) ) {

          fprintf (stderr, "Fail to open file: [%s] error:%s\n", filepath, strerror(errno));

          // file is not accessible
          if (fd != -1) close (fd);
          const char *errorstr = "<html><body>Alsa-Json-Gateway Unknown or Not readable file</body></html>";
          response = MHD_create_response_from_buffer (strlen (errorstr),
                         (void *) errorstr,	 MHD_RESPMEM_PERSISTENT);
          ret = MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
          ret= MHD_YES;


    } else { // open file is OK

        // if url is a directory let's add index.html and redirect client
        if (S_ISDIR (sbuf.st_mode)) {
            strncpy (filepath, url, sizeof (filepath));



            if (url [strlen (url) -1] != '/') strncat (filepath, "/", sizeof (filepath));
            strncat (filepath, "index.html", sizeof (filepath));
            close (fd);
            response = MHD_create_response_from_buffer (0,"", MHD_RESPMEM_PERSISTENT);
            MHD_add_response_header (response,MHD_HTTP_HEADER_LOCATION, filepath);
            ret = MHD_queue_response (connection, MHD_HTTP_MOVED_PERMANENTLY, response);

        } else  if (! S_ISREG (sbuf.st_mode)) { // only standard file any other one including symbolic links are refused.

            fprintf (stderr, "Fail file: [%s] is not a regular file\n", filepath);
            const char *errorstr = "<html><body>Alsa-Json-Gateway Invalid file type</body></html>";
            response = MHD_create_response_from_buffer (strlen (errorstr),
                         (void *) errorstr,	 MHD_RESPMEM_PERSISTENT);
            ret = MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);

        } else {  // we're facing a regular file, check ETAG and if needed send it
            const char *etagCache;
            char etagValue[15];

            // https://developers.google.com/web/fundamentals/performance/optimizing-content-efficiency/http-caching?hl=fr
            // ftp://ftp.heanet.ie/disk1/www.gnu.org/software/libmicrohttpd/doxygen/dc/d0c/microhttpd_8h.html

            // Check etag value and load file only when modification date changes
            etagCache = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_NONE_MATCH);
            computeEtag (etagValue, sizeof (etagValue), &sbuf);

            if (etagCache != NULL && strcmp (etagValue, etagCache) == 0) {

                close (fd); // file did not change since last upload
                if (verbose) fprintf (stderr, "Not Modify: [%s]\n", filepath);
                response = MHD_create_response_from_buffer (0,"", MHD_RESPMEM_PERSISTENT);
                MHD_add_response_header (response,MHD_HTTP_HEADER_CACHE_CONTROL, session->cacheTimeout); // default one hour cache
                MHD_add_response_header (response,MHD_HTTP_HEADER_ETAG, etagValue);
                ret = MHD_queue_response (connection, MHD_HTTP_NOT_MODIFIED, response);

            } else {  // it's a new file, we need to upload it to client
                if (verbose) fprintf (stderr, "Serving: [%s]\n", filepath);
                response =  MHD_create_response_from_fd (sbuf.st_size, fd);
                MHD_add_response_header (response,MHD_HTTP_HEADER_CACHE_CONTROL,session->cacheTimeout); // default one hour cache
                MHD_add_response_header (response,MHD_HTTP_HEADER_ETAG, etagValue);
                ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
            }
        }
    }
    MHD_destroy_response (response);
    return ret;
}

STATIC int newRequest (void *cls,
  struct MHD_Connection *connection,
  const char *url,
  const char *method,
  const char *version,
  const char *upload_data, size_t *upload_data_size, void **con_cls) {

  AJG_session *session = cls;
  int ret;

  if (0 == strcmp (url, "/jsonapi")) {
       ret = requestApi (connection, session, method, url, upload_data, upload_data_size, con_cls);
  } else {
      if (0 != strcmp (method, MHD_HTTP_METHOD_GET)) return MHD_NO;   /* unexpected method */
      ret = requestFile (connection, session, url);
  }

  return ret;
}


STATIC int newClient (void *cls, const struct sockaddr * addr, socklen_t addrlen) {
  // check if client is comming from an acceptable IP
  return (MHD_YES); // MHD_NO
}

PUBLIC AJG_ERROR httpdStart (AJG_session *session) {

  // at 1st call initialise http api hashtable
  if (Request2Commands == NULL) initService (session);

  if (verbose) {
      printf ("AJG:notice Waiting port=%d rootdir=%s\n", session->config->httpdPort, session->config->rootdir);
      printf ("AJG:notice Browser URL= http://localhost:%d\n", session->config->httpdPort);
  }

  session->httpd = (void*) MHD_start_daemon (
			MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, // use request and not threads
            session->config->httpdPort,   // port
            &newClient, NULL,       // Tcp Accept call back + extra attribute
            &newRequest, session,  // Http Request Call back + extra attribute
            MHD_OPTION_NOTIFY_COMPLETED, &endRequest, NULL,
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 15, MHD_OPTION_END); // 15s + options-end
			// TBD: MHD_OPTION_SOCK_ADDR

  if (session->httpd == NULL) {
     printf ("Error: httpStart invalid httpd port: %d", session->config->httpdPort);
     return AJG_FATAL;
  }
  return AJG_SUCCESS;
}

// infinit loop
PUBLIC AJG_ERROR httpdLoop (AJG_session *session) {
    static int count =0;

    initService(session); // initialise http static data

    if (verbose) fprintf (stderr, "AJG:notice entering httpd waiting loop\n");
    if (session->foreground) {

        while (TRUE)  {
            fprintf (stderr, "AJG:notice Use Ctrl-C to quit");
            (void)getc (stdin);
        }
    } else {
        while (TRUE) {
           sleep (3600);
           if (verbose) fprintf (stderr, "AJG:notice httpd alive [%d]\n", count++);
        }
    }

    // should never return from here
    return AJG_FATAL;
}


PUBLIC int httpdStatus (AJG_session *session) {
    return (MHD_run (session->httpd));
}

PUBLIC void httpdStop (AJG_session *session) {
  MHD_stop_daemon (session->httpd);
}
