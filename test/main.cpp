
#include <stdio.h>
#include <string.h>
#include <gio/gio.h>

#include "../src/Logger.h"

#define TIZEN_PREFIX            "org.tizen"
#define PHONE_SERVICE           TIZEN_PREFIX ".phone"
#define PHONE_IFACE             TIZEN_PREFIX ".Phone"
#define PHONE_OBJ_PATH          "/"

//#define BT_ADDRESS "84:7E:40:31:58:42"
#define BT_ADDRESS "6C:A7:80:C8:F8:50" //pali
//#define BT_ADDRESS ""

void * command_handler(void *data);

char *_device = NULL; // to remember device for pairing

static void getBluetoothPowered();
static void selectRemoteDevice(const char* bt_address);
static void getSelectedRemoteDevice();
static void unselectRemoteDevice();
static void invokeCall(const char* phone_number);
static void answerCall();
static void muteCall(bool mute);
static void hangupCall();
static void activeCall();
static void synchronize();
static void getContacts();
static void getCallHistory();
static void restart();
static void pairDevice(const char* bt_address);

static void handleSignal( GDBusConnection *connection,
                          const gchar *sender_name,
                          const gchar *object_path,
                          const gchar *interface_name,
                          const gchar *signal_name,
                          GVariant *parameters,
                          gpointer user_data);

GMainLoop *loop = NULL;

int main (int argc, char *argv[])
{
    pthread_t thread;
    pthread_create(&thread, NULL, command_handler, NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "BluetoothPowered",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "CallChanged",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "CallHistoryEntryAdded",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "ContactsChanged",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "CallHistoryChanged",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "RemoteDeviceSelected",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "PairingResult",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "RequestPinCode",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "RequestPasskey",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    g_dbus_connection_signal_subscribe( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL,NULL),
                                        PHONE_SERVICE,
                                        PHONE_IFACE,
                                        "RequestConfirmation",
                                        NULL,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        handleSignal,
                                        NULL,//this,
                                        NULL);

    loop = g_main_loop_new(NULL, TRUE);
    g_main_loop_run(loop);

    return 0;
}

