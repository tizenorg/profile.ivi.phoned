
#include "obex.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <fstream>

#include "Logger.h"

namespace PhoneD {

#define OBEX_PREFIX                        "org.bluez.obex"

#define OBEX_CLIENT_IFACE                  OBEX_PREFIX ".Client1"
#define OBEX_PHONEBOOK_IFACE               OBEX_PREFIX ".PhonebookAccess1"
#define OBEX_TRANSFER_IFACE                OBEX_PREFIX ".Transfer1"

// check for stalled active transfer (in seconds)
#define CHECK_STALLED_TRANSFER_TIMEOUT     120

/*! \class PhoneD::SyncPBData
 * A Class to provide a storage for Queued synchronization requests.
 */
class SyncPBData {
    public:
        /**
         * A default constructor which allows to specify object data in the construction phase.
         * @param[in] location Location of phonebook data, see SyncPBData::location.
         * @param[in] phonebook Phonebook data identification, see SyncPBData::phonebook.
         * @param[in] count Number of latest entries to be synchronized (the default is 0), see SyncPBData::count.
         */
        SyncPBData(const char *location, const char *phonebook, unsigned long count = 0)
        {
            this->location = location;
            this->phonebook = phonebook;
            this->count = count;
        }
    public:
        const char *location;   /*!< Location of phonebook data: "INT", "SIM1", "SIM2". */
        const char *phonebook;  /*!< Phonebook data identification: "pb", "ich", "och", "mch", "cch". */
        unsigned long count;    /*!< Number of latest entries to be synchronized (0 means to request all). */
};

Obex::Obex() :
    mSelectedRemoteDevice(""),
    mSession(NULL),
    mActiveTransfer(NULL)
{
    LoggerD("entered");
    mContacts.clear();
    mContactsOrder.clear();
    mCallHistory.clear();
    mCallHistoryOrder.clear();
}

Obex::~Obex() {
    LoggerD("entered");
    removeSession(false); // remove session if it's active
}

void Obex::createSession(const char *bt_address) {
    LoggerD("entered");

    // remove existing session if exists
    removeSession(false);

    GVariant *args[8];
    int nargs = 0;

    // add dict entry for "PBAP" target
    GVariant * key = g_variant_new_string("Target");
    GVariant * str = g_variant_new_string("PBAP");
    GVariant * var = g_variant_new_variant(str);
    args[nargs++] = g_variant_new_dict_entry(key, var);
    GVariant *array = g_variant_new_array(G_VARIANT_TYPE("{sv}"), args, nargs);

    // build the parameters variant
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(sa{sv})"));
    g_variant_builder_add_value(builder, g_variant_new("s", bt_address));
    g_variant_builder_add_value(builder, array);
    GVariant *parameters = g_variant_builder_end(builder);

    g_dbus_connection_call( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                            OBEX_PREFIX,
                            "/org/bluez/obex",
                            OBEX_CLIENT_IFACE,
                            "CreateSession",
                            parameters,
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            Obex::asyncCreateSessionReadyCallback,
                            this);
}

// callback for async call of "CreateSession" method
void Obex::asyncCreateSessionReadyCallback(GObject *source, GAsyncResult *result, gpointer user_data) {
    Obex *ctx = static_cast<Obex*>(user_data);
    if(!ctx) {
        LoggerE("Failed to cast object: Obex");
        return;
    }

    GError *err = NULL;
    GVariant *reply;
    reply = g_dbus_connection_call_finish(g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL), result, &err);
    if(err || !reply) {
        ctx->createSessionFailed(err?err->message:"Invalid reply from 'CreateSession'");
        if(err)
            g_error_free(err);
        return;
    }

    const char *session = NULL;
    g_variant_get(reply, "(o)", &session);
    LoggerD("Created session: " << session);

    // make a copy of object path, since it will be destroyed when the reply is unref-ed
    if(session) {
        ctx->mSession = strdup(session);
        ctx->createSessionDone(ctx->mSession);
    }
    else
        ctx->createSessionFailed("Failed to get 'session' from the 'CreateSession' reply");

    g_variant_unref(reply);
}

// location:  "INT", "SIM1", "SIM2"
// phonebook: "pb", "ich", "och", "mch", "cch"
bool Obex::select(const char *location, const char *phonebook) {
    LoggerD("Selecting phonebook: " << location << "/" << phonebook);

    if(!mSession) {
        LoggerE("No session to execute operation on");
        return false;
    }

    GError *err = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 OBEX_PREFIX,
                                 mSession,
                                 OBEX_PHONEBOOK_IFACE,
                                 "Select",
                                 g_variant_new("(ss)", location, phonebook), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &err);
    if(err) {
        LoggerE("Failed to select phonebook " << location << "/" << phonebook << ": " << err->message);
        g_error_free(err);
        return false;
    }

    return true;
}

