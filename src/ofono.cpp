#include "ofono.h"
#include "utils.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <stdlib.h>

#include "Logger.h"

namespace PhoneD {

#define OFONO_PREFIX                     "org.ofono"

#define OFONO_SERVICE                    OFONO_PREFIX
#define OFONO_MANAGER_IFACE              OFONO_PREFIX ".Manager"
#define OFONO_MODEM_IFACE                OFONO_PREFIX ".Modem"
#define OFONO_VOICECALLMANAGER_IFACE     OFONO_PREFIX ".VoiceCallManager"
#define OFONO_VOICECALL_IFACE            OFONO_PREFIX ".VoiceCall"
#define OFONO_CALLVOLUME_IFACE           OFONO_PREFIX ".CallVolume"

#define CHECK_FOR_MODEM_POWERED_INTERVAL 60 // interval for check whether 'Selected' remote device is Powered, and power it on, if it is not (in seconds)

OFono::OFono() :
    mModemPath( NULL ),
    mActiveCall( NULL )
{
    LoggerD("entered");

    /*
    if(getModems()) {
        LoggerD("Failed to call 'GetModems'");
    }
    */
    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_MANAGER_IFACE, "/",
                             "ModemAdded", OFono::handleSignal,
                             this);
    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_MANAGER_IFACE, "/",
                             "ModemRemoved", OFono::handleSignal,
                             this);

    // won't request calls here, since the service OFONO_VOICECALLMANAGER_IFACE may not be available yet
    //get active calls
    //if(mModemPath) {
    //    getCalls();
    //}

    // periodic check whether modem for 'Selected' remote device is powered
    // among other things, it handles also the case when the phone gets out of the range of HMI BT
    g_timeout_add(CHECK_FOR_MODEM_POWERED_INTERVAL*1000, OFono::checkForModemPowered, this);
}

OFono::~OFono() {
    LoggerD("entered");
    removeModem(mModemPath);
}

gboolean OFono::checkForModemPowered(gpointer user_data) {
    OFono *ctx = static_cast<OFono*>(user_data);
    if(!ctx || !ctx->mModemPath)
        return G_SOURCE_CONTINUE; // continuous timeout

    if(!ctx->isModemPowered(ctx->mModemPath)) {
        ctx->setModemPowered(ctx->mModemPath, true);
    }

    return G_SOURCE_CONTINUE; // continuous timeout
}

bool OFono::isModemPowered(const char *modem) {
    if(modem)
        return false;

    GError *err = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                         OFONO_SERVICE,
                                         modem,
                                         OFONO_MODEM_IFACE,
                                         "GetProperties",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);


    if(err || !reply) {
        if(err)
            g_error_free(err);
        return false;
    }

    GVariant *prop = NULL;
    GVariantIter *iter = NULL;
    g_variant_get(reply, "(a{sv})", &iter);
    bool powered = false;
    while((prop = g_variant_iter_next_value(iter))) {
        const char *name = NULL;
        GVariant *value = NULL;
        g_variant_get(prop, "{sv}", &name, &value);
        if(name && !strcmp(name, "Powered")) {
            powered = g_variant_get_boolean(value);
        }
    }

    g_variant_unref(reply);

    return powered;
}

// synchronous version of method for setting property
bool OFono::setProperty(const char* iface, const char* path, const char *property, int type, void *value) {
    LoggerD("OFono::setProperty(): name= " << property << " on " << path);

    char signature[2];
    sprintf(signature, "%c", type); // eg. "b" for boolean

    GError *err = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                                 OFONO_SERVICE,
                                 path,
                                 iface,
                                 "SetProperty",
                                 g_variant_new ("(sv)", // floating parameters are consumed, no cleanup/unref needed
                                     property,
                                     g_variant_new(signature, value) // floating parameters are consumed, no cleanup/unref needed
                                 ),
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &err);

    if(err) {
        LoggerE("Failed to call \"SetProperty\" DBUS method: " << err->message);
        g_error_free(err);
        return false;
    }

    return true;
}

// asynchronous version of method for setting property
void OFono::setPropertyAsync(const char* iface, const char* path, const char *property, int type, void *value, GAsyncReadyCallback callback) {
    LoggerD("Setting OFono property via asynchronous call: name= " << property << " on " << path);

    char signature[2];
    sprintf(signature, "%c", type); // eg. "b" for boolean

    g_dbus_connection_call( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                            OFONO_SERVICE,
                            path,
                            iface,
                            "SetProperty",
                            g_variant_new ("(sv)", // floating parameters are consumed, no cleanup/unref needed
                                property,
                                g_variant_new(signature, value) // floating parameters are consumed, no cleanup/unref needed
                            ),
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            callback,
                            this);
}

