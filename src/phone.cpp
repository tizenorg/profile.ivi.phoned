#include <string.h>
#include <fstream>
#include <algorithm>
#include <aul.h>

#include "phone.h"
#include "utils.h"

#include "Logger.h"

namespace PhoneD {

#define TIZEN_PREFIX           "org.tizen"
#define DIALER_APP_ID          "org.tizen.dialer"
#define MODELLO_APP_ID         "Modello005.Homescreen"

#define PHONE_SERVICE          TIZEN_PREFIX ".phone"
#define PHONE_IFACE            TIZEN_PREFIX ".Phone"
#define PHONE_OBJ_PATH         "/"

#define DELAYED_SYNC_CALLHISTORY_INTERVAL  5000 // delayed synchronization of CallHistory from the phone (in ms)
                                                // when a call is made, it takes certain time until the entry gets added in the list
                                                // use a timeout to delay synchronization request

#define CALLHISTORY_UPDATED_SYNC_COUNT     10   // number of latest CallHistory entries to be requested from the phone

#define PHONE_INTERFACE_XML                                     \
    "<node>"                                                    \
    "  <interface name='" PHONE_IFACE "'>"                      \
    "    <method name='SelectRemoteDevice'>"                    \
    "      <arg type='s' name='address' direction='in'/>"       \
    "    </method>"                                             \
    "    <method name='GetSelectedRemoteDevice'>"               \
    "      <arg type='s' name='address' direction='out'/>"      \
    "    </method>"                                             \
    "    <method name='UnselectRemoteDevice'>"                  \
    "    </method>"                                             \
    "    <method name='Dial'>"                                  \
    "      <arg type='s' name='number' direction='in'/>"        \
    "    </method>"                                             \
    "    <method name='Answer'>"                                \
    "    </method>"                                             \
    "    <method name='Mute'>"                                  \
    "      <arg type='b' name='muted' direction='in'/>"         \
    "    </method>"                                             \
    "    <method name='Hangup'>"                                \
    "    </method>"                                             \
    "    <method name='ActiveCall'>"                            \
    "      <arg type='a{sv}' name='call' direction='out'/>"     \
    "    </method>"                                             \
    "    <method name='Synchronize'>"                           \
    "    </method>"                                             \
    "    <method name='GetContacts'>"                           \
    "      <arg type='u' name='count' direction='in'/>"         \
    "      <arg type='s' name='contacts' direction='out'/>"     \
    "    </method>"                                             \
    "    <method name='GetCallHistory'>"                        \
    "      <arg type='u' name='count' direction='in'/>"         \
    "      <arg type='s' name='calls' direction='out'/>"        \
    "    </method>"                                             \
    "  </interface>"                                            \
    "</node>"

/*
Signals:

       "RemoteDeviceSelected"  : "(s)" ... {?value,?error}
       "ContactsChanged"       : ""
       "CallHistoryChanged"    : ""
       "CallHistoryEntryAdded" : "(s)" ... tizen.CallHistoryEntry
       "CallChanged"           : "(a{sv})" ... "state", "line_id", "contact"
*/

Phone::Phone() :
    ConnMan(),
    Bluez(),
    OFono(),
    Obex(),
    mAdapterPowered(false),
    mWantedRemoteDevice(""),
    mSelectedRemoteDevice(""),
    mPBSynchronized(false),
    mNameRequestId(0),
    mRegistrationId(0),
    mIntrospectionData(NULL)
{
    LoggerD("entered");

    memset(&mIfaceVTable, 0, sizeof(mIfaceVTable));

    // initialize DBUS
    mNameRequestId = g_bus_own_name(G_BUS_TYPE_SESSION,
                                    PHONE_SERVICE,
                                    G_BUS_NAME_OWNER_FLAGS_NONE,
                                    Phone::busAcquiredCb,
                                    Phone::nameAcquiredCb,
                                    Phone::nameLostCb,
                                    this,
                                    NULL); //GDestroyNotify

    // Request to get own name is sent, now
    // busAcquiredCb should be called and thus setup() initiated

    /*
     * No longer need to power up bluetooth
     * so that bluetooth status is persisted
     *
    if(setBluetoothPowered(true)) {
        LoggerD("Bluetooth powered");
        if(setAdapterPowered(true)) {
            LoggerD("hci adapter powered");
        }
        else {
            LoggerE("Failed to Power-ON hci adapter");
        }
    }
    else {
        LoggerE("Failed to Power-ON Bluetooth");
    }
    */
}

Phone::~Phone() {
    LoggerD("entered");
    if(mRegistrationId > 0) {
        g_dbus_connection_unregister_object(g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL), mRegistrationId);
        LoggerD("Unregistered object with id: " << mRegistrationId);
        mRegistrationId = 0;
    }
    if(mNameRequestId > 0) {
        g_bus_unown_name(mNameRequestId);
        LoggerD("Un-owned name with id: " << mNameRequestId);
        mNameRequestId = 0;
    }
    removeSession();
}

