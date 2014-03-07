#ifndef PHONE_H_
#define PHONE_H_

#include <gio/gio.h>

#include "connman.h"
#include "bluez.h"
#include "ofono.h"
#include "obex.h"
#include "utils.h"

namespace PhoneD {

/**
 * \brief A namespace for all classes used to construct \b phoned daemon.
 * @namespace PhoneD
 */

/**
 * @defgroup phoned phoned daemon
 * \brief Phoned daemon to provide a single point to access phone functionality.
 *
 * A module to provide a signle point to access phone functionality. It provides a phoned daemon, which registers itself on D-Bus as \b org.tizen.phone service and exports phone specific methods on D-Bus, as well as emits a phone specific signals on the bus. See Phone for more details.
 *
 * @addtogroup phoned
 * @{
 */

/*! \class PhoneD::Phone
 *  \brief Class which is utilizing ConnMan, Bluez, OFono and Obex D-Bus services to be used for operations with a phone.
 *
 * Class which is utilizing ConnMan, Bluez, OFono and Obex D-Bus services and in controlled way exports them via D-Bus to be used by WRT plugin @link wrt-plugins-ivi-phone wrt-plugins-ivi-phone @endlink to provide access to phone functionality from Web application. \n
 * In the construction phase, it requests to get own name on the D-Bus. Once the bus acquired callback is called, it starts setup procedure. It registers a service \b org.tizen.phone on D-Bus, registers Bluez agent for BT authentication and finaly starts OFono (selects and activates a modem) and Obex (creates a session) instances. ConnMan instance is used for initial powering up BT technology. Bluez is used to subscribe for BT specific notifications, such as notifications about \b Powered state of BT, notifications about paired and selected device being removed/unpaired, and many more.
 *
 * When it registers a service on D-Bus, it exports the following methods on \b org.tizen.Phone interface:
 * <ul>
 * <li> \b SelectRemoteDevice ( \a \b address ) Selects a remote device/phone, to which the phone operations should be performed at. Will emit \b RemoteDeviceSelected signal, once the device gets selected. </li>
 *     <ul>
 *     <li> \a \b address [in] \b 's' MAC address of a remote device to be selected. </li>
 *     </ul>
 *
 * <li> \b GetSelectedRemoteDevice ( \a \b address ) Gets a currently selected remote device used for phone operations. It returns \b "" if no device is currently selected. </li>
 *     <ul>
 *     <li> \a \b address [out] \b 's' MAC address of selected remote device. </li>
 *     </ul>
 *
 * <li> \b UnselectRemoteDevice () Unselects selected remote device. Will emit \b RemoteDeviceSelected signal with an address argument \b "". </li>
 *
 * <li> \b Dial ( \a \b number ) Dials a given phone number. </li>
 *     <ul>
 *     <li> \a \b number [in] \b 's' A phone number to dial. </li>
 *     </ul>
 *
 * <li> \b Answer () Answers an incoming phone call. </li>
 *
 * <li> \b Mute ( \a \b muted ) Mutes/unmutes the active phone call. </li>
 *     <ul>
 *     <li> \a \b muted [in] \b 'b' Specifies whether to mute, or unmute the call. </li>
 *     </ul>
 *
 * <li> \b Hangup () A method to decline incoming call, or hangup the active one. </li>
 *
 * <li> \b ActiveCall ( \a \b call ) Gets the active phone call in JSON format, or \b {} when there is no active call. </li>
 *     <ul>
 *     <li> \a \b call [out] \b 'a{sv}' An active call in JSON format. </li>
 *     </ul>
 *
 * <li> \b Synchronize () Synchronizes PB data from the phone (Contacts, CallHistory).
 *
 * <li> \b GetContacts ( \a \b count, \a \b contacts ) Gets \a \b count first contacts in \b tizen.Contact JSON format, or \b [] when the data are not synchronized, or there are no contacts on the remote device. </li>
 *     <ul>
 *     <li> \a \b count [in] \b 'u' A number to specify how many first contacts should be returned. \a \b 0 means to return all contacts. </li>
 *     <li> \a \b contacts [out] \b 's' Returned \a \b count countacts in \b tizen.Contact JSON format. </li>
 *     </ul>
 *
 * <li> \b GetCallHistory ( \a \b count, \a \b calls ) Gets \a \b count latest call entries from the history in \b tizen.CallHistoryEntry JSON format, or \b [] when the data are not synchronized, or there are no calls in the history on the remote device. </li>
 *     <ul>
 *     <li> \a \b count [in] \b 'u' A number to specify how many latest call entries should be returned. </li>
 *     <li> \a \b calls [out] \b 's' Returned latest \a \b count call entries in \b tizen.CallHistoryEntry JSON format. </li>
 *     </ul>
 *
 * </ul>

 * And emits the following signals:
 * <ul>
 * <li> \b RemoteDeviceSelected ( \a \b device ) A signal which is emitted when the selected remote device has changed, eg. the selected remote device has been selected, or when the remote device has been unselected, eg. due to connection lost with the remote device, or as a result of calling \b UnselectRemoteDevice method.
 *     <ul>
 *     <li> \a \b device [in] \b 's' A device in JSON format describing the change. </li>
 *     </ul>
 *
 * <li> \b ContactsChanged () A signal which is emitted when there is a change in synchronized contacts.
 *
 * <li> \b CallHistoryChanged () A signal which is emitted when there is a change in synchronized call history, eg. due to made/received phone call.
 *
 * <li> \b CallHistoryEntryAdded ( \a \b call ) A signal which is emitted when a call has been added to the call history, eg. due to made/received phone call.
 *     <ul>
 *     <li> \a \b call \b 's' A call in \b tizen.CallHistoryEntry fromat that has been added to the call history.
 *     </ul>
 *
 * <li> \b CallChanged ( \a \b call ) A signal which is emitted when there is a change in the active call. It indicates incoming, as well as the individual states that the call may go through, like "alerting", "active", "disconnected".
 *     <ul>
 *     <li> \a \b call \b '(a{sv})' A call object which specifies \a \b state of the call, \a \b line \a \b identifier (the phone number) and the \a \b contact if there is a match with the \a \b line_id.
 *     </ul>
 * </ul>
 */
class Phone : public ConnMan, public Bluez, public OFono, public Obex {
    public:
        /**
         * A default constructor which initializes individual services and sets BT adapter "Powered" ON. It requests a name \b org.tizen.phone to register itself on D-Bus. Once the bus acquired callback is called, it registers Bluez agent for BT authentification and starts OFono and Obex services.
         */
        Phone();