void * command_handler(void *data) {
    LoggerD("START: command_handler");

    char command[32];
    char argument[32];
    char input[128];
    while(true) {
        LoggerD("Waiting for command:");
        fgets(input, sizeof(input), stdin);
        int validargs = sscanf(input, "%s %s", command, argument);
        if(validargs == 1) {
            if(!strncmp(command, "exit", 4) || !strncmp(command, "quit", 4) || !strncmp(command, "q", 1))
                break;
            else if(!strncmp(command, "bt", 2))
                getBluetoothPowered();
            else if(!strncmp(command, "sel1", 4))
                selectRemoteDevice(BT_ADDRESS);
            else if(!strncmp(command, "sel2", 4))
                selectRemoteDevice("00:A7:80:C8:F8:00");
            else if(!strncmp(command, "uns", 3))
                unselectRemoteDevice();
            else if(!strncmp(command, "gsd", 3))
                getSelectedRemoteDevice();
            else if(!strncmp(command, "dial", 4))
                invokeCall("+421918362985");
            else if(!strncmp(command, "answer", 6))
                answerCall();
            else if(!strncmp(command, "hangup", 6))
                hangupCall();
            else if(!strncmp(command, "state", 5))
                activeCall();
            else if(!strncmp(command, "synchronize", 11))
                synchronize();
            else if(!strncmp(command, "contacts", 8))
                getContacts();
            else if(!strncmp(command, "history", 7))
                getCallHistory();
            else if(!strncmp(command, "restart", 7))
                restart();
            else if(!strncmp(command, "pair1", 5))
                pairDevice(BT_ADDRESS);
            else if(!strncmp(command, "pair2", 5))
                pairDevice("INVALID_MAC");
            else if(!strncmp(command, "help", 7)) {
                LoggerD("Commands:");
                LoggerD("\tdial");
                LoggerD("\tanswer");
                LoggerD("\tmute");
                LoggerD("\thangup");
                LoggerD("\tstate");
                LoggerD("\tsynchronize");
                LoggerD("\tcontacts");
                LoggerD("\thistory");
                LoggerD("\trestart");
            }
            else
                LoggerE("Invalid command: " << command);
        }
        else if(validargs == 2) {
            if(!strncmp(command, "bt", 2)) {
                bool powered = !strncmp(argument, "on", 2);
                LoggerD("setting BT Powered to: " << powered);
                GError *error = NULL;
                g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                             PHONE_SERVICE,
                                             PHONE_OBJ_PATH,
                                             PHONE_IFACE,
                                             "SetBluetoothPowered",
                                             g_variant_new("(v)", g_variant_new_boolean(powered)),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);
                if(error) {
                    LoggerD("Failed to call 'SetModemPowered': " << error->message);
                    g_error_free(error);
                }
            }
            if(!strncmp(command, "pin", 3)) {
                LoggerD("entered pin: " << argument);
                GError *error = NULL;
                g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                             PHONE_SERVICE,
                                             PHONE_OBJ_PATH,
                                             PHONE_IFACE,
                                             "RequestPinCodeReply",
                                             g_variant_new("(ss)", _device, argument), // floating variants are consumed
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);
                if(error) {
                    LoggerD("Failed to call 'RequestPinCodeReply'");
                    g_error_free(error);
                }
            }
            else if(!strncmp(command, "pass", 4)) {
                unsigned long passkey = atoi(argument);
                //LoggerD("entered pass: " << argument);
                printf("entered pass: %lu\n", passkey);
                GError *error = NULL;
                g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                             PHONE_SERVICE,
                                             PHONE_OBJ_PATH,
                                             PHONE_IFACE,
                                             "RequestPasskeyReply",
                                             g_variant_new("(su)", _device, passkey), // floating variants are consumed
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);
                if(error) {
                    LoggerD("Failed to call 'RequestPasskeyReply'");
                    g_error_free(error);
                }
            }
            else if(!strncmp(command, "conf", 4)) {
                unsigned long passkey = atoi(argument);
                bool confirmed = !strncmp(argument, "yes", 3);
                printf("entered confirmed: %s\n", confirmed?"YES":"NO");
                GError *error = NULL;
                g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                             PHONE_SERVICE,
                                             PHONE_OBJ_PATH,
                                             PHONE_IFACE,
                                             "RequestConfirmationReply",
                                             g_variant_new("(sv)", _device, g_variant_new_boolean(confirmed)),
                                             NULL,
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             &error);
                if(error) {
                    LoggerD("Failed to call 'RequestConfirmationReply'");
                    g_error_free(error);
                }
            }
            else if(!strncmp(command, "mute", 4)) {
                bool mute = !strncmp(argument, "on", 2);
                muteCall(mute);
            }
        }
        else
            LoggerE("Invalid number of arguments");
    };

    LoggerD("END: command_handler");
    g_main_loop_quit(loop);
}

void invokeCall(const char *phone_number) {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "Dial",
                                 g_variant_new("(s)", phone_number), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
    if(error) {
        LoggerE("Failed to dial number: " << error->message);
        g_error_free(error);
    }
}

void selectRemoteDevice(const char *bt_address) {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "SelectRemoteDevice",
                                 g_variant_new("(s)", bt_address), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);

    if(error) {
        LoggerE("Failed to request 'SelectRemoteDevice': " << error->message);
        g_error_free(error);
    }
}

void unselectRemoteDevice() {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "UnselectRemoteDevice",
                                 NULL,
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);

    if(error) {
        LoggerE("Failed to request 'UnselectRemoteDevice': " << error->message);
        g_error_free(error);
    }
}

void getSelectedRemoteDevice() {
    LoggerD("entered");

    GError *error = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                         PHONE_SERVICE,
                                         PHONE_OBJ_PATH,
                                         PHONE_IFACE,
                                         "GetSelectedRemoteDevice",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if(error || !reply) {
        if(error) {
            LoggerE("Failed to request GetSelectedRemoteDevice: " << error->message);
            g_error_free(error);
        }
        else if(!reply)
            LoggerE("reply is null");

        return;
    }

    char *btAddress = NULL;
    g_variant_get(reply, "(s)", &btAddress);
    LoggerD("Selected remote device: " << btAddress);

    g_variant_unref(reply);
}

