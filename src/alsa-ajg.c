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
   http://alsa-lib.sourcearchive.com/documentation/1.0.20/modules.html
   http://alsa-lib.sourcearchive.com/documentation/1.0.8/group__Control_gd48d44da8e3bfe150e928267008b8ff5.html
   http://alsa.opensrc.org/HowTo_access_a_mixer_control
   https://github.com/json-c/json-c
   https://linuxprograms.wordpress.com/2010/05/20/json-c-libjson-tutorial/

   $Id: $
*/

#include "local-def-ajg.h"
#include <alsa/asoundlib.h>

#define AJG_ALSACTL_JTYPE "AJG_ctrls"
#define AJG_SNDCARD_JTYPE "AJG_sndcard"
#define AJG_SNDLIST_JTYPE "AJG_sndlist"

// retreive info for one given card
PUBLIC json_object * alsaProbeCard (AJG_session *session, AJG_request *request) {
      char  *info, *name, *uid;
      json_object *sndcard;
      char cardName[32];
      snd_ctl_t   *handle;
      snd_ctl_card_info_t *cardinfo;
      snd_ctl_card_info_alloca(&cardinfo);
      int err, index;

	  // not really clean, but this is how aplay search for ALSA boards
	  sprintf(cardName, "hw:%d", request->sndcard);

	  if ((err = snd_ctl_open(&handle, cardName, 0)) < 0) {
		 return  jsonNewMessage (AJG_FATAL, "Control device %s hw not found: %s", cardName, snd_strerror(err));
	  }

	  if ((err = snd_ctl_card_info(handle, cardinfo)) < 0) {
		  snd_ctl_close(handle);
		  return  jsonNewMessage (AJG_FAIL, "Control device %s hw info error: %s", cardName, snd_strerror(err));
	  }

	  sndcard = json_object_new_object();
      json_object_object_add (sndcard, "ajgtype" ,json_object_new_string (AJG_SNDCARD_JTYPE));
	  uid = strdup(cardName);
      json_object_object_add (sndcard, "uid"     , json_object_new_string (uid));
      name = strdup(snd_ctl_card_info_get_name (cardinfo));
      request->cardname = name; // save cardname for session management
	  json_object_object_add (sndcard, "name"    , json_object_new_string (name));

	  uid   = strdup(cardName);

	  if (!request->quiet) {
		  info  = strdup(snd_ctl_card_info_get_longname (cardinfo));
          json_object_object_add (sndcard, "info" , json_object_new_string (info));
   	      if (verbose) fprintf (stderr, "AJG: Sound Card uid=%s name=%s\n", uid, info);
	  }

      // if requested keep track of sound probed handle
      if (request->cardhandle) request->cardhandle = handle;
      else snd_ctl_close(handle);
	  return (sndcard);
}

// Loop on everypotential Sound card and register active one
PUBLIC json_object * alsaFindCard (AJG_session *session, AJG_request *request) {
	int  card;
	json_object *sndcards, *element, *ajgResponse;

	// if no specific card requested loop on all
	if (request->sndcard < 0) {
	     // return an array of sndcard
		 sndcards =json_object_new_array();

		 // loop on potential card number
		 for (card =0; card < 32; card++) {
			json_object *sndcard;

			// probe card from its number
			request->sndcard = card;
			sndcard = alsaProbeCard (session, request);

			// If card probe fail to return UID check for status
			if (!json_object_object_get_ex (sndcard, "uid", &element)) {
				char *status;
				json_object_object_get_ex (sndcard, "status", &element);
				if (!strcmp (status, ERROR_LABEL[AJG_FATAL]))  break;
				continue; // just ignore this entry
			}
			// add current sndcard to sndcards object
			json_object_array_add (sndcards, sndcard);
	   	 }

	 } else {
		 // only one card was requested let's probe it
		 sndcards = alsaProbeCard (session, request);
		 // If card probe fail to return UID check for status
		 if (!json_object_object_get_ex (sndcards, "uid", &element)) {
			return (sndcards);
		 }
	}

    ajgResponse = json_object_new_object();
    json_object_object_add (ajgResponse, "ajgtype" , json_object_new_string (AJG_SNDLIST_JTYPE));
    json_object_object_add (ajgResponse, "status"  , jsonNewError(AJG_SUCCESS));
    json_object_object_add (ajgResponse, "data"    , sndcards);

   return (ajgResponse);
}

