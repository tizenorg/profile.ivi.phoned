
#ifndef UTILS_H_
#define UTILS_H_

#include <glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <string>
#include <map>

namespace PhoneD {

/**
 * @addtogroup phoned
 * @{
 */

/**
 * @fn bool makeMACFromDevicePath(std::string &device)
 * Formatter function. Extracts MAC address from device path, eg. from \b /org/bluez/223/hci0/dev_AA_BB_CC_DD_EE_FF makes \b AA:BB:CC:DD:EE:FF.
 * @param[in,out] device The path of the device.
 * @return True if the formatted MAC is valid.
 */
bool makeMACFromDevicePath(std::string &device);

/**
 * @fn bool makeMACFromRawMAC(std::string &address)
 * Formatter function. Formats given MAC address in RAW format, eg. \b AABBCCDDEEFF into 'colon' format: \b AA:BB:CC:DD:EE:FF.
 * @param[in,out] address MAC address to be formatted.
 * @return True if the formatted MAC is valid.
 */
bool makeMACFromRawMAC(std::string &address);

/**
 * @fn bool makeRawMAC(std::string &address)
 * Formatter function. Formats given MAC address in 'colon' format, eg. \b AA:BB:CC:DD:EE:FF into RAW format: \b AABBCCDDEEFF.
 * @param[in,out] address MAC address to be formatted.
 * @return True if the formatted MAC is valid.
 */
bool makeRawMAC(std::string &address);

/**
 * @fn void formatPhoneNumber(std::string &phoneNumber)
 * Formatter function. Formats given phone number to one specific format, ie. it removes all non-HEX digit characters and replaces leadin '00' by '+' character.
 * \li 00421123456  -> +421123456
 * \li +421-123-456 -> +421123456
 * @param[in,out] phoneNumber Phone number to be formatted.
 */
void formatPhoneNumber(std::string &phoneNumber);

/**
 * @fn bool isValidMAC(std::string address)
 * Function to check wthether given MAC address is valid or not. It expects MAC address to be in 'full' format, eg. \b AA:BB:CC:DD:EE:FF. It doesn't support abbreviated MAC addresses, eg. \b AA:B:C:DD:EE:F. First it check whether ':' are at correct positions (2,5,8,11,14), then removes all non-HEX characters from the string and checks its length. If the length is 12, the given MAC address is valid.
 * @param[in] address MAC address to perform validation on.
 * @return \b True, if the MAC address is valid, otherwise returns \b false.
 */
bool isValidMAC(std::string address);

/**
 * int isndigit(int x)
 * The function to check if the input character is not hexa digit, ie. is not any of: 0 1 2 3 4 5 6 7 8 9 a b c d e f A B C D E F
 * @param [in] x Input character to check.
 * @return The function returns \b true if x is not hexa digit, otherwise it returns \b false.
 */
int isndigit(int x);

/**
 * @fn int isnxdigit(int x)
 * The function to check if the input character is not digit, ie. is not any of: 0 1 2 3 4 5 6 7 8 9
 * @param [in] x Input character to check.
 * @return The function returns \b true if x is not digit, otherwise it returns \b false.
 */
int isnxdigit(int x);

/**
 * A define for g_phone_error_quark().
 */
#define G_PHONE_ERROR  g_phone_error_quark()

/**
 * @fn GQuark g_phone_error_quark(void)
 * Creates a <a href="https://developer.gnome.org/glib/2.39/glib-Quarks.html">Quark</a> to be used to throw an error on D-Bus. The Quark is created from string \b "g-phone-error-quark".
 * @return A created Quark.
 */
GQuark g_phone_error_quark(void);

/*! \class PhoneD::Utils
 *  \brief Utility class providing helper functions for operating with Phone.
 */
class Utils {
    public:
        /**
         * A default constructor.
         */
        Utils () {}

        /**
         * A destructor.
         */
        virtual ~Utils () {}

        /**
         * A helper method to subscribe for DBUS siganl.
         * @param[in] type A type of the bus that the signal should be subscribed on. See <a href="https://developer.gnome.org/gio/2.35/GDBusConnection.html#GBusType">GBusType</a> documentation.
         * @param[in] service Service name to match on (unique or well-known name).
         * @param[in] iface D-Bus interface name to match on.
         * @param[in] path Object path to match on.
         * @param[in] name D-Bus signal name to match on.
         * @param[in] cb Callback to invoke when there is a signal matching the requested data. See <a href="https://developer.gnome.org/gio/2.35/GDBusConnection.html#GDBusSignalCallback">GDBusSignalCallback</a> documentation.
         * @param[in] data User data to pass to \b cb.
         * @return a bool indicating success of setting listener
         */
        static bool setSignalListener(GBusType type, const char *service, const char *iface,
                                      const char *path, const char *name, GDBusSignalCallback cb,
                                      void *data);
        /**
         * A helper method to unsubscribe from DBUS siganl.
         * @param[in] type A type of the bus that the signal is subscribed to. See <a href="https://developer.gnome.org/gio/2.35/GDBusConnection.html#GBusType">GBusType</a> documentation.
         * @param[in] service Service name to match on (unique or well-known name).
         * @param[in] iface D-Bus interface name to match on.
         * @param[in] path Object path to match on.
         * @param[in] name D-Bus signal name to match on.
         */
        static void removeSignalListener(GBusType type, const char *service, const char *iface,
                                         const char *path,   const char *name);

    private: // viriables
        static std::map<std::string, guint> mSubsIdsMap; /*! A map that holds ids of subscriptions to DBUS signals */
};

/*! \class PhoneD::CtxCbData
 *  \brief A class to store data for asynchronous operation.
 *
 *  A class to store data for asynchronous operation: context, callback function and two pointers to user data.
 */
class CtxCbData {
    public:
        /**
         * A default constructor, allowing to pass the data in the construction phase.
         * @param[in] ctx A pointer to the instance of an object that the asynchronous call was made on.
         * @param[in] cb A pointer to store callback method. It is usually used when there is a need for multiple level of aynchronous calls.
         * @param[in] data1 User data to pass to \b cb.
         * @param[in] data2 User data to pass to \b cb.
         */
        CtxCbData(void *ctx, void *cb, void *data1, void *data2) {
            this->ctx = ctx;
            this->cb = cb;
            this->data1 = data1;
            this->data2 = data2;
        }
        void *ctx;     /*!< to store current context */
        void *cb;      /*!< to store pointer to callback function */
        void *data1;   /*!< to store user data 1 */
        void *data2;   /*!< to store user data 2 */
};

} // PhoneD

#endif /* UTILS_H_ */

/** @} */