void Obex::removeSession(bool notify) {
    if(!mSession) // there isn't active session to be removed
        return;

    LoggerD("Removing session:" << mSession);

    // delete/unref individual contacts
    for(auto it=mContacts.begin(); it!=mContacts.end(); ++it) {
        EContact *contact = (*it).second;
        if(contact) {
            // TODO: delete also all its attribs?
            g_object_unref(contact);
        }
    }
    mContacts.clear();
    mContactsOrder.clear();

    // delete/unref individual cll history entries
    for(auto it=mCallHistory.begin(); it!=mCallHistory.end(); ++it) {
        EContact *item = (*it).second;
        if(item) {
            // TODO: delete also all its attribs?
            g_object_unref(item);
        }
    }
    mCallHistory.clear();
    mCallHistoryOrder.clear();

    GError *err = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 OBEX_PREFIX,
                                 "/org/bluez/obex",
                                 OBEX_CLIENT_IFACE,
                                 "RemoveSession",
                                 g_variant_new("(o)", mSession), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &err);
    if(err) {
        LoggerE("Failed to remove session " << mSession << ": " << err->message);
        g_error_free(err);
    }

    free(mSession);
    mSession = NULL;


    // clear sync queue, since all data/requests in the queue are not valid anymore
    clearSyncQueue();

    if(notify)
        removeSessionDone();
}

// this method should be called once the individual sync operation has finished
// the sync operation that is on-going is at the top of the queue
void Obex::initiateNextSyncRequest() {
    // remove the actual sync operation, which has just finished
    if(!mSyncQueue.empty()) {
        delete mSyncQueue.front();
        mSyncQueue.pop_front();
        if(!mSyncQueue.empty()) {
            // there is another sync request in the queue
            SyncPBData *sync = mSyncQueue.front();
            LoggerD("synchronizing data: " << sync->location << "/" << sync->phonebook);
            if(select(sync->location, sync->phonebook)) { // do call 'pullAll' only if 'select' operation was successful
                if(OBEX_ERR_NONE != pullAll(sync->phonebook, sync->count)) {
                    // 'PullAll' has not started at all, ie. there will be no 'Complete'/'Error' signals
                    // on 'Transport' - no signal at all, threfore go to next sync request from sync queue
                    initiateNextSyncRequest();
                }
            }
        }
        else {
            LoggerD("Synchronization done");
            pbSynchronizationDone();
        }
    }
    else {
        // we should never get here, but just in case
        // inform the user that the sync has finished
        // TODO: emit the signal here
    }
}

void Obex::clearSyncQueue() {
    /*
    if(mActiveTransfer) {
        GError *err = NULL;
        g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                     OBEX_PREFIX,
                                     mActiveTransfer,
                                     OBEX_TRANSFER_IFACE,
                                     "Cancel",
                                     NULL,
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &err);
        if(err) {
            LoggerE("Failed to 'Cancel' active Transfer: " << err->message);
            g_error_free(err);
            return;
        }
        LoggerD("Active transfer 'Cancel'ed");
    }
    */

    for(unsigned int i=0; i<mSyncQueue.size(); i++) {
        delete mSyncQueue.at(i);
    }
    mSyncQueue.clear();
}

void Obex::setSelectedRemoteDevice(std::string &btAddress) {
    mSelectedRemoteDevice = btAddress;
}

