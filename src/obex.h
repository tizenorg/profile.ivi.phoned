#ifndef OBEX_H_
#define OBEX_H_

#include <libebook-contacts/libebook-contacts.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <string>
#include <map>
#include <vector>
#include <deque>

namespace PhoneD {

/**
 * @addtogroup phoned
 * @{
 */

class SyncPBData;

/*! \class PhoneD::Obex
 *  \brief Class which is utilizing Obex D-Bus service. It is a base class and is not meant to be instantiated directly.
 *
 * A class providing access to <a href="http://www.bluez.org">Obex</a> functionality.
 */
class Obex {
    public:
        /*! Error codes for Obex operations. */
        enum Error {
            OBEX_ERR_NONE = 0,            /*!< No error. */
            OBEX_ERR_INVALID_SESSION,     /*!< Invalid session, or Obex session with remote device not created. */
            OBEX_ERR_DBUS_ERROR,          /*!< D-Bus specific error. */
            OBEX_ERR_DBUS_INVALID_REPLY,  /*!< D-Bus invalid reply. */
            OBEX_ERR_INVALID_ARGUMENTS    /*!< Invalid arguments specified. */
        };

    public:
        /**
         * A default constructor. Constructs and initializes an object.
         */
        Obex();

        /**
         * A destructor. Removes session created with Obex::createSession() and destructs the object.
         * @see createSession()
         */
        ~Obex();

        /**
         * Creates an Obex session to the BT device (paired) specified by given MAC address. It calls \b CreateSession on \b org.bluez.obex.Client interface with the \b Target specified to be \b "PBAP".
         * @param[in] bt_address A MAC address of paired device that the Obex session should be created to.
         * @see removeSession()
         */
        void createSession(const char *bt_address);

        /**
         * Removes the session (the default/only-one) created by createSession() method.
         * @param[in] notify Specifies whether to notify the class which is deriving from this class about the session being removed.
         * @see createSession()
         */
        void removeSession(bool notify = true);

        /**
         * Synchronizes phones PhoneBook contacts. Pulls the contacts from remote device that the Obex session is created to. The pull is done on \b "INT" internal phone's contacts list.
         * @param[in] count Specifies the number of latest contacts to be pulled from the phone. \b 0 means to pull all contacts.
         * @see createSession()
         * @return The status of the operation, ie. whether pull-ing the contacts has successfuly started.
         */
        Obex::Error syncContacts(unsigned long count = 0);

        /**
         * Synchronizes phone's call history . Pulls the call history entries from remote device that the Obex session is created to. The pull is done on \b "INT" internal phone's call history. It retrieves \b "cch", ie. any kind of call (DIALED,MISSED,....).
         * @param[in] count Specifies the number of latest calls from the call history to be pulled from the phone. \b 0 means to pull all call entries.
         * @see createSession()
         * @return The status of the operation, ie. whether pull-ing the call entries has successfuly started.
         */
        Obex::Error syncCallHistory(unsigned long count = 0);

        /**
         * Method to get synchronized contacts in JSON format as an array of \b tizen.Contacts. Returns empty array \b "[]", if the contacts are not yet synchronized , or if there are no contacts.
         * @param[out] contacts A container for the contacts. The contacts are in \b tizen.Contact format.
         * @param[in] count Specifies the number of latest contacts to be returned. \0 means to return all contacts.
         */
        void getJsonContacts(std::string& contacts, unsigned long count);

        /**
         * Method to get synchronized call history entries in JSON format as an array of \b tizen.CallHisoryEntry-ies. Returns empty array \b "[]", if the call history is not yet synchronized, or if there are no calls in history.
         * @param[out] calls A container for the call history entries. The call history entries are in \b tizen.CallHistoryEntry format.
         * @param[in] count Specifies the number of latest call history entries to be returned. \b 0 means to return all calls from the history.
         */
        void getJsonCallHistory(std::string& calls, unsigned long count);

        /**
         * Returns contact in \b tizen.Contact JSON format, which matches given phone number. It returns an empty JSON object "{}" if the contact is not found.
         * @param[in] phoneNumber A phone number for which the contact should be returned.
         * @param[out] contact A container for the contact that match the phone number. The contact is in \b tizen.Contact JSON format.
         */
        void getContactByPhoneNumber(const char *phoneNumber, std::string &contact);

    protected:
        /**
         * Sets selected remote device, which is used when processing received VCards to make sure that they belong to the device selected remote device.
         * @param[in] &btAddress A MAC address of remote BT device for processing VCards.
         */
        void setSelectedRemoteDevice(std::string &btAddress);

    private: // methods

        virtual void contactsChanged() = 0;
        virtual void callHistoryChanged() = 0;
        virtual void pbSynchronizationDone() = 0;
        virtual void createSessionFailed(const char *err) = 0;
        virtual void createSessionDone(const char *s) = 0;
        virtual void removeSessionDone() = 0;
        // to get notifications about CallHistoryEntry added to the CallHistory list
        // ie, when a call has been made ("disconnected" received), new entry is added to the list
        virtual void callHistoryEntryAdded(std::string &entry) = 0;
        // the method, which will be called when active Transfer is stalled
        virtual void transferStalled() = 0;
        // method to clear the sync queue, eg. when the transfer is stalled
        void clearSyncQueue();

        // Select phonebook object for other operations
        bool select(const char *location, const char *phonebook);

        // type: type of pull request - "pb" for Contacts, "cch" for CallHistory
        Obex::Error pullAll(const char *type, unsigned long count); // retrieves 'count' selected (select()) entries from
                                                                    // the phonebook (0=ALL)

        static void handleSignal(GDBusConnection *connection,  const gchar     *sender,
                                 const gchar     *object_path, const gchar     *interface_name,
                                 const gchar     *signal_name, GVariant        *parameters,
                                 gpointer         user_data);

        // method to add "E_CONTACT_UID" to the EContact
        // will remove existing one, if it exists
        // returns: bool indicating successfull UID creation
        bool makeUid(EContact *entry);
        // process received VCARD contacts' data
        // check the data against origin MAC, ie. the user may have selected other
        // remote device while synchronization was ongoing and thus the data (VCards)
        // may not belong to the device selected at the time
        void processVCards(const char *filePath, const char *type, const char *origin);
        static void asyncCreateSessionReadyCallback(GObject *source, GAsyncResult *result, gpointer user_data);

        void initiateNextSyncRequest();

        void parseEContactToJsonTizenContact(EContact *econtact, std::string &contact);
        void parseEContactToJsonTizenCallHistoryEntry(EContact *econtact, std::string &call);

        static gboolean checkStalledTransfer(gpointer user_data);

    private: // variables
        std::string mSelectedRemoteDevice;
        char *mSession;
        char *mActiveTransfer;
        // only one synchronization operation getContacts/getCallHistory,
        // is allowed at a time via Obex due to the selection of phonebook
        // use std::deque to handle this limitation
        std::deque<SyncPBData*> mSyncQueue;
        std::map<std::string, EContact*> mContacts;
        std::vector<std::string> mContactsOrder; // order of contacts inserted into the MAP
        std::map<std::string, EContact*> mCallHistory;
        std::vector<std::string> mCallHistoryOrder; // order of calls inserted into the MAP
};

#endif /* BLUEZ_H_ */

} // PhoneD

/** @} */