void answerCall() {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "Answer",
                                 NULL,
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
    if(error) {
        LoggerE("Failed to answer call: " << error->message);
        g_error_free(error);
    }
}

void muteCall(bool mute) {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "Mute",
                                 g_variant_new("(b)", mute), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
    if(error) {
        LoggerE("Failed to mute call: " << error->message);
        g_error_free(error);
    }
}

void hangupCall() {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "Hangup",
                                 NULL,
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
    if(error) {
        LoggerE("Failed to hangup call: " << error->message);
        g_error_free(error);
    }
}

void activeCall() {
    LoggerD("entered");

    GError *error = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                         PHONE_SERVICE,
                                         PHONE_OBJ_PATH,
                                         PHONE_IFACE,
                                         "ActiveCall",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if(error || !reply) {
        if(error) {
            LoggerE("Failed to get active call: " << error->message);
            g_error_free(error);
        }
        else if(!reply)
            LoggerE("reply is null");

        return;
    }

    GVariantIter *iter;
    g_variant_get(reply, "(a{sv})", &iter);
    const char *key;
    GVariant *value;
    while(g_variant_iter_next(iter, "{sv}", &key, &value)) {
        if(!strcmp(key, "state")) {
            const char *state = g_variant_get_string(value, NULL);
            LoggerD("state: " << state);
        }
        else if(!strcmp(key, "line_id")) {
            const char *line_id = g_variant_get_string(value, NULL);
            LoggerD("line_id: " << line_id);
        }
        else if(!strcmp(key, "contact")) {
            const char *contact = g_variant_get_string(value, NULL);
            printf("contact: %s\n", contact);
        }
    }

    g_variant_unref(reply);
}

void synchronize() {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "Synchronize",
                                 NULL,
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
    if(error) {
        LoggerE("Failed to synchronize data with the phone: " << error->message);
        g_error_free(error);
    }
}

void getBluetoothPowered() {
    LoggerD("entered");

    GError *error = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                         PHONE_SERVICE,
                                         PHONE_OBJ_PATH,
                                         PHONE_IFACE,
                                         "GetBluetoothPowered",
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if(error || !reply) {
        if(error) {
            LoggerE("Failed to call 'GetBluetoothPowered': " << error->message);
            g_error_free(error);
        }
        else if(!reply)
            LoggerE("reply is null");

        return;
    }

    GVariant *value = NULL;
    g_variant_get(reply, "(v)", &value);
    bool powered = g_variant_get_boolean(value);

    LoggerD("Bluetooth powered: " << (powered?"ON":"OFF"));

    g_variant_unref(reply);
}

void getContacts() {
    LoggerD("entered");

    GError *error = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                         PHONE_SERVICE,
                                         PHONE_OBJ_PATH,
                                         PHONE_IFACE,
                                         "GetContacts",
                                         g_variant_new("(u)", 10),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if(error || !reply) {
        if(error) {
            LoggerE("Failed to request Contacts: " << error->message);
            g_error_free(error);
        }
        else if(!reply)
            LoggerE("reply is null");

        return;
    }

    char *contacts = NULL;
    g_variant_get(reply, "(s)", &contacts);
    //LoggerD("contacts: " << contacts); // !!! Logger is working with a buffer of 128 chars only
    printf("%s\n", contacts);

    g_variant_unref(reply);
}

void getCallHistory() {
    LoggerD("entered");

    GError *error = NULL;
    GVariant *reply = NULL;
    reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                         PHONE_SERVICE,
                                         PHONE_OBJ_PATH,
                                         PHONE_IFACE,
                                         "GetCallHistory",
                                         g_variant_new("(u)", 10),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if(error || !reply) {
        if(error) {
            LoggerE("Failed to request CallHistory: " << error->message);
            g_error_free(error);
        }
        else if(!reply)
            LoggerE("reply is null");

        return;
    }

    char *calls = NULL;
    g_variant_get(reply, "(s)", &calls);
    //LoggerD("calls: " << calls); // !!! Logger is working with a buffer of 128 chars only
    printf("%s\n", calls);

    g_variant_unref(reply);
}

void restart() {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "Restart",
                                 NULL,
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);
    if(error) {
        LoggerE("Failed to request Restart: " << error->message);
        g_error_free(error);
    }
}