//DBUS: object, dict PullAll(string targetfile, dict filters)
Obex::Error Obex::pullAll(const char *type, unsigned long count) {
    LoggerD("entered");

    if(!type) {
        LoggerD("Invalid input argument(s)");
        return OBEX_ERR_INVALID_ARGUMENTS;
    }

    if(!mSession) {
        LoggerE("No session to execute operation on");
        initiateNextSyncRequest();
        return OBEX_ERR_INVALID_SESSION;
    }

    GError *err = NULL;
    GVariant *reply;

    GVariant *filters[8];
    int nfilters = 0;

    GVariant *name, *str, *var;
    // "Format" -> "vcard30"
    name = g_variant_new_string("Format");
    str = g_variant_new_string("vcard30");
    var = g_variant_new_variant(str);
    filters[nfilters++] = g_variant_new_dict_entry(name, var);

    // "Offset" -> Offset of the first item, default is 0
    if(count > 0) {
        name = g_variant_new_string("MaxCount");
        str = g_variant_new_uint16(count);
        var = g_variant_new_variant(str);
        filters[nfilters++] = g_variant_new_dict_entry(name, var);
    }

    GVariant *array = g_variant_new_array(G_VARIANT_TYPE("{sv}"), filters, nfilters);

    // build the parameters variant
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(sa{sv})"));
    g_variant_builder_add_value(builder, g_variant_new("s", "")); // target file name will be automatically calculated
    g_variant_builder_add_value(builder, array);
    GVariant *parameters = g_variant_builder_end(builder);

    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                         OBEX_PREFIX,
                                         mSession,
                                         OBEX_PHONEBOOK_IFACE,
                                         "PullAll",
                                         parameters,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);

    if(err) {
        LoggerE("Failed to 'PullAll': " << err->message);
        initiateNextSyncRequest();
        g_error_free(err);
        return OBEX_ERR_DBUS_ERROR;
    }

    if(!reply) {
        LoggerE("Reply from call 'PullAll' is NULL");
        initiateNextSyncRequest();
        return OBEX_ERR_DBUS_INVALID_REPLY;
    }

    const char *transfer = NULL;
    GVariantIter *iter;
    g_variant_get(reply, "(oa{sv})", &transfer, &iter);
    LoggerD("transfer path = " << transfer);
    mActiveTransfer = strdup(transfer);
    g_timeout_add(CHECK_STALLED_TRANSFER_TIMEOUT*1000,
                  Obex::checkStalledTransfer,
                  new CtxCbData(this, NULL, strdup(transfer), NULL));

    // let's abuse 'cb' field from CtxCbData to store selected remote device's MAC
    CtxCbData *data = new CtxCbData(this, strdup(mSelectedRemoteDevice.c_str()), NULL, (void*)type);

    const char *key;
    GVariant *value;
    while(g_variant_iter_next(iter, "{sv}", &key, &value)) {
        if(!strcmp(key, "Size")) { // "Size"
            //guint64 val = g_variant_get_uint64(value);
            //LoggerD(key << " = " << val);
        }
        else { // "Name", "Filename"
            //LoggerD(key << " = " << g_variant_get_string(value, NULL));
            if(!strcmp(key, "Filename")) {
                const char *fileName = g_variant_get_string(value, NULL);
                if(fileName) {
                    LoggerD("Saving pulled data/VCards into: " << fileName);
                    // we call subscribe for "Complete" signal here, since we need to know path of stored file
                    // !!! what if signal comes before we subscribe for it? ... CAN IT? ... signals from DBUS
                    // should be executed in the thread the registration was made from, ie. this method has to
                    // to be completed first
		    data->data1 = strdup(fileName);
                    Utils::setSignalListener(G_BUS_TYPE_SESSION, OBEX_PREFIX,
                                             "org.freedesktop.DBus.Properties", transfer, "PropertiesChanged",
                                             Obex::handleSignal, data);
                }
            }
        }
    }

    g_variant_unref(reply);

    return OBEX_ERR_NONE;
}

gboolean Obex::checkStalledTransfer(gpointer user_data) {
    CtxCbData *data = static_cast<CtxCbData*>(user_data);
    if(!data)
        return G_SOURCE_REMOVE; // single shot timeout

    Obex *ctx = static_cast<Obex*>(data->ctx);
    char *transfer = static_cast<char*>(data->data1);
    if(!ctx || !transfer) {
        LoggerE("Failed to cast to Obex");
        return G_SOURCE_REMOVE; // single shot timeout
    }
    if(ctx->mActiveTransfer && !strcmp(ctx->mActiveTransfer, transfer)) {
        LoggerD("The active transfer is Stalled");
        ctx->clearSyncQueue();
        ctx->transferStalled();
    }
    free(transfer);
    delete data;
    return G_SOURCE_REMOVE; // single shot timeout
}