STATIC json_object *DB2StringJsonOject (long dB) {
    char label [20];
	if (dB < 0) {
		snprintf(label, sizeof(label), "-%li.%02lidB", -dB / 100, -dB % 100);
	} else {
		snprintf(label, sizeof(label), "%li.%02lidB", dB / 100, dB % 100);
	}

    // json function takes care of string copy
	return (json_object_new_string (label));
}


// Direct port from amixer TLV decode routine. This code is too complex for me.
// I hopefully did not break it when porting it.
STATIC json_object *decodeTlv(unsigned int *tlv, unsigned int tlv_size) {
    char label[20];
	unsigned int type = tlv[0];
	unsigned int size;
	unsigned int idx = 0;
	const char *chmap_type = NULL;
	json_object * decodeTlvJson =json_object_new_object();

	if (tlv_size < 2 * sizeof(unsigned int)) {
		printf("TLV size error!\n");
		return NULL;
	}
	type = tlv[idx++];
	size = tlv[idx++];
	tlv_size -= 2 * sizeof(unsigned int);
	if (size > tlv_size) {
		fprintf(stderr, "TLV size error (%i, %i, %i)!\n", type, size, tlv_size);
		return NULL;
	}
	switch (type) {

	case SND_CTL_TLVT_CONTAINER: {
        json_object * containerJson = json_object_new_array();

		size += sizeof(unsigned int) -1;
		size /= sizeof(unsigned int);
		while (idx < size) {
    		json_object *embedJson;

			if (tlv[idx+1] > (size - idx) * sizeof(unsigned int)) {
				fprintf(stderr, "TLV size error in compound!\n");
				return NULL;
			}
			embedJson = decodeTlv(tlv + idx, tlv[idx+1] + 8);
			json_object_array_add  (containerJson, embedJson);
			idx += 2 + (tlv[idx+1] + sizeof(unsigned int) - 1) / sizeof(unsigned int);
		}
		json_object_object_add (decodeTlvJson, "container",  containerJson);
		break;
		}

	case SND_CTL_TLVT_DB_SCALE: {
	    json_object * dbscaleJson = json_object_new_object();

		if (size != 2 * sizeof(unsigned int)) {
		    json_object * arrayJson =  json_object_new_array();
			while (size > 0) {
				snprintf(label, sizeof(label), "0x%08x,", tlv[idx++]);
    			json_object_array_add (arrayJson, json_object_new_string (label));
				size -= sizeof(unsigned int);
			}
			json_object_object_add (dbscaleJson, "array", arrayJson);
		} else {
			json_object_object_add (dbscaleJson, "min",  DB2StringJsonOject ((int)tlv[2]));
			json_object_object_add (dbscaleJson, "step", DB2StringJsonOject (tlv[3] & 0xffff));
			json_object_object_add (dbscaleJson, "mute", DB2StringJsonOject ((tlv[3] >> 16) & 1));
		}
		json_object_object_add (decodeTlvJson, "dbscale",  dbscaleJson);
		break;
		}

#ifdef SND_CTL_TLVT_DB_LINEAR
	case SND_CTL_TLVT_DB_LINEAR: {
	    json_object * dbLinearJson = json_object_new_object();

		if (size != 2 * sizeof(unsigned int)) {
		    json_object * arrayJson =  json_object_new_array();
			while (size > 0) {
				snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
    			json_object_array_add (arrayJson, json_object_new_string (label));
				size -= sizeof(unsigned int);
			}
			json_object_object_add (dbLinearJson, "offset", arrayJson);
		} else {
			json_object_object_add (dbLinearJson, "min", DB2StringJsonOject((int)tlv[2]));
			json_object_object_add (dbLinearJson, "max", DB2StringJsonOject((int)tlv[3]));
		}
		json_object_object_add (decodeTlvJson, "dblinear",  dbLinearJson);
		break;
		}
#endif

#ifdef SND_CTL_TLVT_DB_RANGE
	case SND_CTL_TLVT_DB_RANGE: {
        json_object *dbRangeJson = json_object_new_object();

		if ((size % (6 * sizeof(unsigned int))) != 0) {
			json_object *arrayJson   = json_object_new_array();
			while (size > 0) {
				snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
    			json_object_array_add (arrayJson, json_object_new_string (label));
				size -= sizeof(unsigned int);
			}
    		json_object_object_add (dbRangeJson, "dbrange", arrayJson);
			break;
		}
		while (size > 0) {
   		    json_object * embedJson = json_object_new_object();
			snprintf(label, sizeof(label),"%i,", tlv[idx++]);
			json_object_object_add (embedJson, "rangemin", json_object_new_string (label));
			snprintf(label, sizeof(label),"%i", tlv[idx++]);
			json_object_object_add (embedJson, "rangemax", json_object_new_string (label));
			embedJson = decodeTlv(tlv + idx, 4 * sizeof(unsigned int));
			json_object_object_add (embedJson, "tlv", embedJson);
			idx += 4;
			size -= 6 * sizeof(unsigned int);
			json_object_array_add (dbRangeJson, embedJson);
		}
		json_object_object_add (decodeTlvJson, "dbrange",  dbRangeJson);
		break;
		}
#endif
#ifdef SND_CTL_TLVT_DB_MINMAX
	case SND_CTL_TLVT_DB_MINMAX:
	case SND_CTL_TLVT_DB_MINMAX_MUTE: {
	    json_object * dbMinMaxJson = json_object_new_object();

        if (size != 2 * sizeof(unsigned int)) {
            json_object * arrayJson = json_object_new_array();
            while (size > 0) {
                snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
                json_object_array_add (arrayJson, json_object_new_string (label));
                size -= sizeof(unsigned int);
            }
            json_object_object_add (dbMinMaxJson, "array", arrayJson);

        } else {
            json_object_object_add (dbMinMaxJson, "min", DB2StringJsonOject((int)tlv[2]));
            json_object_object_add (dbMinMaxJson, "max", DB2StringJsonOject((int)tlv[3]));
        }

        if (type == SND_CTL_TLVT_DB_MINMAX_MUTE) {
          json_object_object_add (decodeTlvJson ,"dbminmaxmute", dbMinMaxJson);
        } else {
          json_object_object_add (decodeTlvJson ,"dbminmax", dbMinMaxJson);
        }
		break;
		}
#endif
#ifdef SND_CTL_TLVT_CHMAP_FIXED
	case SND_CTL_TLVT_CHMAP_FIXED:
		chmap_type = "fixed";
		/* Fall through */
	case SND_CTL_TLVT_CHMAP_VAR:
		if (!chmap_type)
			chmap_type = "variable";
		/* Fall through */
	case SND_CTL_TLVT_CHMAP_PAIRED:
		if (!chmap_type)
			chmap_type = "paired";

	    json_object * chmapJson = json_object_new_object();
        json_object * arrayJson = json_object_new_array();

		while (size > 0) {
			snprintf(label, sizeof(label),"%s", snd_pcm_chmap_name(tlv[idx++]));
			size -= sizeof(unsigned int);
			json_object_array_add (arrayJson, json_object_new_string (label));
		}
		json_object_object_add (chmapJson,chmap_type, arrayJson);
		json_object_object_add (decodeTlvJson ,"chmap", chmapJson);
		break;
#endif
	default: {
		printf("unk-%i-", type);
        json_object * arrayJson = json_object_new_array();

		while (size > 0) {
			snprintf(label, sizeof(label),"0x%08x,", tlv[idx++]);
			size -= sizeof(unsigned int);
			json_object_array_add (arrayJson, json_object_new_string (label));
		}
		break;
		json_object_object_add (decodeTlvJson ,"unknown", arrayJson);
		}
	}

	return (decodeTlvJson);
}