// callback for async calls on 'g_dbus'
void OFono::GDbusAsyncReadyCallback(GObject *source, GAsyncResult *result, gpointer user_data) {
    //process result here if needed
    //LoggerD("Async method call finished, ie. callback received");
}

void OFono::setModemPowered(const char *modem, bool powered) {
    LoggerD("Setting modem 'Powered': " << modem << " to " << powered);
    setPropertyAsync(OFONO_MODEM_IFACE, modem, "Powered", DBUS_TYPE_BOOLEAN, &powered, OFono::asyncSetModemPoweredCallback);
}

void OFono::asyncSetModemPoweredCallback(GObject *source, GAsyncResult *result, gpointer user_data) {
    GError *err = NULL;
    g_dbus_connection_call_finish(g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL), result, &err);
    if(err) {
        LoggerE("Failed to set 'Powered' property on modem: " << err->message);
        OFono *ctx = static_cast<OFono*>(user_data);
        if(!ctx) {
            LoggerE("Invalid ctx");
            return;
        }
        // notify about the failure
        if(!ctx->isModemPowered(ctx->mModemPath)) // Modem is not 'Powered', ie. the remote BT device is not 'Selected' for BT operations
            ctx->setModemPoweredFailed(err?err->message:"Failed to set 'Powered' property on Modem.");
        g_error_free(err);
    }
    else
        LoggerE("Property 'Powered' successfuly set on modem");
}

void OFono::asyncSelectModemCallback(GObject *source, GAsyncResult *result, gpointer user_data) {
    CtxCbData *data = static_cast<CtxCbData*>(user_data);
    if(!data || !data->ctx || !data->data1) {
        LoggerE("Invalid callback data");
        return;
    }
    OFono *ctx = static_cast<OFono*>(data->ctx);
    char *addr = static_cast<char*>(data->data1);

    std::string btAddress = addr;

    free(addr);
    delete data;

    GError *err = NULL;
    GVariant *reply;
    reply = g_dbus_connection_call_finish(g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL), result, &err);
    if(err || !reply) {
        if(err) {
            LoggerE("Failed to 'GetModems': " << err->message);
            g_error_free(err);
        }
        else if(!reply)
            LoggerE("Reply from calling 'GetModems' method is NULL");

        return;
    }

    // path of retrieved modems is in form: /hfp/AABBCCDDEEFF_FFEEDDCCBBAA
    // where: AABBCCDDEEFF is MAC address of default local BT device
    //   and: FFEEDDCCBBAA is MAC address of selected remote BT device
    // we expect given MAC address to be in the form: AA:BB:CC:DD:EE:FF
    // remove all non-digit characters, eg. ":","-"
    btAddress.erase(std::remove_if(btAddress.begin(), btAddress.end(), isnxdigit), btAddress.end());

    GVariantIter *modems;
    GVariantIter *props;
    const char *path;
    g_variant_get(reply, "(a(oa{sv}))", &modems);
    while(g_variant_iter_next(modems, "(oa{sv})", &path, &props)) {
        // check if the modem is from selected remote device
        std::string pathString(path);
        size_t idx = pathString.find( "_" ) + 1; // index of address of remote device
        std::string modemRemoteBtAddress = pathString.substr (idx, pathString.length()-idx);
        if(!modemRemoteBtAddress.compare(btAddress)) {
            ctx->addModem(path, props);
            // currently only one modem is supported - that's why the break is here
            // take the first one from the list and make it 'default'
            break;
        }
    }

    g_variant_unref(reply);
}

void OFono::selectModem(std::string &btAddress) {
    LoggerD("Selecting modem: " << btAddress);
    // Retrieving list of available modems to get the one that is for 'wanted' device
    g_dbus_connection_call( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                            OFONO_SERVICE,
                            "/",
                            OFONO_MANAGER_IFACE,
                            "GetModems",
                            NULL,
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            OFono::asyncSelectModemCallback,
                            new CtxCbData(this, NULL, strdup(btAddress.c_str()), NULL));
}