Obex::Error Obex::syncContacts(unsigned long count) {
    LoggerD("entered");
    if(!mSession) {
        LoggerD("Session not created, you have to call createSession before calling any method");
        return OBEX_ERR_INVALID_SESSION;
    }
    mSyncQueue.push_back(new SyncPBData("INT", "pb", count));

    // if the size is one, that means that there has not been
    // synchronization on-going and therefore we can initiate
    // synchronization from here, otherwise it will be initiated
    // once the current one will have finished
    if(mSyncQueue.size() == 1) {
        SyncPBData *sync = mSyncQueue.front();
        LoggerD("synchronizing data: " << sync->location << "/" << sync->phonebook << " count=" << count);
        if(select(sync->location, sync->phonebook)) { // do call 'pullAll' only if 'select' operation was successful
            if(OBEX_ERR_NONE != pullAll(sync->phonebook, sync->count)) {
                // 'PullAll' has not started at all, ie. there will be no 'Complete'/'Error' signals
                // on 'Transport' - no signal at all, threfore go to next sync request from sync queue
                initiateNextSyncRequest();
            }
        }
    }

    return OBEX_ERR_NONE;
}

void Obex::getContactByPhoneNumber(const char *phoneNumber, std::string &contact) {
    if(!phoneNumber) {
        // return empty JSON contact, in case phoneNumber is invalid
        contact = "{}";
        return;
    }

    for(auto it=mContacts.begin(); it!=mContacts.end(); ++it) {
        GList *phoneNumbersList = (GList*)e_contact_get((*it).second, E_CONTACT_TEL);
        if(phoneNumbersList) {
            const char *phoneNumberToCheck = phoneNumbersList->data?(const char*)phoneNumbersList->data:NULL;
            if(phoneNumberToCheck && !strcmp(phoneNumberToCheck, phoneNumber)) {
                parseEContactToJsonTizenContact((*it).second, contact);
                g_list_free(phoneNumbersList);
                return;
            }
            g_list_free(phoneNumbersList);
        }
    }

    // if the contact is not found, return empty JSON contact
    contact = "{}";
}

void Obex::getJsonContacts(std::string& contacts, unsigned long count) {
    LoggerD("entered");

    contacts = "[";

    // if count == 0, ie. return all contacts
    count = (count>0 && count<mContactsOrder.size())?count:mContactsOrder.size();

    for(unsigned int i = 0; i<count; ++i) { // get 'count' latest contacts, ie. 'count' first from the list
        EContact *item = mContacts[mContactsOrder.at(i)];
        if(item) { // make sure, that the item exists
            //if(i!=0) // exclude ',' for the first entry - DON'T compare it against the index - What if first item is not found in the map?
            if(contacts.compare("[")) // exclude ',' for the first entry
                contacts += ",";
            std::string contact;
            parseEContactToJsonTizenContact(item, contact);
            contacts += contact;
        }
    }

    contacts += "]";
}

