
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "utils.h"

#include "Logger.h"

namespace PhoneD {

GQuark g_phone_error_quark(void)
{
    return g_quark_from_static_string("g-phone-error-quark");
}

std::map<std::string, guint> Utils::mSubsIdsMap;


bool Utils::setSignalListener(GBusType type, const char *service,
                              const char *iface, const char *path,
                              const char *name, GDBusSignalCallback cb,
                              void *data)
{
    // key is of form: BUS_TYPE:SERVICE:IFACE:OBJ_PATH:SIGNAL
    const char *bus_type = (type == G_BUS_TYPE_SYSTEM) ? "SYSTEM" : (type == G_BUS_TYPE_SESSION) ? "SESSION" : "";
    std::string key = std::string(bus_type) + ":" + service + ":" + iface + ":" + path + ":" + name;
    LoggerD("subscribing for DBUS signal: " << key);

    // only one listener (subscription on DBUS signal) allowed for specific signal
    if(mSubsIdsMap[key] <= 0) { // not yet subscribed for DBUS signal
        guint id = 0;
        id = g_dbus_connection_signal_subscribe(g_bus_get_sync(type, NULL, NULL),
                                                service,
                                                iface,
                                                name,
                                                path,
                                                NULL,
                                                G_DBUS_SIGNAL_FLAGS_NONE,
                                                cb,
                                                data,
                                                NULL);

        if(id == 0) {
            LoggerE("Failed to subscribe to: " << key);
            return false;
        }

        mSubsIdsMap[key] = id;

        return true; // success
    }

    // already subscribed for DBUS signal
    return false;
}

void Utils::removeSignalListener(GBusType type, const char *service,
                                 const char* iface, const char* path,
                                 const char* name)
{

    // key is of form: BUS_TYPE:SERVICE:IFACE:OBJ_PATH:SIGNAL
    const char *bus_type = (type == G_BUS_TYPE_SYSTEM) ? "SYSTEM" : (type == G_BUS_TYPE_SESSION) ? "SESSION" : "";
    std::string key = std::string(bus_type) + ":" + service + ":" + iface + ":" + path + ":" + name;
    LoggerD("unsubscribing from DBUS signal: " << key);

    std::map<std::string, guint>::iterator iter = mSubsIdsMap.begin();
    while(iter != mSubsIdsMap.end()) {
        if(!strcmp(key.c_str(), (*iter).first.c_str())) {
            g_dbus_connection_signal_unsubscribe(g_bus_get_sync(type, NULL, NULL), (*iter).second);
            mSubsIdsMap.erase(iter);
            break;
        }
        iter++;
    }
}

// makes AABBCCDDEEFF from AA:BB:CC:DD:EE:FF
bool makeRawMAC(std::string &address) {
    address.erase(std::remove_if(address.begin(), address.end(), isnxdigit), address.end());
    return (address.length() == 12);
}

// makes AA:BB:CC:DD:EE:FF from AABBCCDDEEFF
bool makeMACFromRawMAC(std::string &address) {
    if(address.length() == 12) {
        address.insert(10,":");
        address.insert(8,":");
        address.insert(6,":");
        address.insert(4,":");
        address.insert(2,":");
        return true;
    }
    return false;
}

// makes AA:BB:CC:DD:EE:FF from device path, eg: /org/bluez/223/hci0/dev_AA_BB_CC_DD_EE_FF
bool makeMACFromDevicePath(std::string &device) {
    size_t idx = device.find( "dev_" ) + 4;
    device = device.substr (idx, device.length()-idx);
    device.erase(std::remove_if(device.begin(), device.end(), isnxdigit), device.end());
    return makeMACFromRawMAC(device);
}

// don't use a reference for address - we need a copy
bool isValidMAC(std::string address) {
    // expect MAC address in form: AA:BB:CC:DD:EE:FF
    if( (address[2] != ':') || (address[5] != ':') || (address[8] != ':') ||
        (address[11] != ':') || (address[14] != ':'))
        return false;

    // remove all non HEX digits
    address.erase(std::remove_if(address.begin(), address.end(), isnxdigit), address.end());

    // after removing, there should be exactly 12 remaining characters/digits
    return (address.length() == 12);
}

int isndigit(int x) {
    return !std::isdigit(x);
}

// will remove all non-digit characters, except leading '+'
void formatPhoneNumber(std::string &phoneNumber) {
    bool leadingPlusSign = (phoneNumber[0] == '+');
    phoneNumber.erase(std::remove_if(phoneNumber.begin(), phoneNumber.end(), isndigit), phoneNumber.end());
    if(leadingPlusSign)
        phoneNumber.insert(0, "+");
}

int isnxdigit(int x) {
    return !std::isxdigit(x);
}

} // PhoneD

