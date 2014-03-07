
#include "connman.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

#include "Logger.h"

namespace PhoneD {

#define CONNMAN_PREFIX                     "net.connman"

#define CONNMAN_SERVICE                    CONNMAN_PREFIX
#define CONNMAN_MANAGER_IFACE              CONNMAN_PREFIX ".Manager"
#define CONNMAN_TECHNOLOGY_IFACE           CONNMAN_PREFIX ".Technology"

ConnMan::ConnMan() :
    mBluetoothPowered(false)
{
    char *bluetooth = getBluetoothTechnology();
    if(bluetooth) {
        Utils::setSignalListener(G_BUS_TYPE_SYSTEM, CONNMAN_SERVICE, CONNMAN_TECHNOLOGY_IFACE,
                                 bluetooth, "PropertyChanged", ConnMan::handleSignal,
                                 this);
        free(bluetooth);
    }
}

ConnMan::~ConnMan() {
    char *bluetooth = getBluetoothTechnology();
    if(bluetooth) {
        Utils::removeSignalListener(G_BUS_TYPE_SYSTEM, CONNMAN_SERVICE, CONNMAN_TECHNOLOGY_IFACE, bluetooth, "PropertyChanged");
        free(bluetooth);
    }
}

void ConnMan::init() {
    char *bluetooth = getBluetoothTechnology();
    if(bluetooth)
        free(bluetooth);
}

char *ConnMan::getBluetoothTechnology() {
    GError *err = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                                         CONNMAN_SERVICE,
                                         "/",
                                         CONNMAN_MANAGER_IFACE,
                                         "GetTechnologies",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &err);

    if(err || !reply) {
        if(err) {
            LoggerE("Failed to call \"GetTechnologies\" DBUS method: " << err->message);
            g_error_free(err);
        }
        else if(!reply)
            LoggerE("Reply from \"GetTechnologies\" DBUS method is NULL");
        return NULL;
    }

    char *technology = NULL, *result = NULL;
    GVariantIter *props = NULL;
    GVariantIter *technologies = NULL;
    g_variant_get(reply, "(a(oa{sv}))", &technologies);
    while(g_variant_iter_next(technologies, "(oa{sv})", &technology, &props)) {
        if(technology && strstr(technology, "bluetooth")) {
            result = strdup(technology);
            const char *key;
            GVariant *value;
            while(g_variant_iter_next(props, "{sv}", &key, &value)) {
                if(!strcmp(key, "Powered")) {
                    bool powered = g_variant_get_boolean(value);
                    LoggerD("powered = " << powered);
                    mBluetoothPowered = powered;
                    break;
                }
            }
            break;
        }
    }

    g_variant_unref(reply);
    return result;
}

bool ConnMan::setBluetoothPowered(bool value) {
    char *bluetooth = getBluetoothTechnology();
    if(bluetooth) {
        GError *err = NULL;
        g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
                                     CONNMAN_SERVICE,
                                     bluetooth,
                                     CONNMAN_TECHNOLOGY_IFACE,
                                     "SetProperty",
                                     g_variant_new ("(sv)", // floating parameters are consumed, no cleanup/unref needed
                                         "Powered",
                                         g_variant_new_boolean(value)
                                     ),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL,
                                     &err);

        if(err) {
            if((value  && strstr(err->message, "Already enabled")) || // it's not an error, 'casue the BT is already Powered ON
               (!value && strstr(err->message, "Already disabled")))  // it's not an error, 'cause the BT is already Powered OFF
            {
                free(bluetooth);
                g_error_free(err);
                return true;
            }
            LoggerE("Failed to call \"SetProperty\" DBUS method: " << err->message);
            free(bluetooth);
            g_error_free(err);
            return false;
        }

        free(bluetooth);
        return true;
    }

    return false;
}

void ConnMan::handleSignal(GDBusConnection  *connection,
                         const gchar      *sender,
                         const gchar      *object_path,
                         const gchar      *interface_name,
                         const gchar      *signal_name,
                         GVariant         *parameters,
                         gpointer          user_data)
{
    LoggerD("signal received: '" << interface_name << "' -> '" << signal_name << "' -> '" << object_path << "'");

    ConnMan *ctx = static_cast<ConnMan*>(user_data);
    if(!ctx) {
        LoggerD("Failed to cast to ConnMan");
        return;
    }

    if(!strcmp(interface_name, CONNMAN_TECHNOLOGY_IFACE)) {
        if(!strcmp(signal_name, "PropertyChanged")) {
            const char *name;
            GVariant *value;
            g_variant_get(parameters, "(sv)", &name, &value);
            if(!strcmp(name, "Powered")) {
                bool powered = g_variant_get_boolean(value);
                ctx->mBluetoothPowered = powered;
                //LoggerD("\tBT Powered set to " << (powered?"TRUE":"FALSE"));
            }
        }
    }
}

} // PhoneD