void Obex::parseEContactToJsonTizenContact(EContact *econtact, std::string &contact) {
       const char *uid = (const char*)e_contact_get_const(econtact, E_CONTACT_UID);

       if(!econtact || !uid) {
           contact = "{}"; // empty contact
           return;
       }

       // uid:
       contact = "{";
       contact += "\"uid\":\"";
       contact += uid;
       contact += "\"";

       // personId:
       GList *phoneNumbersList = (GList*)e_contact_get(econtact, E_CONTACT_TEL);
       const char *personId = (phoneNumbersList && phoneNumbersList->data)?(const char*)phoneNumbersList->data:NULL; // phoneNumber is used as personId - first number from the list is used
       if(personId && strcmp(personId, "")) {
           contact += ",\"personId\":\"";
           contact += personId;
           contact += "\"";
       }
       // DON'T free the list yet, it will be used later in the function
       //if(phoneNumbersList)
       //    g_list_free(phoneNumbersList);

       // addressBookId: not parsed

       // lastUpdated: not parsed

       // isFavorite: not parsed

       // name:
       contact += ",\"name\":{";
       const char *firstName = (const char*)e_contact_get_const(econtact, E_CONTACT_GIVEN_NAME);
       const char *lastName = (const char*)e_contact_get_const(econtact, E_CONTACT_FAMILY_NAME);
       const char *displayName = (const char*)e_contact_get_const(econtact, E_CONTACT_FULL_NAME);
       bool firstAttr = true; // used to indicate whether separating comma should be used
       if(firstName && strcmp(firstName, "")) {
           firstAttr = false;
           contact += "\"firstName\":\"";
           contact += firstName;
           contact += "\"";
       }
       if(lastName && strcmp(lastName, "")) {
           if(firstAttr)
               contact += "\"lastName\":\"";
           else
               contact += ",\"lastName\":\"";
           firstAttr = false;
           contact += lastName;
           contact += "\"";
       }
       if(displayName && strcmp(displayName, "")) {
           if(firstAttr)
               contact += "\"displayName\":\"";
           else
               contact += ",\"displayName\":\"";
           firstAttr = false;
           contact += displayName;
           contact += "\"";
       }
       contact += "}";

       // addresses:
       contact += ",\"addresses\":[";
       for(int id=E_CONTACT_ADDRESS_HOME; id<=E_CONTACT_ADDRESS_OTHER; id++) {
           EContactAddress *address = (EContactAddress*)e_contact_get(econtact, (EContactField)id);
           if(address) {
               contact += "{";
               contact += "\"isDefault\":\"false\"";
               if(address->country && strcmp(address->country,"")) {
                   contact += ",\"country\":\"";
                   contact += address->country;
                   contact += "\"";
               }
               if(address->region && strcmp(address->region,"")) {
                   contact += ",\"region\":\"";
                   contact += address->region;
                   contact += "\"";
               }
               if(address->locality && strcmp(address->locality,"")) {
                   contact += ",\"city\":\"";
                   contact += address->locality;
                   contact += "\"";
               }
               if(address->street && strcmp(address->street,"")) {
                   contact += ",\"streetAddress\":\"";
                   contact += address->street;
                   contact += "\"";
               }
               if(address->code && strcmp(address->code,"")) {
                   contact += ",\"postalCode\":\"";
                   contact += address->code;
                   contact += "\"";
               }
               contact += ",\"types\":[\"";
               contact += id==E_CONTACT_ADDRESS_HOME ? "HOME" : (id==E_CONTACT_ADDRESS_WORK ? "WORK" : (id==E_CONTACT_ADDRESS_OTHER ? "OTHER" : ""));
               contact += "\"]";
               contact += "}";

               e_contact_address_free(address);
           }
       }
       contact += "]";

       // photoURI:
       EContactPhoto *photo = (EContactPhoto*)e_contact_get(econtact, E_CONTACT_PHOTO);
       if(photo) {
           // we should have only URI type of contact photo, ... see processVCards() method
           if(E_CONTACT_PHOTO_TYPE_URI == photo->type) {
                const char *uri = e_contact_photo_get_uri(photo);
                if(uri && strcmp(uri, "")) {
                    //LoggerD("photoURI = " << uri);
                    contact += ",\"photoURI\":\"";
                    contact += uri;
                    contact += "\"";
                }
           }
       }
       e_contact_photo_free(photo);

       // phoneNumbers
       contact += ",\"phoneNumbers\":[";
       bool firstNumber = true;
       while(phoneNumbersList && phoneNumbersList->data) {
           const char *phoneNumber = (phoneNumbersList && phoneNumbersList->data)?(const char*)phoneNumbersList->data:"";
           if(phoneNumber && strcmp(phoneNumber, "")) {
               if(firstNumber)
                   contact += "{\"number\":\"";
               else
                   contact += ",{\"number\":\"";
               firstNumber = false;
               contact += phoneNumber;
               contact += "\"}";
           }
           phoneNumbersList = phoneNumbersList->next;
       }
       contact += "]";
       // now we can free the list
       if(phoneNumbersList)
           g_list_free(phoneNumbersList);

       // emails:
       contact += ",\"emails\":[";
       bool firstEmail = true;
       for(int id=E_CONTACT_FIRST_EMAIL_ID; id<=E_CONTACT_LAST_EMAIL_ID; id++) {
           const char *email = (const char*)e_contact_get_const(econtact, (EContactField)id);
           if(email && strcmp(email, "")) {
               if(firstEmail)
                   contact += "{\"email\":\"";
               else
                   contact += ",{\"email\":\"";

               contact += email;
               contact += "\"";
               contact += ",\"isDefault\":\"false\""; // TODO: ?use 'firstEmail' value to set the first e-mail address as default?
               contact += ",\"types\":[\"WORK\"]"; // just some default value

               firstEmail = false;
               contact += "}";
           }
       }
       contact += "]";

       // birthday: not parsed

       // anniversaries: not parsed

       // organizations: not parsed

       // notes: not parsed

       // urls: not parsed

       // ringtoneURI: not parsed

       // groupIds: not parsed

       contact += "}";
}

