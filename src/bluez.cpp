#include "bluez.h"
#include "utils.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <dbus/dbus.h>

#include "Logger.h"

namespace PhoneD {

#define BLUEZ_PREFIX            "org.bluez"

#define BLUEZ_SERVICE           BLUEZ_PREFIX
#define BLUEZ_MANAGER_IFACE     BLUEZ_PREFIX ".Manager"
#define BLUEZ_ADAPTER_IFACE     BLUEZ_PREFIX ".Adapter"
#define BLUEZ_DEVICE_IFACE      BLUEZ_PREFIX ".Device"
#define BLUEZ_AGENT_IFACE       BLUEZ_PREFIX ".Agent"

#define AGENT_PATH              "/org/bluez/agent_poc"
#define AGENT_CAPABILITIES      "KeyboardDisplay"

#define AGENT_PASSKEY            123456
#define AGENT_PINCODE           "123456"

#define AGENT_INTERFACE_XML                                 \
    "<node>"                                                \
    "  <interface name='org.bluez.Agent'>"                  \
    "    <method name='Release'>"                           \
    "    </method>"                                         \
    "    <method name='Authorize'>"                         \
    "      <arg type='o' name='device' direction='in'/>"    \
    "      <arg type='s' name='uuid' direction='in'/>"      \
    "    </method>"                                         \
    "    <method name='RequestPinCode'>"                    \
    "      <arg type='o' name='device' direction='in'/>"    \
    "      <arg type='s' name='pincode' direction='out'/>"  \
    "    </method>"                                         \
    "    <method name='RequestPasskey'>"                    \
    "      <arg type='o' name='device' direction='in'/>"    \
    "      <arg type='u' name='passkey' direction='out'/>"  \
    "    </method>"                                         \
    "    <method name='DisplayPasskey'>"                    \
    "      <arg type='o' name='device' direction='in'/>"    \
    "      <arg type='u' name='passkey' direction='in'/>"   \
    "    </method>"                                         \
    "    <method name='DisplayPinCode'>"                    \
    "      <arg type='o' name='device' direction='in'/>"    \
    "      <arg type='s' name='pincode' direction='in'/>"   \
    "    </method>"                                         \
    "    <method name='RequestConfirmation'>"               \
    "      <arg type='o' name='device' direction='in'/>"    \
    "      <arg type='u' name='passkey' direction='in'/>"   \
    "    </method>"                                         \
    "    <method name='ConfirmModeChange'>"                 \
    "      <arg type='s' name='mode' direction='in'/>"      \
    "    </method>"                                         \
    "    <method name='Cancel'>"                            \
    "    </method>"                                         \
    "  </interface>"                                        \
    "</node>"

/* NOTE:
 * "Release"             ... does nothing
 * "Authorize"           ... automatically authorized
 * "RequestPinCode"      ... used default pin code (AGENT_PINCODE)
 * "RequestPasskey"      ... used default passkey (AGENT_PASSKEY)
 * "RequestConfirmation" ... automatically confirmed
 * "DisplayPinCode"      ... does nothing
 * "DisplayPasskey"      ... does nothing
 * "ConfirmModeChange"   ... automatically confirmed
 * "Cancel"              ... does nothing
 */

Bluez::Bluez() :
    mAdapterPath(NULL),
    mAgentRegistrationId(-1),
    mAgentIntrospectionData(NULL)
{
    LoggerD("entered");

    mAdapterPath = getDefaultAdapter();
    if(!mAdapterPath) {
        LoggerE("Unable to get default adapter");
    }
    memset(&mAgentIfaceVTable, 0, sizeof(mAgentIfaceVTable));

    // subscribe for AdapterAdded/AdapterRemoved to get notification about the change
    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, BLUEZ_MANAGER_IFACE,
                             "/", "AdapterAdded", Bluez::handleSignal,
                             this);
    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, BLUEZ_MANAGER_IFACE,
                             "/", "AdapterRemoved", Bluez::handleSignal,
                             this);

    if(mAdapterPath) {
        Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, BLUEZ_ADAPTER_IFACE,
                                 mAdapterPath, "DeviceCreated", Bluez::handleSignal,
                                 this);
        Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, BLUEZ_ADAPTER_IFACE,
                                 mAdapterPath, "DeviceRemoved", Bluez::handleSignal,
                                 this);
        Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, BLUEZ_ADAPTER_IFACE,
                                 mAdapterPath, "PropertyChanged", Bluez::handleSignal,
                                 this);
    }
}

Bluez::~Bluez() {
    if(mAdapterPath) {
        free(mAdapterPath);
        mAdapterPath = NULL;
    }
}