        /**
         * A destructor.
         * Unregisters the service from D-Bus and destructs the object.
         */
        virtual ~Phone();

    private:
        void setup(); // called once the bus is acquired
        void startServices(); // starts OFono/Obex services
        void stopServices(); // stops OFono/Obex services
        static void busAcquiredCb(GDBusConnection *gdbus, const gchar *name, gpointer data);
        static void nameAcquiredCb(GDBusConnection *gdbus, const gchar *name, gpointer data);
        static void nameLostCb(GDBusConnection *gdbus, const gchar *name, gpointer data);
        static void handleMethodCall( GDBusConnection       *connection,
                                      const gchar           *sender,
                                      const gchar           *object_path,
                                      const gchar           *interface_name,
                                      const gchar           *method_name,
                                      GVariant              *parameters,
                                      GDBusMethodInvocation *invocation,
                                      gpointer               user_data);

        // Bluez stuff
        virtual void adapterPowered(bool value); // to handle "Powered" property changed on ADAPTER, due to eg. RF-kill
        virtual void defaultAdapterAdded();
        virtual void defaultAdapterRemoved();
        virtual void deviceCreated(const char *device);
        virtual void deviceRemoved(const char *device);
        // OFono stuff
        virtual void callChanged(const char* state, const char* phoneNumber);
        virtual void modemAdded(std::string &modem); // MAC address of modem ... from ModemAdded DBUS
        virtual void modemPowered(bool powered);
        virtual void setModemPoweredFailed(const char *err);
        // Obex stuff
        virtual void contactsChanged();
        virtual void callHistoryChanged();
        virtual void pbSynchronizationDone();
        virtual void createSessionFailed(const char *err);
        virtual void createSessionDone(const char *s);
        virtual void removeSessionDone();
        // NOTE: entry is valid only for the time of call this method
        virtual void callHistoryEntryAdded(std::string &entry);
        // active transfer is stalled - ??? session is down ???
        virtual void transferStalled();
        static gboolean delayedSyncCallHistory(gpointer user_data);

        // function to store MAC address of selected remote device in persistent storage/file
        // returns bool indicating result of the operation: false = failed, true = OK
        bool storeSelectedRemoteDeviceMAC(const std::string &btAddress);
        // function to read MAC address of selected remote device from persistent storage/file
        // remote address is returned via 'btAddress' argument to the function
        // returns bool indicating result of the operation: false = failed, true = OK
        bool readSelectedRemoteDeviceMAC(std::string &btAddress);

        // sets selected remote device MAC address
        void setSelectedRemoteDevice(std::string &btAddress);

    private:
        bool mAdapterPowered; // variable to hold the state of Default adpater - not to try to 'CreateSession' 'cause it will result in Powering up the device, which is unwanted behavior
        std::string mWantedRemoteDevice; // a remote device that is requested to be 'Selected'
        std::string mSelectedRemoteDevice; // selected remote device - if initialization of OFono/Obex is successfull
        bool mPBSynchronized; // indicating whether PB data are synchronized, or not
        std::vector<CtxCbData*> mSynchronizationDoneListeners; // listeners to be notified once the synchronization finishes
        guint mNameRequestId;
        guint mRegistrationId;
        GDBusNodeInfo *mIntrospectionData;
        GDBusInterfaceVTable mIfaceVTable;
};

} // PhoneD

#endif /* PHONE_H_ */

/** @} */