Obex::Error Obex::syncCallHistory(unsigned long count) {
    LoggerD("entered");
    if(!mSession) {
        LoggerD("Session not created, you have to call createSession before calling any method");
        return OBEX_ERR_INVALID_SESSION;
    }
    mSyncQueue.push_back(new SyncPBData("INT", "cch", count));

    // if the size is one, that means that there has not been
    // synchronization on-going and therefore we can initiate
    // synchronization from here, otherwise it will be initiated
    // once the current one will have finished
    if(mSyncQueue.size() == 1) {
        SyncPBData *sync = mSyncQueue.front();
        LoggerD("synchronizing data: " << sync->location << "/" << sync->phonebook << " count=" << count);
        if(select(sync->location, sync->phonebook)) { // do call 'pullAll' only if 'select' operation was successful
            if(OBEX_ERR_NONE != pullAll(sync->phonebook, sync->count)) {
                // 'PullAll' has not started at all, ie. there will be no 'Complete'/'Error' signals
                // on 'Transport' - no signal at all, threfore go to next sync request from sync queue
                initiateNextSyncRequest();
            }
        }
    }

    return OBEX_ERR_NONE;
}

void Obex::getJsonCallHistory(std::string& calls, unsigned long count) {
    LoggerD("entered");

    calls = "[";

    // if count == 0, ie. return all calls
    count = (count>0 && count<mCallHistoryOrder.size())?count:mCallHistoryOrder.size();

    for(unsigned int i = 0; i<count; ++i) { // get 'count' latest calls, ie. 'count' first from the list
        EContact *item = mCallHistory[mCallHistoryOrder.at(i)];
        if(item) { // make sure, that the item exists
            //if(i!=0) // exclude ',' for the first entry - DON'T compare it against the index - What if first item is not found in the map?
            if(calls.compare("[")) // exclude ',' for the first entry
                calls += ",";
            std::string call;
            parseEContactToJsonTizenCallHistoryEntry(item, call);
            calls += call;
        }
    }

    calls += "]";
}

void Obex::parseEContactToJsonTizenCallHistoryEntry(EContact *econtact, std::string &call) {
       const char *uid = (const char*)e_contact_get_const(econtact, E_CONTACT_UID);
       if(!econtact || !uid) {
           call = "{}"; // empty call history entry
           return;
       }

       // uid:
       call = "{";
       call += "\"uid\":\"";
       call += uid;
       call += "\",";

       // type: not parsing - use some DEFAULT value, eg. "TEL"
       call += "\"type\":\"TEL\",";

       // features: not parsing - use some DEFAULT value, eg. "VOICECALL"
       call += "\"features\":[\"VOICECALL\"],";

       // remoteParties
       call += "\"remoteParties\":[";
       call += "{";
       call += "\"personId\":\"";
       GList *phoneNumbersList = (GList*)e_contact_get(econtact, E_CONTACT_TEL);
       const char *personId = (phoneNumbersList && phoneNumbersList->data)?(const char*)phoneNumbersList->data:""; // phoneNumber is used as personId - first number from the list is used
       call += personId;
       call += "\"";
       if(phoneNumbersList)
           g_list_free(phoneNumbersList);
       const char *fullName = (const char*)e_contact_get_const(econtact, E_CONTACT_FULL_NAME);
       if(fullName && strcmp(fullName, "")) {
           call += ",\"remoteParty\":\"";
           call += fullName;
           call += "\"";
       }
       call += "}";
       call += "],";

       // startTime
       const char *startTime = (const char*)e_contact_get_const(econtact, E_CONTACT_REV); // 'REV' holds call date/time
       if(startTime && strcmp(startTime, "")) {
           std::string startTimeStr = startTime;
           startTimeStr.insert(13,":");
           startTimeStr.insert(11,":");
           startTimeStr.insert(6,"-");
           startTimeStr.insert(4,"-");
           call += "\"startTime\":\"";
           call += startTimeStr;
           call += "\",";
       }

       // duration: not parsing - use 0 as default value
       call += "\"duration\":\"0\",";

       //  direction:
       call += "\"direction\":\"";
       const char *direction = (const char*)e_contact_get_const(econtact, E_CONTACT_NOTE); // 'NOTE' holds direction of the call
       call += direction?direction:(char*)"UNDEFINED";
       call += "\"";

       call += "}";
}