// pack Alsa element's ACL into a JSON object
STATIC json_object *  getControlAcl (snd_ctl_elem_info_t *info) {

    json_object * jsonAclCtrl = json_object_new_object();

    json_object_object_add (jsonAclCtrl, "read"   , json_object_new_boolean(snd_ctl_elem_info_is_readable(info)));
    json_object_object_add (jsonAclCtrl, "write"  , json_object_new_boolean(snd_ctl_elem_info_is_writable(info)));
    json_object_object_add (jsonAclCtrl, "inact"  , json_object_new_boolean(snd_ctl_elem_info_is_inactive(info)));
    json_object_object_add (jsonAclCtrl, "volat"  , json_object_new_boolean(snd_ctl_elem_info_is_volatile(info)));
    json_object_object_add (jsonAclCtrl, "lock"   , json_object_new_boolean(snd_ctl_elem_info_is_locked(info)));

    // if TLV is readable we insert its ACL
    if (!snd_ctl_elem_info_is_tlv_readable(info)) {
        json_object * jsonTlv = json_object_new_object();

        json_object_object_add (jsonTlv, "read"   , json_object_new_boolean(snd_ctl_elem_info_is_tlv_readable(info)));
        json_object_object_add (jsonTlv, "write"  , json_object_new_boolean(snd_ctl_elem_info_is_tlv_writable(info)));
        json_object_object_add (jsonTlv, "command", json_object_new_boolean(snd_ctl_elem_info_is_tlv_commandable(info)));

        json_object_object_add (jsonAclCtrl, "tlv", jsonTlv);
    }
    return (jsonAclCtrl);
}