void Phone::busAcquiredCb(GDBusConnection *gdbus, const gchar *name, gpointer data)
{
    LoggerD("acquired bus " << name);

    Phone *phone = static_cast<Phone*>(data);
    if(!phone) {
        LoggerE("Failed to cast data to Phone*");
        return;
    }

    phone->setup();
}

void Phone::nameAcquiredCb(GDBusConnection *gdbus, const gchar *name, gpointer data)
{
    LoggerD("acquired name " << name);

    Phone *phone = static_cast<Phone*>(data);
    if(!phone) {
        LoggerE("Failed to cast data to Phone*");
        return;
    }

    // it is generally too late to export the objects in name_acquired_handler,
    // which is done in setup() method (calling: g_dbus_connection_register_object())
    // Instead, you can do this in bus_acquired_handler, ie. in busAcquiredCb() callback method
    //phone->setup();

    phone->init(); // ConnMan
}

void Phone::nameLostCb(GDBusConnection *gdbus, const gchar *name, gpointer data)
{
    LoggerD("lost name " << name);

    Phone *phone = static_cast<Phone*>(data);
    if(!phone) {
        LoggerE("Failed to cast data to Phone*");
        return;
    }
    //TODO: think of an algorithm to re-request own name in case name is lost
}

void Phone::setup() {
    LoggerD("entered");

    mIfaceVTable.method_call = Phone::handleMethodCall;
    mIntrospectionData = g_dbus_node_info_new_for_xml(PHONE_INTERFACE_XML, NULL);

    if (mIntrospectionData == NULL) {
        LoggerE("Failed to create introspection data from XML");
        return;
    }

    GError *error = NULL;
    mRegistrationId = g_dbus_connection_register_object( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                                         PHONE_OBJ_PATH,
                                                         mIntrospectionData->interfaces[0],
                                                         &mIfaceVTable,
                                                         this,
                                                         NULL, //GDestroyNotify
                                                         &error);

    if(error) {
        LoggerE("Failed to register object: " << PHONE_OBJ_PATH << " : " << error->message);
        g_error_free(error);
        return;
    }

    // Obex/OFono needs an agent to be registered on DBus
    // TODO: implement a check whether agent is registered and running
    // NOTE: if the device is not yet paired, you need to pair it first
    // NOTE: if the device is not yet "trusted", the user has to accept
    // the request to trust on the device
    setupAgent();    // Bluez
    registerAgent(); // Bluez

    // read MAC address of selected remote device from persistent storage
    std::string btAddress;
    bool isSelectedRemoteDevice = readSelectedRemoteDeviceMAC(btAddress);
    if(isSelectedRemoteDevice) {
        mWantedRemoteDevice = btAddress;
        bool paired = isDevicePaired(btAddress.c_str());
        if(paired) {
            // TODO: here should be also a check whether the device is visible (is in the range)
            LoggerD("The device is paired ... starting services");
            startServices();
        }
    }
}

void Phone::adapterPowered(bool value) {
    LoggerD("Default adapter powered: " << (value?"ON":"OFF"));
    mAdapterPowered = value;
    return value ? startServices() : stopServices();
}

void Phone::defaultAdapterAdded() {
    LoggerD("Default adapter added");
    startServices();
}

void Phone::defaultAdapterRemoved() {
    LoggerD("Default adapter removed");
    stopServices();
}

void Phone::startServices() {
    std::string device = "";
    setSelectedRemoteDevice(device);
    // select the modem for the 'wanted' device - once the modem is found, it is
    // set 'Powered' ON, which will call 'modemPowered', from where 'createSession' is called
    selectModem(mWantedRemoteDevice);
    //createSession(mWantedRemoteDevice.c_str());
}

void Phone::stopServices() {
    std::string device = "";
    setSelectedRemoteDevice(device);
    // call 'unselectModem', which unselects and sets modem "Powered" OFF and as a result,
    // modemPowered method is called in which 'removeSession' is called
    unselectModem();
    //removeSession();
}