void OFono::addModem(const char *path, GVariantIter *props) {
    LoggerD("entered");

    if(!path || !props)
        return;

    // shouldn't happen, but just in case free the memory here
    if(mModemPath)
        free(mModemPath);

    mModemPath = strdup(path); // make a copy of path, 'cause it will be unref-ed

    const char *key = NULL;
    GVariant *value = NULL;
    bool online = FALSE;
    while(g_variant_iter_next(props, "{sv}", &key, &value)) {
        if(!strcmp(key, "Powered")) {
            //bool powered = g_variant_get_boolean(value);
        }
        else if(!strcmp(key, "Online")) {
            online = g_variant_get_boolean(value);
        }
    }

    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_VOICECALLMANAGER_IFACE, mModemPath,
                             "CallAdded", OFono::handleSignal,
                             this);

    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_VOICECALLMANAGER_IFACE, mModemPath,
                             "PropertyChanged", OFono::handleSignal,
                             this);

    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_VOICECALLMANAGER_IFACE, mModemPath,
                             "CallRemoved", OFono::handleSignal,
                             this);

    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_MODEM_IFACE, mModemPath,
                             "PropertyChanged", OFono::handleSignal,
                             this);

    if(!online) {
        // power on modem
        setModemPowered(mModemPath, true);
    }
    else
    {
        // we can query calls from here, since the modem is already powered
        getCalls();
        // when the modem is already online, "PropertyChanged" for "Paired" is not emited
        // to handle all functionality to modem being powered, call modemPowered from here as well
        modemPowered(true);
    }
}

void OFono::unselectModem() {
    removeModem(mModemPath);
}

void OFono::removeModem(const char *modemPath) {
    LoggerD("Removing modem: " << (modemPath?modemPath:"INVALID DATA"));

    // remove all subscriptions to DBUS signals from the modem
    if(modemPath) {
        Utils::removeSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                                    OFONO_VOICECALLMANAGER_IFACE, modemPath,
                                    "CallAdded");
        Utils::removeSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                                    OFONO_VOICECALLMANAGER_IFACE, modemPath,
                                    "PropertyChanged");
        Utils::removeSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                                    OFONO_VOICECALLMANAGER_IFACE, modemPath,
                                    "CallRemoved");
        Utils::removeSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                                    OFONO_MODEM_IFACE, modemPath,
                                    "PropertyChanged");

        // power off modem
        setModemPowered(modemPath, false);
        // at this point we will not get notification "PropertyChanged" on 'Powered' property,
        // call 'modemPowered()' method directly from here
        modemPowered(false);

        // free the memory
        if(modemPath && mModemPath && !strcmp(modemPath, mModemPath)) {
            free(mModemPath);
            mModemPath = NULL;
        }
    }
}

void OFono::getCalls()
{
    LoggerD("entered");

    GError *err = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                         OFONO_SERVICE,
                                         mModemPath,
                                         OFONO_VOICECALLMANAGER_IFACE,
                                         "GetCalls",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);


    if(err || !reply) {
        if(err) {
            LoggerE("error calling GetCalls method");
            g_error_free(err);
        }
        else if(!reply)
            LoggerE("reply is NULL");

        return;
    }

    GVariantIter *calls;
    GVariantIter *props;
    const char *path;
    g_variant_get(reply, "(a(oa{sv}))", &calls);
    while(g_variant_iter_next(calls, "(oa{sv})", &path, &props)) {
        addCall(path, props);
    }

    g_variant_unref(reply);

    return;
}