void Obex::handleSignal(GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *signal_name,
                         GVariant              *parameters,
                         gpointer               user_data)
{
    LoggerD("signal received: '" << interface_name << "' -> '" << signal_name << "' -> '" << object_path << "'");

    CtxCbData *data = static_cast<CtxCbData*>(user_data);
    if(!data) {
        LoggerE("Failed to cast object: CtxCbData");
        return;
    }
    Obex *ctx = static_cast<Obex*>(data->ctx);
    if(!ctx) {
        LoggerE("Failed to cast object: Obex");
        return;
    }

    if(!strcmp(signal_name, "PropertiesChanged"))
    {
		char *objPath = NULL;
		GVariantIter* iter, iter2;

		g_variant_get(parameters, "(sa{sv}as)", &objPath, &iter, &iter2);

		if(objPath)
		{
			GVariant* var;
			char *prop = NULL;

			while(g_variant_iter_next(iter, "{sv}", &prop, &var))
			{
				if(!strcmp(prop, "Status"))
				{
					char *status_str=0;
					g_variant_get(var, "s", &status_str);
					//LoggerD("Status is: " << status_str);

					if(!strcmp(status_str, "complete"))
					{
						const char *path = static_cast<const char *>(data->data1);
						const char *type = static_cast<const char *>(data->data2);
						const char *origin = static_cast<const char *>(data->cb);
						ctx->processVCards(path, type, origin);


						if(data->data1) free(data->data1); // path - to the file containing received VCards
						if(data->cb) free(data->cb);       // origin - MAC address of selected remote device
					   	delete data;
						ctx->initiateNextSyncRequest();
					}
				}
			}
		}
		else
		{
			LoggerD("No objectPath found. Exiting.");
		}
    }
}

void Obex::processVCards(const char *filePath, const char *type, const char *origin) {
    LoggerD("entered");

    if(!filePath || !type || !origin) {
        LoggerE("Invalid argument(s)");
        return;
    }

    if(strcmp(origin, mSelectedRemoteDevice.c_str())) {
        LoggerD("Received VCards don't belong to currently selected device - IGNORING");
        return;
    }

    std::map<std::string, EContact*> *items = NULL;
    std::vector<std::string> *order = NULL;
    if(!strcmp(type, "pb")) { // Contacts
        items = &mContacts;
        order = &mContactsOrder;
    }
    else if(!strcmp(type, "cch")) { // CallHistory
        items = &mCallHistory;
        order = &mCallHistoryOrder;
    }
    // if the size of items map is 0, ie. that the received
    // VCards are from first sync request and they should
    // be added to the map (uid order vector) in the order they
    // are processed (push_back), otherwise they should be
    // inserted at the front (push_front)
    bool firstData = items->size() == 0 ? true : false;

    // process VCards one-by-one
    std::ifstream file(filePath);
    std::string vcard;
    for(std::string line; getline(file, line);)
    {
        //std::replace( line.begin(), line.end(), '\r', '\n'); // removes carriage return
        line.replace(line.find("\r"), 1, "\n");

        if(line.find("BEGIN:VCARD") == 0) {
            vcard = line; // start collecting new VCard
        }
        else if(line.find("END:VCARD") == 0) {
            vcard += line;

            // start processing VCard
            //printf("%s\n", vcard.c_str());

            EContact *item = e_contact_new_from_vcard(vcard.c_str());
            if(!item) {
                LoggerD("Failed to create EContact from vcard");
                continue;
            }

            // won't use E_CONTACT_UID as a key to the map, since it is not returned by all phone devices
            if(!makeUid(item)) {
                // failed to create UID from EContact
                // won't add the entry to the list - UID used as a key to the map
                continue;
            }
            const char *uid = (const char*)e_contact_get_const(item, E_CONTACT_UID);

            // check if item has photo and it's INLINED type
            // if so, change it to URI type, since the data are in binary form
            // and as such can't be processed in JSON directly
            // to avoid yet another conversion to eg. BASE64 format, save the
            // contact photo in /tmp and use the URI to reference the photo instead
            EContactPhoto *photo = (EContactPhoto*)e_contact_get(item, E_CONTACT_PHOTO);
            if(photo) {
                if(E_CONTACT_PHOTO_TYPE_INLINED == photo->type) {
                    gsize length = 0;
                    const guchar *data = e_contact_photo_get_inlined (photo, &length);
                    //uid is used as a file name
                    char fileName[128];
                    snprintf(fileName, sizeof(fileName), "/tmp/%s.jif", uid);
                    FILE *fd = fopen(fileName,"wb");
                    if(!fd) {
                        LoggerD("Unable to store contact photo: " << fileName);
                    }
                    else {
                        LoggerD("Saving contact photo: " << fileName);
                        size_t written = fwrite(data, sizeof(guchar), length, fd);
                        fclose(fd);
                        if(written == length) {
                            // contact photo has been successfully saved
                            // change photo attribute from INLINED to URI
                            e_contact_photo_free(photo);
                            photo = NULL;
                            photo = e_contact_photo_new();
                            if(photo) {
                                photo->type = E_CONTACT_PHOTO_TYPE_URI;
                                //e_contact_photo_set_mime_type(photo, "");
                                char uri[128];
                                snprintf(uri, sizeof(uri), "file://%s", fileName);
                                e_contact_photo_set_uri(photo, uri);
                                e_contact_set(item, E_CONTACT_PHOTO, photo);
                            }
                        }
                    }
                }
                e_contact_photo_free(photo);
            }

            // check if an item with the given UID exists in the list
            if((*items)[uid] == NULL) {
                //LoggerD("NEW ITEM: " << uid);
                (*items)[uid] = item;
                if(firstData)
                    order->push_back(uid);
                else
                    order->insert(order->begin(), uid); // push at the front
                if(!strcmp(type, "cch")) { // notify only for CallHistory
                    std::string entry;
                    parseEContactToJsonTizenCallHistoryEntry(item, entry);
                    callHistoryEntryAdded(entry);
                }
            }
            else {
                // the item already exists in the list, unref the item,
                // since we loose any reference to it
                g_object_unref(item);
            }
        }
        else {
            // the current implementation of EContact doesn't support
            // X-IRMC-CALL-DATETIME field, so as a workaround we use
            // two separate fields instead: E_CONTACT_NOTE
            //                              E_CONTACT_REV
            if((line.find("NOTE") == 0) || (line.find("REV") == 0)) {
                // exclude NOTE and REV as we are using it to store
                // X-IRMC-CALL-DATETIME attribute
                // exclude = do not copy it to vcard
            }
            else if(line.find("UID") == 0) {
                // exclude UID as we are creating own UID
                // exclude = do not copy it to vcard
            }
            else if(line.find("X-IRMC-CALL-DATETIME") == 0) {
                size_t index1 = line.find( "TYPE=" ) + 5;
                size_t index2 = line.find( ":", index1 ) + 1;

                std::string note = line.substr (index1, index2-index1-1);
                std::string rev = line.substr (index2, line.length()-index2);

                vcard += "NOTE:" + note + "\n";
                vcard += "REV:" + rev; // '\n' is taken from 'line'
            }
            else {
                vcard += line;
            }
        }
    }

    // notify listener about Contacts/CallHistory being changed/synchronized
    if(type) {
        if(!strcmp(type, "pb")) // Contacts
            contactsChanged();
        else if(!strcmp(type, "cch")) // CallHistory
            callHistoryChanged();
    }
}