// this method is called when the device is paired (it's not directly connected
// to Bluez' DBUS "DeviceCreated" method, but rather to Paired PropertyChanged
void Phone::deviceCreated(const char *device) {
    LoggerD("created&paired device: " << device);

    std::string dev = device;
    makeMACFromDevicePath(dev); // AA:BB:CC:DD:EE:FF
    makeRawMAC(dev); // AABBCCDDEEFF

    // make a copy, since we will modify it
    std::string wanted = mWantedRemoteDevice;
    wanted.erase(std::remove_if(wanted.begin(), wanted.end(), isnxdigit), wanted.end());
    if(!dev.compare(wanted)) {
        LoggerD("Paired wanted device");
        startServices();
    }
}

void Phone::deviceRemoved(const char *device) {
    LoggerD("removed device: " << device);

    std::string dev = device;
    makeMACFromDevicePath(dev); // AA:BB:CC:DD:EE:FF
    makeRawMAC(dev); // AABBCCDDEEFF

    // make a copy, since we will modify it
    std::string selected = mSelectedRemoteDevice;
    selected.erase(std::remove_if(selected.begin(), selected.end(), isnxdigit), selected.end());
    if(!dev.compare(selected)) {
        LoggerD("Removed selected device");
        stopServices();
    }
}

void Phone::modemAdded(std::string &modem) {
    LoggerD("modem added: " << modem);
    if (mWantedRemoteDevice.empty()) {
        LoggerD("No modem selected yet, default to: " << modem);
        mWantedRemoteDevice = modem;
        storeSelectedRemoteDeviceMAC(modem);
        startServices();
    } else if(!mWantedRemoteDevice.compare(modem)) {
        selectModem(mWantedRemoteDevice);
    }
}

void Phone::modemPowered(bool powered) {
    LoggerD("Modem powered: " << (powered?"ON":"OFF"));
    if(powered) {
        mPBSynchronized = false;
        createSession(mWantedRemoteDevice.c_str());
    }
    else {
        mPBSynchronized = false;
        removeSession();
    }
}

void Phone::setModemPoweredFailed(const char *error) {
    LoggerD("SelectModem failed: " << error);
    std::string result("");
    result += "{\"error\":\"";
    result += (error?error:"Unknown error");
    result += "\"}";
    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "RemoteDeviceSelected",
                                   g_variant_new("(s)", result.c_str()),
                                   NULL);
}

void Phone::createSessionFailed(const char *error) {
    LoggerD("CreateSession failed: " << error);
    std::string result("");
    result += "{\"error\":\"";
    result += (error?error:"Unknown error");
    result += "\"}";
    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "RemoteDeviceSelected",
                                   g_variant_new("(s)", result.c_str()),
                                   NULL);
}

// creates a copy of session name ... don't forget to free the memory in dtor
// and possibly on other places as well ( ??? session closed ??? )
void Phone::createSessionDone(const char *session) {
    LoggerD("CreateSession DONE: " << (session?session:"SESSION NOT CREATED"));

    setSelectedRemoteDevice(mWantedRemoteDevice);

    // emit signal that the remote device has been selected
    std::string result("");
    result += "{\"value\":\"";
    result += mSelectedRemoteDevice;
    result += "\"}";
    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "RemoteDeviceSelected",
                                   g_variant_new("(s)", result.c_str()), // empty object indicates that the selecting remote device has been successfull
                                   NULL);

    LoggerD("starting synchronization process: contacts/call history");
    /*Obex::Error err = */syncContacts();
    /*Obex::Error err = */syncCallHistory();
}

void Phone::removeSessionDone() {
    LoggerD("entered");

    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "RemoteDeviceSelected",
                                   g_variant_new("(s)", "{\"value\":\"\"}"),
                                   NULL);

    // cleared list of contacts
    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "ContactsChanged",
                                   NULL,
                                   NULL);

    // cleared call history
    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "CallHistoryChanged",
                                   NULL,
                                   NULL);
}

void Phone::transferStalled() {
    if(mAdapterPowered) {
        LoggerD("Adapter is powered ... calling 'CreateSession'");
        mPBSynchronized = false;
        std::string device = "";
        setSelectedRemoteDevice(device);
        createSession(mWantedRemoteDevice.c_str());
    }
    else {
        LoggerD("Adapter is not powered ... not calling 'CreateSession'");
    }
}