void OFono::handleSignal(GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *signal_name,
                         GVariant              *parameters,
                         gpointer               user_data)
{
    LoggerD("signal received: \"" << interface_name << "\" -> \"" << signal_name << "\"");

    OFono* ctx = static_cast<OFono*>(user_data);
    if(!ctx) {
        LoggerE("Failed to cast object");
        return;
    }

    if(!interface_name || !signal_name) {
        LoggerE("Invalid callback data");
        return;
    }
    if(!strcmp(interface_name, OFONO_MANAGER_IFACE)) {
        if(!strcmp(signal_name, "ModemAdded")) {
            const char *modem = NULL;
            GVariantIter *props = NULL;
            g_variant_get(parameters, "(oa{sv})", &modem, &props);
            if(modem) {
                std::string modemString(modem);
                size_t idx = modemString.find( "_" ) + 1; // index of address of remote device
                std::string modemRemoteBtAddress = modemString.substr (idx, modemString.length()-idx);
                if(makeMACFromRawMAC(modemRemoteBtAddress)) {
                    ctx->modemAdded(modemRemoteBtAddress);
                }
            }
        }
    }
    else if(!strcmp(interface_name, OFONO_MANAGER_IFACE)) {
        if(!strcmp(signal_name, "ModemRemoved")) {
            const char *modem = NULL;
            g_variant_get(parameters, "(o)", &modem);
            if(modem) {
                LoggerD("Modem removed: " << modem);
                if(ctx->mModemPath && !strcmp(modem, ctx->mModemPath)) {
                    ctx->removeModem(ctx->mModemPath);
                }
            }
        }
    }
    else if(!strcmp(interface_name, OFONO_MODEM_IFACE)) {
        if(!strcmp(signal_name, "PropertyChanged")) {
            const char *name = NULL;
            GVariant *value = NULL;
            g_variant_get(parameters, "(sv)", &name, &value);
            if(!strcmp(name, "Powered")) {
                bool powered = g_variant_get_boolean(value);
                LoggerD("\t" << name << " = " << (powered?"TRUE":"FALSE"));
                ctx->modemPowered(powered);
                // !!! won't request calls here, since the service may not be available yet
                // see "Interfaces" "PropertyChanged" on OFONO_MODEM_IFACE
                //if(powered)
                //    ctx->getCalls();
            }
            else if(!strcmp(name, "Online")) {
                bool online = g_variant_get_boolean(value);
                LoggerD("\t" << name << " = " << (online?"TRUE":"FALSE"));
            }
            else if(!strcmp(name, "Interfaces")) {
                GVariantIter iter;
                GVariant *interface;
                GVariant *variant = g_variant_get_child_value(parameters,1); // signature: "v"
                GVariant *interfaces = g_variant_get_child_value(variant,0); // signature: "as"
                g_variant_iter_init(&iter, interfaces);
                while((interface = g_variant_iter_next_value(&iter))) {
                    const char *iface = g_variant_get_string(interface, NULL);
                    if(!strcmp(iface, OFONO_VOICECALLMANAGER_IFACE)) {
                        //TODO: ??? check if the service is newly added ??? - not to request calls multiple times
                        // service is up, request active calls now
                        ctx->getCalls();
                    }
                };
            }
        }
    }
    else if(!strcmp(interface_name, OFONO_VOICECALLMANAGER_IFACE)) {
        if(!strcmp(signal_name, "CallAdded")) {
            if(!parameters) {
                LoggerE("Invalid parameters");
                return;
            }

            const char *path = NULL;
            GVariantIter *props = NULL;
            g_variant_get(parameters, "(oa{sv})", &path, &props);
            ctx->addCall(path, props);
        }
        else if(!strcmp(signal_name, "CallRemoved")) {
            if(!parameters) {
                LoggerD("Invalid parameters");
                return;
            }
            // TODO: do the check for the call
            // for now, remove just the active one
            ctx->removeCall(&ctx->mActiveCall);
        }
        else {
            LoggerD("un-handled signal \"" << signal_name << "\" on " << interface_name);
        }
    }
    else if(!strcmp(interface_name, OFONO_VOICECALL_IFACE)) {
        if(!strcmp(signal_name, "PropertyChanged")) {
            const char *key = NULL;
            GVariant *value = NULL;
            g_variant_get(parameters, "(sv)", &key, &value);
            if(!strcmp(key, "State")) {
                const char *state = NULL;
                g_variant_get(value, "s", &state);
                if(ctx->mActiveCall->state)
                    g_free(ctx->mActiveCall->state);
                ctx->mActiveCall->state = strdup(state);
                LoggerD("PROPERTY CHANGED: " << key << " = " << state);
            }
            else {
                LoggerD("PROPERTY CHANGED: " << key);
            }
            // TODO: check for the remaining properties
            ctx->callChanged(ctx->mActiveCall->state, ctx->mActiveCall->line_id);
        }
    }
    else {
        LoggerD("signal received for un-handled IFACE: " << interface_name);
    }
}