static void handleSignal( GDBusConnection *connection,
                          const gchar *sender_name,
                          const gchar *object_path,
                          const gchar *interface_name,
                          const gchar *signal_name,
                          GVariant *parameters,
                          gpointer user_data)
{
    LoggerD("entered");
    LoggerD("\tsender_name: " << sender_name);
    LoggerD("\tobject_path: " << object_path);
    LoggerD("\tinterface_name: " << interface_name);
    LoggerD("\tsignal_name: " << signal_name);

    if(!strcmp(signal_name, "BluetoothPowered")) {
        GVariant *value = NULL;
        g_variant_get(parameters, "(v)", &value);
        bool powered = g_variant_get_boolean(value);
        LoggerD("Received POWERED status: " << powered);
    }
    else if(!strcmp(signal_name, "CallChanged")) {
        GVariantIter *iter;
        g_variant_get(parameters, "(a{sv})", &iter);
        const char *key;
        GVariant *value;
        while(g_variant_iter_next(iter, "{sv}", &key, &value)) {
            if(!strcmp(key, "state")) {
                const char *state = g_variant_get_string(value, NULL);
                LoggerD("\t- state: " << state);
            }
            else if(!strcmp(key, "line_id")) {
                const char *line_id = g_variant_get_string(value, NULL);
                LoggerD("\t- line_id: " << line_id);
            }
            else if(!strcmp(key, "contact")) {
                const char *contact = g_variant_get_string(value, NULL);
                LoggerD("\t- contact: " << contact);
            }
        }
    }
    else if(!strcmp(signal_name, "ContactsChanged")) {
        LoggerD("ContactsChanged");
    }
    else if(!strcmp(signal_name, "CallHistoryChanged")) {
        LoggerD("CallHistoryChanged");
    }
    else if(!strcmp(signal_name, "CallHistoryEntryAdded")) {
        const char *entry = NULL;
        g_variant_get(parameters, "(s)", &entry);
        printf("%s\n", entry);
    }
    else if(!strcmp(signal_name, "RemoteDeviceSelected")) {
        LoggerD("RemoteDeviceSelected");
        const char *result;
        g_variant_get(parameters, "(s)", &result);
        LoggerD("result = " << result);
    }
    else if(!strcmp(signal_name, "PairingResult")) {
        const char *device = NULL, *message = NULL;
        GVariant *tmp = NULL;
        g_variant_get(parameters, "(svs)", &device, &tmp, &message);
        bool success = g_variant_get_boolean(tmp);
        LoggerD("Pairing result: device=" << device << " success=" << success << " message=" << message);
    }
    else if(!strcmp(signal_name, "RequestPinCode")) {
        LoggerD("RequestPinCode");
        const char *device;
        g_variant_get(parameters, "(s)", &device);
        LoggerD("Enter PinCode for " << device << ": pin <PIN CODE>");
        if(_device) free(_device);
        _device = strdup(device);
    }
    else if(!strcmp(signal_name, "RequestPasskey")) {
        LoggerD("RequestPasskey");
        const char *device;
        g_variant_get(parameters, "(s)", &device);
        LoggerD("Enter Passkey for " << device << ": pass <PASS KEY>");
        if(_device) free(_device);
        _device = strdup(device);
    }
    else if(!strcmp(signal_name, "RequestConfirmation")) {
        LoggerD("RequestConfirmation");
        const char *device;
        unsigned long passkey;
        g_variant_get(parameters, "(su)", &device, &passkey);
        LoggerD("Confirm " << device << " with passkey " << passkey << " : conf <yes/no>");
        if(_device) free(_device);
        _device = strdup(device);
    }
}

void pairDevice(const char *bt_address) {
    LoggerD("entered");

    GError *error = NULL;
    g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL),
                                 PHONE_SERVICE,
                                 PHONE_OBJ_PATH,
                                 PHONE_IFACE,
                                 "PairDevice",
                                 g_variant_new("(s)", bt_address), // floating variants are consumed
                                 NULL,
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);

    if(error) {
        LoggerE("Failed to request 'PairDevice': " << error->message);
        g_error_free(error);
    }
}