bool Obex::makeUid(EContact *entry) {
    // use combination of phone number, given/family name and the modification date
    const char *_uid = (const char*)e_contact_get_const(entry, E_CONTACT_UID);
    if(_uid) {
        // we shouldn't get here, since E_CONTACT_UID is filtered-out in processVCards() method
        // does "e_contact_set" frees the memory if the field already exists?
        return false;
    }
    GList *phoneNumbersList = (GList*)e_contact_get(entry, E_CONTACT_TEL);
    const char *phoneNumber = (phoneNumbersList && phoneNumbersList->data) ? (const char*)phoneNumbersList->data : NULL;
    const char *givenName = (const char*)e_contact_get_const(entry, E_CONTACT_GIVEN_NAME);
    const char *familyName = (const char*)e_contact_get_const(entry, E_CONTACT_FAMILY_NAME);
    const char *call_rev = (const char*)e_contact_get_const(entry, E_CONTACT_REV);

    if((!phoneNumber || !phoneNumber[0]) && (!givenName || !givenName[0]) && (!familyName || !familyName[0]) && (!call_rev || !call_rev[0])) {
        // uid is used as a key to the map
        LoggerD("Invalid EContact entry - not adding to the list");
        if(phoneNumbersList)
            g_list_free(phoneNumbersList);
        return false;
    }

    char uid[128];
    snprintf(uid, sizeof(uid), "%s:%s:%s:%s", phoneNumber?phoneNumber:"",
                                              givenName?givenName:"",
                                              familyName?familyName:"",
                                              call_rev?call_rev:"");

    // TODO: check
    // does "e_contact_set" make a copy of value, or
    // do we need make a copy of the value on the HEAP?
    e_contact_set(entry, E_CONTACT_UID, uid);

    if(phoneNumbersList)
        g_list_free(phoneNumbersList);

    return true;
}

} // PhoneD