void OFono::addCall(const char *path, GVariantIter *props) {
    LoggerD("entered");

    // only one call at a time is curently supported
    if(mActiveCall) {
        LoggerD("Unable to add a call when another is on-going");
        return;
    }

    if(!path)
        return;

    mActiveCall = new OFono::Call();
    mActiveCall->path = strdup(path);

    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                             OFONO_VOICECALL_IFACE, path,
                             "PropertyChanged", OFono::handleSignal,
                             this);

    const char *key = NULL;
    GVariant *value = NULL;
    while(g_variant_iter_next(props, "{sv}", &key, &value)) {
        if(!strcmp(key, "State")) {
            const char *state = NULL;
            g_variant_get(value, "s", &state);
            mActiveCall->state = state ? strdup(state) : NULL;
        }
        else if(!strcmp(key, "LineIdentification")) {
            const char *line_id = NULL;
            g_variant_get(value, "s", &line_id);
            mActiveCall->line_id = line_id ? strdup(line_id) : NULL;
        }
    }

    // notify listener about call state changed
    if(!!mActiveCall->state && !!mActiveCall->line_id) {
        callChanged(mActiveCall->state, mActiveCall->line_id);
    }
}

void OFono::removeCall(OFono::Call **call) {
    LoggerD("entered");

    // unsubscribe and remove signal listeners for the call
    Utils::removeSignalListener(G_BUS_TYPE_SYSTEM, OFONO_SERVICE,
                                OFONO_VOICECALL_IFACE, (*call)->path,
                                "PropertyChanged");

    if(*call) {
        delete *call;
        *call = NULL;
    }
}

bool OFono::invokeCall(const char* phoneNumber, char **error) {
    if(!mModemPath) { // no selected modem to perform operation on
        *error = strdup("No active modem set");
        return false;
    }

    if(mActiveCall) { // active call
        *error = strdup("Already active call");
        return false;
    }

    GError *err = NULL;
    GVariant *reply;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                                         OFONO_SERVICE,
                                         mModemPath,
                                         OFONO_VOICECALLMANAGER_IFACE,
                                         "Dial",
                                         g_variant_new ("(ss)", // floating parameters are consumed, no cleanup/unref needed
                                             phoneNumber,
                                             ""
                                         ),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);

    if(err) {
        LoggerE("Failed to call 'Dial' DBUS method: " << err->message);
        *error = strdup(err->message);
        g_error_free(err);
        return false;
    }

    if(!reply) {
        LoggerE("Reply from calling 'Dial' is NULL");
        *error = strdup("Failed to get 'call' object from OFONO's 'Dial' reply");
        return false;
    }

    // TODO: process reply here - ??? is it really needed 'cause we are handling CallAdded signal anyway ???
    g_variant_unref(reply);

    return true;
}

bool OFono::answerCall(char **error) {
    if(!mActiveCall) { // no active call to hangup
        *error = strdup("No active call");
        return false;
    }

    GError *err = NULL;
    g_dbus_connection_call_sync (g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                    OFONO_SERVICE,
                    mActiveCall->path,
                    OFONO_VOICECALL_IFACE,
                    "Answer",
                    NULL,
                    NULL,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    NULL,
                    &err);

    if(err) {
        LoggerD("Failed to call 'Answer' DBUS method: " << err->message);
        *error = strdup(err->message);
        g_error_free(err);
        return false;
    }

    return true;
}

bool OFono::hangupCall(char **error) {
    if(!mActiveCall) { // no active call to hangup
        *error = strdup("No active call");
        return false;
    }

    GError *err = NULL;
    g_dbus_connection_call_sync (g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                    OFONO_SERVICE,
                    mActiveCall->path,
                    OFONO_VOICECALL_IFACE,
                    "Hangup",
                    NULL,
                    NULL,
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    NULL,
                    &err);

    if(err) {
        LoggerD("Failed to call 'Hangup' DBUS method: " << err->message);
        *error = strdup(err->message);
        g_error_free(err);
        return false;
    }

    return true;
}

OFono::Call *OFono::activeCall() {
    LoggerD("OFono::activeCall()");
    // make a copy of object, since it may be destroyed meanwhile
    return mActiveCall ? new OFono::Call(mActiveCall) : NULL;
}

bool OFono::muteCall(bool mute, char **error) {
    if(!mActiveCall) { // no active call to hangup
        *error = strdup("No active call");
        return false;
    }

    bool success = setProperty(OFONO_CALLVOLUME_IFACE, mModemPath, "Muted", DBUS_TYPE_BOOLEAN, &mute);
    if(!success) {
        *error = strdup("Failed to set 'Muted' property on the call");
        return false;
    }

    return true;
}

} // PhoneD