void Phone::setSelectedRemoteDevice(std::string &btAddress) {
    mSelectedRemoteDevice = btAddress;
    // Obex also needs to know selected remote device to pair received VCards with
    Obex::setSelectedRemoteDevice(btAddress);
}

void Phone::callHistoryEntryAdded(std::string &entry) {
    //LoggerD("CallHistoryEntryAdded: " << entry);

    // do not emit signal if the PB has not yet been synchronized
    // doing so would result in emiting too many signals: one signal
    // per entry in the call history list
    // only newly placed/received calls will be emited
    if(mPBSynchronized) {
        g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                       NULL,
                                       PHONE_OBJ_PATH,
                                       PHONE_IFACE,
                                       "CallHistoryEntryAdded",
                                       g_variant_new("(s)", entry.c_str()),
                                       NULL);
    }
}

void Phone::contactsChanged() {
    LoggerD("entered");

    // do emit signal only if PB is not yet synchronized
    // the current implementation doesn't handle contacts added/updated/removed
    // this signal is to notify client app that contacts list has chaned, eg. as
    // a result of selecting other remote device via 'selectRemoteDevice' method
    if(!mPBSynchronized) {
        g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                       NULL,
                                       PHONE_OBJ_PATH,
                                       PHONE_IFACE,
                                       "ContactsChanged",
                                       NULL,
                                       NULL);
    }
}

void Phone::callHistoryChanged() {
    LoggerD("entered");

    // do emit signal only if PB is not yet synchronized
    // the current implementation doesn't handle calls updated/removed (adding
    // call entry into call history is notified via CallHistoryEntryAdded signal
    // this signal is to notify client app that contacts list has chaned, eg. as
    // a result of selecting other remote device via 'selectRemoteDevice' method
    if(!mPBSynchronized) {
        g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                       NULL,
                                       PHONE_OBJ_PATH,
                                       PHONE_IFACE,
                                       "CallHistoryChanged",
                                       NULL,
                                       NULL);
    }
}

void Phone::pbSynchronizationDone() {
    LoggerD("PB synchronization DONE");
    mPBSynchronized = true;
    // remove session after sync is done to prevent blocking other clients
    // each sync will now require calling createSession() instead
    removeSession(false);
}

