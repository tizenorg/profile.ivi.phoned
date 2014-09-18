#ifndef OFONO_H_
#define OFONO_H_

#include <glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <string>
#include <string.h>
#include <map>

namespace PhoneD {

/**
 * @addtogroup phoned
 * @{
 */

/*! \class PhoneD::OFono
 *  \brief Class which is utilizing OFono D-Bus service. It is a base class and is not meant to be instantiated directly.
 *
 * A class providing access to <a href="http://www.ofono.org">OFono</a> functionality. It allows make a phone call, answer/decline incoming call, hang-up active phone call.
 */
class OFono {

    public:

    /*! \class Call
     *  \brief A Class describing phone call object.
     *
     * A class describing phone call object. It is used to get information about active call.
     */
    class Call {
        public:
            /**
             * A default constructor which constructs an empty object with NULL initialized members.
             */
            Call() : path(NULL), state(NULL), line_id(NULL) {};
            /**
             * Copy constructor to make a copy from the Call object specified by the pointer to it.
             * @param[in] call A pointer to Call object to make a copy of.
             */
            Call(OFono::Call *call) {
                if(call->path)  path = strdup(call->path);
                if(call->state) state = strdup(call->state);
                if(call->line_id) line_id = strdup(call->line_id);
            };
            /**
             * A destructor which frees the allocated memory of member variables and destroys the object.
             */
            ~Call() {
                if(path)    { g_free(path);    path=NULL;    }
                if(state)   { g_free(state);   state=NULL;   }
                if(line_id) { g_free(line_id); line_id=NULL; }
            };
        public:
            char *path;     /*!< A path to the call object */
            char *state;    /*!< A state of phone call (incoming,dialing,active,disconnected) */
            char *line_id;  /*!< A line identifier. It contains a phone number of the caller, or the calling person respectively. */
    };

    public:
        /**
         * A default constructor. Constructs the object and subscribes for certain D-Bus signals of \b org.ofono service:
         * <ul>
         * <li> "ModemAdded" on \b org.ofono.Manager interface - To get notified when modem is added. </li>
         * <li> "ModemRemoved" on \b org.ofono.Manager interface - To get notified when modem is removed.</li>
         * </ul>
         * It starts a timer to check whether default modem is \b Powered, or not. It handles the case, when the device gets out of the range, or when the user
         * turns OFF and then turns ON the BT on the phone device, in which case there isn't a notification that the remote device got back.
         * The timer in specific interval checks whether the modem is \b Powered and if not, it tries to power it up.
         */
        OFono();

        /**
         * A destructor. Unselects default modem and destructs the object. @see selectModem() and unselectModem()
         */
        ~OFono();

        /**
         * A method to select modem. It gets the list of available modems and checks them against the given MAC address. If the modem is for the device specified by the given MAC address, it is selected as default one.
         * @param[in] btAddress A MAC address of a remote BT device for which the modem should be selected.
         * @see unselectModem()
         */
        void selectModem(std::string &btAddress);

        /**
         * A method to unselect the default modem.
         * @see selectModem()
         */
        void unselectModem();

        /**
         * A method to invoke phone call.
         * @param[in] phoneNumber A phone number to invoke phone call.
         * @param[out] error If the return value from the method is \b false, it contains a description of the error. The caller is responsible for freeing the memory if the error is set.
         * @return \b True if D-Bus "Dial" method was successfuly called on \b org.ofono.VoiceCallManager interface, otherwise it returns \b false.
         */
        bool invokeCall(const char* phoneNumber, char **error);

        /**
         * A method to answer incoming phone call.
         * @param[out] error If the return value from the method is \b false, it contains a description of the error. The caller is responsible for freeing the memory if the error is set.
         * @return \b True if D-Bus "Answer" method was successfuly called on \b org.ofono.VoiceCall interface, otherwise it returns \b false.
         */
        bool answerCall(char **error);

        /**
         * A method to decline incoming, or hangup active phone call.
         * @param[out] error If the return value from the method is \b false, it contains a description of the error. The caller is responsible for freeing the memory if the error is set.
         * @return \b True if D-Bus "Hangup" method was successfuly called on \b org.ofono.VoiceCall interface, otherwise it returns \b false.
         */
        bool hangupCall(char **error);

        /**
         * A method to mute/unmute active phone call.
         * @param[in] mute Specifies whether to mute/unmute phone call.
         * @param[out] error If the return value from the method is \b false, it contains a description of the error. The caller is responsible for freeing the memory if the error is set.
         * @return \b True if D-Bus "SetProperty" method to set "Muted" property was successfuly called on \b org.ofono.CallVolume interface, otherwise it returns \b false.
         */
        bool muteCall(bool mute, char **error);

        /**
         * Gets the information about active phone call. It creates new object (allocates a memory) and the caller has to delete it.
         * @return OFono::Call object describing active phone call.
         */
        OFono::Call *activeCall();

    private:
        static void asyncSelectModemCallback(GObject *source, GAsyncResult *result, gpointer user_data); // async callback for "GetModems" invoked from selectModem() method
        virtual void callChanged(const char* state, const char* phoneNumber) = 0;

        void addModem(const char *path, GVariantIter *props);
        virtual void modemAdded(std::string &modem) = 0; // MAC address of modem ... from ModemAdded DBUS
        void removeModem(const char *modemPath);
        virtual void modemPowered(bool powered) = 0;
        virtual void setModemPoweredFailed(const char *err) = 0;
        static void handleSignal(GDBusConnection *connection,  const gchar     *sender,
                                 const gchar     *object_path, const gchar     *interface_name,
                                 const gchar     *signal_name, GVariant        *parameters,
                                 gpointer         user_data);

        //DBUS: array{object,dict}
        void getModems(); //Get an array of call object paths and properties of available modems
        void getCalls(); //Get an array of call object paths and properties that represents the currently present calls.
        void addCall(const char *path, GVariantIter *props);
        void removeCall(OFono::Call **call);

        // used to set property on ifaces that do have same object path, eg. Modem, CallVolume, ...
        bool setProperty(const char* iface, const char* path, const char *property, int type, void *value); // returns success of the operation
        void setPropertyAsync(const char* iface, const char* path, const char *property, int type, void *value, GAsyncReadyCallback callback);

        void setModemPowered(const char *path, bool powered); // path of Modem object
        bool isModemPowered(const char *modem);
        static void asyncSetModemPoweredCallback(GObject *source, GAsyncResult *result, gpointer user_data); // async callback for "SetProperty" invoked from setModemPowered() method
        static gboolean checkForModemPowered(gpointer user_data); // continuous timeout method to check for 'Selected' modem being Powered and power it on, if it's not

        static void GDbusAsyncReadyCallback(GObject *source, GAsyncResult *result, gpointer user_data);

    private:
        DBusConnection               *mDBusConnection;
        gchar                        *mModemPath;
        OFono::Call                  *mActiveCall;
};

#endif /* OFONO_H_ */

} // PhoneD

/** @} */

