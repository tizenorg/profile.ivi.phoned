#ifndef CONNMAN_H_
#define CONNMAN_H_

#include <dbus/dbus.h>
#include <gio/gio.h>

namespace PhoneD {

/**
 * @addtogroup phoned
 * @{
 */

/*! \class PhoneD::ConnMan
 *  \brief Class which is utilizing ConnMan D-Bus service. It is a base class and is not meant to be instantiated directly.
 *
 * A class providing basic <a href="https://connman.net">ConnMan</a> functionality on Bluetooth technology, like setting 'Powered' state to ON/OFF.
 */
class ConnMan {
    public:
        /**
         * A constructor. Constructs the object.
         */
        ConnMan();

        /**
         * A destructor. Destructs the object.
         */
        ~ConnMan();

        /**
         * Sets the Bluetooth technology "Powered" state to ON/OFF, ie. sets the Soft-block on Bluetooth, just like you would achieve via `rfkill block bluetooth`. You can check the actual  state of "Powered" property via RFkill utility, typing the command `rfkill list`. The method takes one argument specifying the state that the Bluetooth will be set to.
         * @param[in] value Specifies whether the BT should be switched ON, or OFF
         * @return A \b bool Indicates the result of the operation.
         */
        bool setBluetoothPowered(bool value);

    private:
        /**
         * Method to get Bluetooth technology object. If it returns \b NULL, BT technology is not found.
         * @see setBluetoothPowered()
         * @return  A C-string describing Bluetooth technology object, or \b NULL, if the technology is not found. The caller is responsible for feeing the memory.
         */
        char *getBluetoothTechnology();

        static void handleSignal(GDBusConnection  *connection,
                                 const gchar      *sender,
                                 const gchar      *object_path,
                                 const gchar      *interface_name,
                                 const gchar      *signal_name,
                                 GVariant         *parameters,
                                 gpointer          user_data);
    protected:
        /**
         * Method to set initial value of mBluetoothPowered variable.
         * @see mBluetoothPowered
         */
        void init();

    protected:
        /**
         * A variable to store \b Powered state of BT.
         */
        bool mBluetoothPowered;
};

} // PhoneD

#endif /* CONNMAN_H_ */

/** @} */