void Phone::handleMethodCall( GDBusConnection       *connection,
                              const gchar           *sender,
                              const gchar           *object_path,
                              const gchar           *interface_name,
                              const gchar           *method_name,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer               user_data)
{
    LoggerD(sender << ":" << object_path << ":" << interface_name << ":" << method_name);

    Phone *phone = static_cast<Phone*>(user_data);
    if(!phone) {
        LoggerE("Failed to cast to Phone");
        return;
    }

    if(!strcmp(method_name, "SelectRemoteDevice")) {
        char *btAddress = NULL;
        g_variant_get(parameters, "(&s)", &btAddress);
        if(!btAddress || !isValidMAC(std::string(btAddress))) {
            LoggerE("Won't select remote device: given MAC address \"" << btAddress << "\" is not valid");
            g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                           NULL,
                                           PHONE_OBJ_PATH,
                                           PHONE_IFACE,
                                           "RemoteDeviceSelected",
                                           g_variant_new("(s)", "{\"error\":\"Invalid MAC address\"}"),
                                           NULL);
        }
        else {
            LoggerD("Selecting remote device: " << btAddress);

            // check whether requested device is not yet selected
            // if so, just check if PB is already synchronized and call callback, if it is
            if(!strcmp(phone->mSelectedRemoteDevice.c_str(), btAddress)) {
                if(phone->mPBSynchronized) {
                    std::string result("");
                    result += "{\"value\":\"";
                    result += phone->mSelectedRemoteDevice;
                    result += "\"}";
                    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                                   NULL,
                                                   PHONE_OBJ_PATH,
                                                   PHONE_IFACE,
                                                   "RemoteDeviceSelected",
                                                   g_variant_new("(s)", result.c_str()), // already synchronized/selected
                                                   NULL);
                }
                else {
                    // the synchronization may be already on-going, but request it anyway
                    phone->createSession(phone->mWantedRemoteDevice.c_str());
                }
            }
            else {
                //TODO: stop all services and start everything from begining
                phone->mWantedRemoteDevice = btAddress;
                phone->storeSelectedRemoteDeviceMAC(btAddress);
                phone->startServices();
            }
        }
        g_dbus_method_invocation_return_value(invocation, NULL); // just finish the method call - don't return any status (The status is returned via "RemoteDeviceSelected" signal
    }
    else if(!strcmp(method_name, "GetSelectedRemoteDevice")) {
        g_dbus_method_invocation_return_value( invocation,
                                               g_variant_new("(s)", phone->mSelectedRemoteDevice.c_str()));
    }
    if(!strcmp(method_name, "UnselectRemoteDevice")) {
        phone->mWantedRemoteDevice = "";
        phone->stopServices();
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(!strcmp(method_name, "Dial")) {
        char *number = NULL;
        char *error = NULL;
        g_variant_get(parameters, "(&s)", &number);
        if(phone->invokeCall(number, &error)) {
            LoggerD("Dialing number: " << number);
            g_dbus_method_invocation_return_value(invocation, NULL);
        }
        else {
            if(error) { // sanity check
                LoggerD("Failed to dial number: " << error);
                GError *err = g_error_new(G_PHONE_ERROR, 1, error);
                g_dbus_method_invocation_return_gerror(invocation, err);
                free(error);
            }
        }
    }
    else if(!strcmp(method_name, "Answer")) {
        char *error = NULL;
        if(phone->answerCall(&error)) {
            LoggerD("Answering incoming call");
            g_dbus_method_invocation_return_value(invocation, NULL);
        }
        else {
            if(error) { // sanity check
                LoggerD("Failed to answer the call: " << error);
                GError *err = g_error_new(G_PHONE_ERROR, 2, error);
                g_dbus_method_invocation_return_gerror(invocation, err);
                free(error);
            }
        }
    }
    else if(!strcmp(method_name, "Hangup")) {
        char *error = NULL;
        if(phone->hangupCall(&error)) {
            LoggerD("Hanging-up active/incoming call");
            g_dbus_method_invocation_return_value(invocation, NULL);
        }
        else {
            if(error) { // sanity check
                LoggerD("Failed to hang-up the call: " << error);
                GError *err = g_error_new(G_PHONE_ERROR, 2, error);
                g_dbus_method_invocation_return_gerror(invocation, err);
                free(error);
            }
        }
    }
    else if(!strcmp(method_name, "Mute")) {
        bool mute;
        char *error = NULL;
        g_variant_get(parameters, "(b)", &mute);
        if(phone->muteCall(mute, &error)) {
            LoggerD("Muting MIC: " << mute);
            g_dbus_method_invocation_return_value(invocation, NULL);
        }
        else {
            if(error) { // sanity check
                LoggerD("Failed to mute the call: " << error);
                GError *err = g_error_new(G_PHONE_ERROR, 2, error);
                g_dbus_method_invocation_return_gerror(invocation, err);
                free(error);
            }
        }
    }
    else if(!strcmp(method_name, "ActiveCall")) {
        LoggerD("constructing ActiveCall response");

        OFono::Call *call = phone->activeCall();

        GVariant *props[8];
        int nprops = 0;

        GVariant *key, *str, *var;
        // add state
        key = g_variant_new_string("state");
        str = g_variant_new_string((call && call->state)?call->state:"disconnected");
        var = g_variant_new_variant(str);
        props[nprops++] = g_variant_new_dict_entry(key, var);
        // add line_id
        key = g_variant_new_string("line_id");
        str = g_variant_new_string((call && call->line_id)?call->line_id:"");
        var = g_variant_new_variant(str);
        props[nprops++] = g_variant_new_dict_entry(key, var);
        // get contact by phone number
        std::string contact;
        phone->getContactByPhoneNumber(call?call->line_id:NULL, contact);
        key = g_variant_new_string("contact");
        str = g_variant_new_string(contact.c_str());
        var = g_variant_new_variant(str);
        props[nprops++] = g_variant_new_dict_entry(key, var);

        GVariant *array = g_variant_new_array(G_VARIANT_TYPE("{sv}"), props, nprops);

        GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a{sv})"));
        g_variant_builder_add_value(builder, array);
        GVariant *args = g_variant_builder_end(builder);
        g_variant_builder_unref(builder);
        g_dbus_method_invocation_return_value(invocation, args);

        if(call)
            delete call;
    }
    else if(!strcmp(method_name, "Synchronize")) {
        LoggerD("Synchronizing data with the phone");
        phone->createSession(phone->mWantedRemoteDevice.c_str());
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(!strcmp(method_name, "GetContacts")) {
        unsigned long count;
        g_variant_get(parameters, "(u)", &count);
        std::string contacts;
        phone->getJsonContacts(contacts, count);
        g_dbus_method_invocation_return_value( invocation,
                                               g_variant_new("(s)", contacts.c_str()));
    }
    else if(!strcmp(method_name, "GetCallHistory")) {
        unsigned long count;
        g_variant_get(parameters, "(u)", &count);
        std::string calls;
        phone->getJsonCallHistory(calls, count);
        g_dbus_method_invocation_return_value( invocation,
                                               g_variant_new("(s)", calls.c_str()));
    }
}