// pack element from ALSA control into a JSON object
STATIC json_object * getAlsaControl (snd_hctl_elem_t *elem, snd_ctl_elem_info_t *info,  AJG_request *request) {

	int err;
	unsigned int *tlv;
    json_object *jsonAlsaCtrl,*jsonClassCtrl;
	snd_ctl_elem_id_t *elemid;
	snd_ctl_elem_type_t elemtype;
	snd_ctl_elem_value_t *control;
	snd_ctl_elem_value_alloca(&control);

	int count, idx;

	// allocate ram for ALSA elements
	snd_ctl_elem_id_alloca   (&elemid);

    // get elemId out of elem
    snd_hctl_elem_get_id(elem, elemid);

    // when ctrlid is set, return only this ctrl
    if (request->numid != -1 && request->numid != snd_ctl_elem_id_get_numid(elemid)) return NULL;

    // build a json object out of element
    jsonAlsaCtrl = json_object_new_object(); // http://alsa-lib.sourcearchive.com/documentation/1.0.24.1-3/group__Control_ga4e4f251147f558bc2ad044e836e449d9.html
    json_object_object_add (jsonAlsaCtrl,"numid", json_object_new_int(snd_ctl_elem_id_get_numid (elemid)));
    if (request->quiet < 2) json_object_object_add (jsonAlsaCtrl,"name" , json_object_new_string(snd_ctl_elem_id_get_name (elemid)));
    if (request->quiet < 1) json_object_object_add (jsonAlsaCtrl,"iface", json_object_new_string(snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(elemid))));
    if (request->quiet < 3)json_object_object_add (jsonAlsaCtrl,"actif", json_object_new_boolean(!snd_ctl_elem_info_is_inactive(info)));

    elemtype = snd_ctl_elem_info_get_type(info);

	// number item and value(s) within control.
	count = snd_ctl_elem_info_get_count (info);

	if (snd_ctl_elem_info_is_readable(info)) {
	    json_object *jsonValuesCtrl = json_object_new_array();

		if ((err = snd_hctl_elem_read(elem, control)) < 0) {
		       json_object *jsonValuesCtrl = json_object_new_object();
       		   json_object_object_add (jsonValuesCtrl,"error", json_object_new_string(snd_strerror(err)));
		} else {
			for (idx = 0; idx < count; idx++) { // start from one in amixer.c !!!
				switch (elemtype) {
				case SND_CTL_ELEM_TYPE_BOOLEAN: {
					char *onoff;
					json_object_array_add (jsonValuesCtrl, json_object_new_boolean (snd_ctl_elem_value_get_boolean(control, idx)));
					break;
					}
				case SND_CTL_ELEM_TYPE_INTEGER:
					json_object_array_add (jsonValuesCtrl, json_object_new_int (snd_ctl_elem_value_get_integer(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_INTEGER64:
					json_object_array_add (jsonValuesCtrl, json_object_new_int64 (snd_ctl_elem_value_get_integer64(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_ENUMERATED:
					json_object_array_add (jsonValuesCtrl, json_object_new_int (snd_ctl_elem_value_get_enumerated(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_BYTES:
					json_object_array_add (jsonValuesCtrl, json_object_new_int ((int)snd_ctl_elem_value_get_byte(control, idx)));
					break;
				case SND_CTL_ELEM_TYPE_IEC958: {
					json_object *jsonIec958Ctrl = json_object_new_object();
					snd_aes_iec958_t iec958;
					snd_ctl_elem_value_get_iec958(control, &iec958);

					json_object_object_add (jsonIec958Ctrl,"AES0",json_object_new_int(iec958.status[0]));
					json_object_object_add (jsonIec958Ctrl,"AES1",json_object_new_int(iec958.status[1]));
					json_object_object_add (jsonIec958Ctrl,"AES2",json_object_new_int(iec958.status[2]));
					json_object_object_add (jsonIec958Ctrl,"AES3",json_object_new_int(iec958.status[3]));
					json_object_array_add  (jsonValuesCtrl, jsonIec958Ctrl);
					break;
					}
				default:
					json_object_array_add (jsonValuesCtrl, json_object_new_string ("?unknown?"));
					break;
				}
			}
		}
		json_object_object_add (jsonAlsaCtrl,"value",jsonValuesCtrl);
    }


    if (!request->quiet) {  // in simple mode do not print usable values
        jsonClassCtrl = json_object_new_object();
        json_object_object_add (jsonClassCtrl,"type" , json_object_new_string(snd_ctl_elem_type_name(elemtype)));
		json_object_object_add (jsonClassCtrl,"count", json_object_new_int(count));

		switch (elemtype) {
			case SND_CTL_ELEM_TYPE_INTEGER:
				json_object_object_add (jsonClassCtrl,"min",  json_object_new_int(snd_ctl_elem_info_get_min(info)));
				json_object_object_add (jsonClassCtrl,"max",  json_object_new_int(snd_ctl_elem_info_get_max(info)));
				json_object_object_add (jsonClassCtrl,"step", json_object_new_int(snd_ctl_elem_info_get_step(info)));
				break;
			case SND_CTL_ELEM_TYPE_INTEGER64:
				json_object_object_add (jsonClassCtrl,"min",  json_object_new_int64(snd_ctl_elem_info_get_min64(info)));
				json_object_object_add (jsonClassCtrl,"max",  json_object_new_int64(snd_ctl_elem_info_get_max64(info)));
				json_object_object_add (jsonClassCtrl,"step", json_object_new_int64(snd_ctl_elem_info_get_step64(info)));
				break;
			case SND_CTL_ELEM_TYPE_ENUMERATED: {
				unsigned int item, items = snd_ctl_elem_info_get_items(info);
				json_object *jsonEnum = json_object_new_array();

				for (item = 0; item < items; item++) {
					snd_ctl_elem_info_set_item(info, item);
					if ((err = snd_hctl_elem_info(elem, info)) >= 0) {
						json_object_array_add (jsonEnum, json_object_new_string(snd_ctl_elem_info_get_item_name(info)));
					}
				}
				json_object_object_add (jsonClassCtrl, "enums",jsonEnum);
				break;
			}
			default: break; // ignore any unknown type
			}

		// add collected class info with associated ACLs
		json_object_object_add (jsonAlsaCtrl,"ctrl", jsonClassCtrl);
		json_object_object_add (jsonAlsaCtrl,"acl"  , getControlAcl (info));

		// check for tlv [direct port from amixer.c]
		if (snd_ctl_elem_info_is_tlv_readable(info)) {
				unsigned int *tlv;
				tlv = malloc(4096);
				if ((err = snd_hctl_elem_tlv_read(elem, tlv, 4096)) < 0) {
					error("Control %s element TLV read error\n", snd_strerror(err));
					free(tlv);
				} else {
					json_object_object_add (jsonAlsaCtrl,"tlv", decodeTlv (tlv, 4096));
		   }
		}
   }
   return (jsonAlsaCtrl);
}

PUBLIC json_object *alsaGetControl (AJG_session *session, AJG_request *request) {
	int err;
    int  count=0;
    char uidcard [6];

    snd_ctl_elem_iface_t iface;

	snd_hctl_t *handle;
	snd_hctl_elem_t *elem;
	snd_ctl_elem_info_t *info;
	json_object *jsonsndcard, *jsonAlsaCtrls, *jsonAlsaCtrl, *jsonAclCtrl, *slot;

	// allocate ram for ALSA elements
	snd_ctl_elem_info_alloca (&info);

    // retrieve sound card uid from request
    snprintf (uidcard, sizeof(uidcard),"hw:%i",request->sndcard);

	// main json structure for our sound card
	jsonsndcard = json_object_new_object();

	// add card description to control
	if (request->quiet < 2) {
	    json_object *sndcard;
   	    sndcard =  alsaProbeCard (session, request);
    	json_object_object_add (jsonsndcard,"sndcard",  sndcard);
	}

	if ((err = snd_hctl_open(&handle, uidcard, 0)) < 0) {
		return jsonNewMessage (AJG_FAIL,"Control %s open error: %s", uidcard, snd_strerror(err));
	}
	if ((err = snd_hctl_load(handle)) < 0) {
		return jsonNewMessage (AJG_FAIL,"Control %s local error: %s\n", uidcard, snd_strerror(err));
	}

	// create an json array to hold all sndcard controls
    jsonAlsaCtrls = json_object_new_array();

	for (elem = snd_hctl_first_elem(handle); elem != NULL; elem = snd_hctl_elem_next(elem)) {

        if ((err = snd_hctl_elem_info(elem, info)) < 0) {
			error("Control %s snd_hctl_elem_info error: %s\n", uidcard, snd_strerror(err));
			return jsonNewMessage (AJG_FATAL,"Control %s snd_hctl_elem_info error: %s\n", uidcard, snd_strerror(err));
		}

        // each control is added into a JSON array
		jsonAlsaCtrl = getAlsaControl (elem, info, request);
       	if (jsonAlsaCtrl) json_object_array_add (jsonAlsaCtrls, jsonAlsaCtrl);

	}

    // add controls json array to sndcard
    json_object_object_add (jsonsndcard,"ajgtype", json_object_new_string (AJG_ALSACTL_JTYPE));
	json_object_object_add (jsonsndcard,"controls", jsonAlsaCtrls);
	snd_hctl_close(handle);

   	// fprintf (stdout, "Json object: %s\n",json_object_to_json_string_ext(jsonsndcard, JSON_C_TO_STRING_PRETTY));
	return (json_object_get(jsonsndcard));
}

PUBLIC json_object *alsaSetControl (AJG_session *session, AJG_request *request) {
    static json_object * okresponse =NULL;
	int err;
	snd_ctl_t *handle;
	snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
    json_object *response, *slot;
    char uidcard [6];

    // make standard response only once
    if (okresponse == NULL) okresponse=jsonNewMessage (AJG_SUCCESS, "done");

    // open sound card with low level API as the other seems not stable enough
    /* FIXME: Remove it when hctl find works ok !!! */
    snprintf (uidcard, sizeof(uidcard),"hw:%i",request->sndcard);
    if (err = snd_ctl_open(&handle, uidcard, 0) < 0) {
   		response= jsonNewMessage (AJG_FAIL,"Control %s open error: %s\n", uidcard, snd_strerror(err));
   		return response; // handle is invalid do not try to close it
    }

    if (request->args == NULL || request->numid < 0) {
    	response= jsonNewMessage (AJG_FAIL,"setcontrol Card=%s NumId=%d no values missing &numid=xx&args='values'\n", request->cardname, request->numid);
       	goto ExitNow;
    }

    /* FIXME: Remove it when hctl find works ok !!! */
   	snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_id_set_numid (id, request->numid);

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_info_set_id(info, id);
    if ((err = snd_ctl_elem_info(handle, info)) < 0) {
   		response= jsonNewMessage (AJG_FAIL,"Cannot find the given element from control %s\n", uidcard);
   		goto ExitNow;
    }

  	snd_ctl_elem_info_get_id(info, id);
	snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, id);
	if ((err = snd_ctl_elem_read(handle, control)) < 0) {
   	    response= jsonNewMessage (AJG_FAIL,"Cannot read the given element from control %s\n", uidcard);
	    goto ExitNow;
	}

    err = snd_ctl_ascii_value_parse(handle, control, info, request->args);
  	if (err < 0) {
  	    response= jsonNewMessage (AJG_FAIL,"Control %s fail to parse args=%s: %s\n", uidcard, request->args, snd_strerror(err));
  	    goto ExitNow;
  	}

	if ((err = snd_ctl_elem_write(handle, control)) < 0) {
	   response= jsonNewMessage (AJG_FAIL,"Control %s element write error: %s\n", uidcard, snd_strerror(err));
  	   goto ExitNow;
    }

    // in quiet mode we only return OK otherwise we request full value of modified control
	if (request->quiet) {
        response = okresponse;
	} else {
	    // in verbose mode we return a modified controls
  	    request->quiet=1; // make response short
        response = alsaGetControl (session, request);
	}

ExitNow:
    snd_ctl_close(handle);
	return response;
}

// Low level push one control values to sound card.
// Warning: at this level we expect session file to be save and we write integer values without verification.
STATIC AJG_ERROR alsaSetOneControl(AJG_session *session, AJG_request *request, json_object *ctrlnumid, json_object *ctrlvalue) {
    static json_object *okresponse=NULL, *element;
	int err, numid, value;
	snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *myid;
	snd_ctl_elem_value_t *control;
	unsigned int index, count, length;

    // extract NUMid and loop on values
    numid = json_object_get_int (ctrlnumid);

   	snd_ctl_elem_id_alloca(&myid);
    snd_ctl_elem_id_set_numid (myid, numid);

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_info_set_id(info, myid);
    if ((err = snd_ctl_elem_info(request->cardhandle, info)) < 0) {
   		fprintf (stderr, "AJG: Fail numid=%2d unknown\n", numid);
   		return AJG_FAIL;
    }

   	if (!snd_ctl_elem_info_is_writable(info)) {
   	   if (verbose) fprintf (stderr,"AJG:info numid=%2d ignored [read only value]\n", numid);
   	   return (AJG_EMPTY);
   	}

  	snd_ctl_elem_info_get_id(info, myid);
	snd_ctl_elem_value_alloca(&control);
    snd_ctl_elem_value_set_id(control, myid);
	if ((err = snd_ctl_elem_read(request->cardhandle, control)) < 0) {
   		fprintf (stderr, "AJG: Fail numid=%2d control error\n", numid);
	    return AJG_FAIL;
	}

    // prepare structure to loop on pushing value to sound card
    index = 0; // taken from ctrlparse.c
   	count = snd_ctl_elem_info_get_count(info);
   	snd_ctl_elem_value_set_id(control, myid);
   	length = json_object_array_length (ctrlvalue);

    // Loop on every value and push control to sndcard
    for (index=0; index < count && index < length; index++) {
        element = json_object_array_get_idx(ctrlvalue, index);
        value= json_object_get_int (element);
        snd_ctl_elem_value_set_integer(control, index, value);
    }

    // write array on disk
    if ((err = snd_ctl_elem_write(request->cardhandle, control)) < 0) {
	   fprintf (stderr,"AJG: Fail numid=%2d values=%s write error: %s\n", numid, json_object_to_json_string(ctrlvalue),snd_strerror(err));
	   return AJG_FAIL;
	}

	return (AJG_SUCCESS);

}

// open sndcard to get its name and request existing session for this card
PUBLIC json_object *alsaListSession (AJG_session *session, AJG_request *request) {
   json_object *sndcard, *response, *element;
   const char *cardname;

   // open sndcard and get its name
   sndcard = alsaProbeCard (session, request);
   if (request->cardname == NULL) {
       return (jsonNewMessage (AJG_FATAL,"Sound card [hw:%d] has not 'name' element", request->sndcard));
   }

   response = sessionList (session, request);
   json_object_put (sndcard); // release sndcard json object
   return (response);
}

// load a session for requested card
PUBLIC json_object *alsaLoadSession (AJG_session *session, AJG_request *request) {
   json_object *errorMsg, *jsonSession, *sndcard, *element, *cardinfo;
   const char *sessionname,*cardname;
   unsigned int index;

   // probe soundcard to check it exist and get it name
   request->cardhandle = (void*)TRUE; // request for not closing card handle
   sndcard = alsaProbeCard (session, request);
   if (request->cardname == NULL) {
       errorMsg =  jsonNewMessage (AJG_FATAL,"Sound card [hw:%d] has no 'name' element", request->sndcard);
  	   goto OnErrorExit;
   }

   // request session from disk [response is a valid json error or a valid session]
   jsonSession = sessionFromDisk (session, request);

   // search for sound card descriptor
   if (!json_object_object_get_ex (jsonSession, "sndcard", &cardinfo)) {
   		errorMsg = jsonSession;  json_object_get (jsonSession); // increase jsonsession usecount to keep it as error message
   	    goto OnErrorExit;
   }

   // search for request->cardname with cardinfo descriptor
   if (!json_object_object_get_ex (cardinfo, "name", &element)) {
   		errorMsg =   jsonNewMessage (AJG_FATAL,"session [%s] sndcard block should have {name: %s} ", request->args, request->cardname);
   	    goto OnErrorExit;
   }
   sessionname = json_object_get_string (element);

   if (strcmp (sessionname, request->cardname)) {
	   errorMsg =  jsonNewMessage (AJG_FATAL,"session=[%s] [%s] != [%d]", request->args, sessionname, request->cardname);
	   goto OnErrorExit;
   }

   // let's get sound card controls out of session
   if (!json_object_object_get_ex (jsonSession, "controls", &cardinfo)) {
       errorMsg =  jsonNewMessage (AJG_FATAL,"session [%s/%s] fail to find 'controls' descriptor ", request->args, request->cardname);
	   goto OnErrorExit;
   }

    for (index=0; index < json_object_array_length (cardinfo); index++) {
        json_object *ctrlnumid, *ctrlvalue, *control;
        control = json_object_array_get_idx(cardinfo, index);

        // extract numid and values from sessions's controls
        json_object_object_get_ex (control, "numid", &ctrlnumid);
        json_object_object_get_ex (control, "value", &ctrlvalue);

        // if session content is broken let's write within log file
        if (ctrlnumid == NULL || ctrlvalue == NULL) {
           fprintf (stderr, "AJG:WARNING invalid session [%s] descriptor [%s]\n", request->args, json_object_to_json_string(control));
        }

        // apply control to sound card ignoring errors
        (void) alsaSetOneControl (session, request, ctrlnumid, ctrlvalue);
   }

   // we done let free jsonSession object
   json_object_put   (jsonSession);
   json_object_put   (sndcard);

   // if we have a valid card handle let's close it
   if (request->cardhandle != (void*)TRUE && request->cardhandle != NULL) snd_ctl_close(request->cardhandle);
   return NULL; // in this case No Error message translates to OK

OnErrorExit:
   if (jsonSession) json_object_put (jsonSession);
   if (sndcard) json_object_put (sndcard);

   // if we have a valid card handle let's close it
   if (request->cardhandle != (void*)TRUE && request->cardhandle != NULL) snd_ctl_close(request->cardhandle);
   return (errorMsg);
}

// load every control from the board in a very quiet mode to serialize on disk
PUBLIC json_object *alsaStoreSession (AJG_session *session, AJG_request *request) {
    json_object *controls, *response;

    request->quiet = 1;  // run quiet mode
    controls = alsaGetControl (session, request);

    if (request->cardname == NULL) {
       return jsonNewMessage (AJG_FATAL,"Sound card [hw:%d] no [name] element", request->sndcard);
    }
    response = sessionToDisk (session, request, controls);
    json_object_put   (controls);    // decrease reference count to free the json object
    return response;
}
