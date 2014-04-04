#ifndef BLUEZ_H_
#define BLUEZ_H_

#include <glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>

namespace PhoneD {

/**
 * @addtogroup phoned
 * @{
 */

/*! \class PhoneD::Bluez
 *  \brief Class which is utilizing Bluez D-Bus service. It is a base class and is not meant to be instantiated directly.
 *
 * A class providing access to <a href="http://www.bluez.org">Bluez</a> functionality. Subscribes to certain DBUS signals of org.bluez service, see Bluez::Bluez() for more details. It registers an agent for BT operation authentification.
 */
class Bluez {

    public:
        /**
         * A default constructor. Constructs the object and subscribes for certain D-Bus signals of \b org.bluez service:
         * \li "AdapterAdded" on \b org.bluez.Manager interface - To get notified when hci adapter is added.
         * \li "AdapterRemoved" on \b org.bluez.Manager interface - To get notified when hci adapter is removed.
         * \li "DeviceCreated" on \b org.bluez.Adapter interface - To get notified when a device (remote device) is created, ie. when pairing is initiated.
         * \li "DeviceRemoved" on \b org.bluez.Adapter interface - To get notified when a device (remote device) is removed, ie. when the device is unpaired.
         * \li "PropertyChanged" on \b org.bluez.Adapter interface - To get notified when there is a change in some of adapter's properties, eg. when the adapter is "Powered", the name of adapter has changed, etc.
         */
        Bluez();

        /**
         * A destructor.
         * Destructs the object.
         */
        ~Bluez();

    protected:

        /**
         * Method to setup an agent for BT authentication operations and register the agent object on the D-Bus.
         * @see registerAgent()
         */
        void setupAgent();

        /**
         * Registers created agent via setupAgent() method to the adapter (the default one). It is done by calling \b RegisterAgent method on \b org.bluez.Adapter interface.
         * @see setupAgent()
         */
        void registerAgent();

        /**
         * Gets the state whether the remote device is paired, or not.
         * @param[in] bt_address A MAC address of the remote device to get paired state of.
         * @return \b True if the device is paired, or \b false if it is not.
         */
        bool isDevicePaired(const char *bt_address);

        /**
         * Sets \b Powered state of the default adapter.
         * @param[in] value Specifies whether to power ON, or OFF the adapter.
         * @return \b True, if the setting was successful, otherwise returns \b false.
         */
        bool setAdapterPowered(bool value); // Power ON/OFF hci0 adapter

    private:
        gchar* getDefaultAdapter();
        gchar* getDeviceFromAddress(std::string &address);
        static void handleSignal(GDBusConnection *connection,
                                 const gchar     *sender,
                                 const gchar     *object_path,
                                 const gchar     *interface_name,
                                 const gchar     *signal_name,
                                 GVariant        *parameters,
                                 gpointer         user_data);

        static void agentHandleMethodCall( GDBusConnection       *connection,
                                           const gchar           *sender,
                                           const gchar           *object_path,
                                           const gchar           *interface_name,
                                           const gchar           *method_name,
                                           GVariant              *parameters,
                                           GDBusMethodInvocation *invocation,
                                           gpointer               user_data);

        virtual void adapterPowered(bool value) = 0; // to handle "Powered" property changed on ADAPTER, due to eg. RF-kill
        virtual void defaultAdapterAdded() = 0;
        virtual void defaultAdapterRemoved() = 0;
        virtual void deviceCreated(const char *device) = 0;
        virtual void deviceRemoved(const char *device) = 0;

    private:
        gchar* mAdapterPath;

        // Agent
        int mAgentRegistrationId;
        GDBusInterfaceVTable mAgentIfaceVTable;
        GDBusNodeInfo   *mAgentIntrospectionData;
};

} // PhoneD

#endif /* BLUEZ_H_ */

/** @} */