gboolean Phone::delayedSyncCallHistory(gpointer user_data) {
    Phone *ctx = static_cast<Phone*>(user_data);
    if(!ctx) {
        LoggerE("Failed to cast to Phone");
        return G_SOURCE_REMOVE; // single shot timeout
    }
    // synchronize given number of latest entries in the list
    if(Obex::OBEX_ERR_INVALID_SESSION == ctx->syncCallHistory(CALLHISTORY_UPDATED_SYNC_COUNT)) {
        // if the session is not created, try to create it
        ctx->createSession(ctx->mWantedRemoteDevice.c_str());
    }
    return G_SOURCE_REMOVE; // single shot timeout
}

void Phone::callChanged(const char* state, const char* phoneNumber) {
    LoggerD("CallChanged: " << state << "\t" << phoneNumber);

    if(state && !strcmp(state, "disconnected")) {
        // a call has been made => update call history
        // use a delayed sync, since the list may not be updated on the phone yet
        g_timeout_add(DELAYED_SYNC_CALLHISTORY_INTERVAL, delayedSyncCallHistory, this);
    } else if (state && !strcmp(state, "incoming")) {
        // Launch dialer if Modello is not running since Modello includes a phone app
        if (!aul_app_is_running(MODELLO_APP_ID)) {
            LoggerD("Modello is not running");
            if (aul_open_app(DIALER_APP_ID) < 0)
                LoggerD("Failed to launch dialer");
        }
    }

    GVariant *props[8];
    int nprops = 0;
    GVariant *key, *str, *var;
    // add state
    key = g_variant_new_string("state");
    str = g_variant_new_string(state?state:"disconnected");
    var = g_variant_new_variant(str);
    props[nprops++] = g_variant_new_dict_entry(key, var);
    // add line_id
    key = g_variant_new_string("line_id");
    str = g_variant_new_string(phoneNumber?phoneNumber:"");
    var = g_variant_new_variant(str);
    props[nprops++] = g_variant_new_dict_entry(key, var);
    // get contact by phone number
    std::string contact;
    getContactByPhoneNumber(phoneNumber, contact);
    key = g_variant_new_string("contact");
    str = g_variant_new_string(contact.c_str());
    var = g_variant_new_variant(str);
    props[nprops++] = g_variant_new_dict_entry(key, var);

    GVariant *array = g_variant_new_array(G_VARIANT_TYPE("{sv}"), props, nprops);

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a{sv})"));
    g_variant_builder_add_value(builder, array);
    GVariant *args = g_variant_builder_end(builder);
    g_variant_builder_unref(builder);

    g_dbus_connection_emit_signal( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                   NULL,
                                   PHONE_OBJ_PATH,
                                   PHONE_IFACE,
                                   "CallChanged",
                                   args,
                                   NULL);
}

bool Phone::storeSelectedRemoteDeviceMAC(const std::string &btAddress) {

    char *home = ::getenv("HOME");
    if(!home)
        return false;

    char fileName[32];
    snprintf(fileName, sizeof(fileName), "%s/.phoned", home);

    LoggerD("storing MAC address " << btAddress << " to " << fileName);

    std::ofstream cfgfile (fileName);
    if (cfgfile.is_open())
    {
        cfgfile << btAddress;
        cfgfile.close();

        return true;
    }

    return false;
}

bool Phone::readSelectedRemoteDeviceMAC(std::string &btAddress) {
    char *home = ::getenv("HOME");
    if(!home)
        return false;

    char fileName[32];
    snprintf(fileName, sizeof(fileName), "%s/.phoned", home);

    std::ifstream cfgfile (fileName);
    if (cfgfile.is_open()) {
        getline(cfgfile, btAddress); // no need to validate MAC, - it's done, when MAC is stored
        LoggerD("Re-stored MAC address " << btAddress << " from " << fileName);
        cfgfile.close();
        return true;
    }

    return false;
}

} // PhoneD