gchar* Bluez::getDefaultAdapter()
{
    GError *err = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                         BLUEZ_SERVICE,
                                         "/",
                                         BLUEZ_MANAGER_IFACE,
                                         "DefaultAdapter",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);
    if(err || !reply) {
        if(err) {
            LoggerE("Failed to get default adapter: " << err->message);
            g_error_free(err);
        }
        if(!reply)
            LoggerE("Reply from 'DefaultAdapter' is null");
        return NULL;
    }

    char *adapter = NULL;
    g_variant_get(reply, "(o)", &adapter);
    LoggerD("DefaultAdapter: " << adapter);

    // make a copy of adapter, 'cause it will be destroyed when 'reply' is un-refed
    char *result = adapter?strdup(adapter):NULL;

    g_variant_unref(reply);

    return result;
}

bool Bluez::setAdapterPowered(bool value) {
    if(mAdapterPath) {
        GError *err = NULL;
        g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                                     BLUEZ_SERVICE,
                                     mAdapterPath,
                                     BLUEZ_ADAPTER_IFACE,
                                     "SetProperty",
                                     g_variant_new ("(sv)", // floating parameters are consumed, no cleanup/unref needed
                                         "Powered",
                                         g_variant_new("b", &value) // floating parameters are consumed, no cleanup/unref needed
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

    return false;
}

void Bluez::handleSignal(GDBusConnection  *connection,
                         const gchar      *sender,
                         const gchar      *object_path,
                         const gchar      *interface_name,
                         const gchar      *signal_name,
                         GVariant         *parameters,
                         gpointer          user_data)
{
    LoggerD("signal received: '" << interface_name << "' -> '" << signal_name << "' -> '" << object_path << "'");

    Bluez *ctx = static_cast<Bluez*>(user_data);
    if(!ctx) {
        LoggerD("Failed to cast to Bluez");
        return;
    }

    if(!strcmp(interface_name, BLUEZ_MANAGER_IFACE)) {
        if(!strcmp(signal_name, "AdapterAdded")) {
            const char *adapter = NULL;
            g_variant_get(parameters, "(o)", &adapter);
            if(adapter) {
                LoggerD("Adapter added: " << adapter);
                if(!ctx->mAdapterPath) {
                    // make added adapter as default
                    ctx->mAdapterPath = strdup(adapter);
                    //ctx->setupAgent();
                    ctx->registerAgent();
                    ctx->defaultAdapterAdded();
                }
            }
        }
        else if(!strcmp(signal_name, "AdapterRemoved")) {
            const char *adapter = NULL;
            g_variant_get(parameters, "(o)", &adapter);
            if(adapter) {
                LoggerD("Adapter removed: " << adapter);
                if(ctx->mAdapterPath && !strcmp(ctx->mAdapterPath, adapter)) {
                    // removed the default adapter
                    free(ctx->mAdapterPath);
                    ctx->mAdapterPath = NULL;
                    ctx->defaultAdapterRemoved();
                }
            }
        }
    }
    else if(!strcmp(interface_name, BLUEZ_ADAPTER_IFACE)) {
        if(!strcmp(signal_name, "DeviceCreated")) {
            const char *device;
            g_variant_get(parameters, "(o)", &device);
            LoggerD("DeviceCreated: " << (device?device:"UNKNOWN"));

            // subscribe for PropertyChanged signal on the device,
            // to get notification about device being paired
            Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, BLUEZ_DEVICE_IFACE,
                                     device, "PropertyChanged", Bluez::handleSignal,
                                     ctx);

        }
        else if(!strcmp(signal_name, "DeviceRemoved")) {
            const char *device;
            g_variant_get(parameters, "(o)", &device);
            LoggerD("DeviceRemoved: " << (device?device:"UNKNOWN"));
            ctx->deviceRemoved(device);
        }
        else if(!strcmp(signal_name, "PropertyChanged")) {
            const char *name;
            GVariant *v_value;
            g_variant_get(parameters, "(sv)", &name, &v_value);
            LoggerD("\tname=" << name);
            if(!strcmp(name, "Powered")) {
                bool value = g_variant_get_boolean(v_value);
                ctx->adapterPowered(value);
                //LoggerD("\tvalue=" << (value?"TRUE":"FALSE"));
            }
        }
    }
    else if(!strcmp(interface_name, BLUEZ_DEVICE_IFACE)) {
        if(!strcmp(signal_name, "PropertyChanged")) {
            const char *name;
            GVariant *value;
            g_variant_get(parameters, "(sv)", &name, &value);
            if(!strcmp(name, "Paired")) {
                bool paired = g_variant_get_boolean(value);
                if(paired) { // the device has been paired
                    ctx->deviceCreated(object_path);
                }
            }
        }
    }
}

void Bluez::agentHandleMethodCall( GDBusConnection       *connection,
                                   const gchar           *sender,
                                   const gchar           *object_path,
                                   const gchar           *interface_name,
                                   const gchar           *method_name,
                                   GVariant              *parameters,
                                   GDBusMethodInvocation *invocation,
                                   gpointer               user_data)
{
    LoggerD("entered\n\tsender=" << sender << "\n\tobject_path=" << object_path << "\n\tinterface_name=" << interface_name << "\n\tmethod_name=" << method_name);

    Bluez *ctx = static_cast<Bluez*>(user_data);
    if(!ctx) {
        LoggerD("Failed to cast to Bluez");
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if(!strcmp(method_name, "Authorize")) {
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(!strcmp(method_name, "RequestPinCode")) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", AGENT_PINCODE));
    }
    else if(!strcmp(method_name, "RequestPasskey")) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", AGENT_PASSKEY));
    }
    else if (!strcmp(method_name, "Release")) {
        if(!strcmp(object_path, AGENT_PATH)) { // released agent for pairing
            bool unregistered = g_dbus_connection_unregister_object(g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL), ctx->mAgentRegistrationId);
            if(unregistered)
                ctx->mAgentRegistrationId = -1;
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else {
        // DisplayPasskey, DisplayPinCode, RequestConfirmation, ConfirmModeChange, Cancel
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

bool Bluez::isDevicePaired(const char *bt_addr) {
    bool paired = false;

    if(!mAdapterPath)
        return paired;

    GError *err = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                         BLUEZ_SERVICE,
                                         mAdapterPath,
                                         BLUEZ_ADAPTER_IFACE,
                                         "FindDevice",
                                         g_variant_new("(s)", bt_addr),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);

    if(err || !reply) {
        if(err)
            g_error_free(err);
        return paired;
    }

    const char *tmp = NULL;
    g_variant_get(reply, "(o)", &tmp);
    if(!tmp) {
        g_variant_unref(reply);
        return paired;
    }

    char *device = strdup(tmp);
    g_variant_unref(reply);

    // get device properties and check if the device is Paired
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                         BLUEZ_SERVICE,
                                         device,
                                         BLUEZ_DEVICE_IFACE,
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
        free(device);
        return paired;
    }

    GVariantIter *iter;
    g_variant_get(reply, "(a{sv})", &iter);
    const char *key;
    GVariant *value;
    while(g_variant_iter_next(iter, "{sv}", &key, &value)) {
        if(!strcmp(key, "Paired")) {
            paired = g_variant_get_boolean(value);
            break;
        }
    }

    free(device);
    g_variant_unref(reply);

    return paired;
}

void Bluez::setupAgent()
{
    LoggerD("entered: registering agent " << AGENT_PATH);

    /*
    if(mAgentRegistrationId > 0) { // alread registered
        LoggerD("Bluez agent registered");
        return;
    }
    */

    mAgentIfaceVTable.method_call = Bluez::agentHandleMethodCall;
    mAgentIntrospectionData = g_dbus_node_info_new_for_xml(AGENT_INTERFACE_XML, NULL);

    if (mAgentIntrospectionData == NULL) {
        LoggerD("failed to create introspection data.");
        return;
    }
    LoggerD("introspection data parsed OK");

    GError *err = NULL;
    mAgentRegistrationId = g_dbus_connection_register_object( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                                  AGENT_PATH,
                                                  mAgentIntrospectionData->interfaces[0],
                                                  &mAgentIfaceVTable, //const GDBusInterfaceVTable *vtable,
                                                  this, //user_data
                                                  NULL, //GDestroyNotify
                                                  &err);

    if(err) {
        LoggerD("Failed to register object: " << AGENT_PATH << " : " << err->message);
        g_error_free(err);
        return;
    }
    LoggerD("object registered with id=" << mAgentRegistrationId);
}

void Bluez::registerAgent()
{
    LoggerD("entered");

    GError *err = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
                                 BLUEZ_SERVICE,
                                 mAdapterPath,
                                 BLUEZ_ADAPTER_IFACE,
                                 "RegisterAgent",
                                 g_variant_new("(os)", AGENT_PATH, AGENT_CAPABILITIES), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &err);
    if(err) {
        LoggerE("Failed to register agent: " << err->message);
        g_error_free(err);
        return;
    }

    LoggerD("Agent registered");
}

} // PhoneD

