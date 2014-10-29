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
#define BLUEZ_ADAPTER_IFACE     BLUEZ_PREFIX ".Adapter1"
#define BLUEZ_DEVICE_IFACE      BLUEZ_PREFIX ".Device1"
#define BLUEZ_AGENT_IFACE       BLUEZ_PREFIX ".Agent1"

#define AGENT_PATH              "/org/bluez/poc_agent"
#define AGENT_CAPABILITIES      "KeyboardDisplay"

#define AGENT_PASSKEY            123456
#define AGENT_PINCODE           "123456"

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

    // subscribe for InterfacesAdded/InterfacesRemoved to get notification about the change
    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, "org.freedesktop.DBus.ObjectManager",
                             "/", "InterfacesAdded", Bluez::handleSignal,
                             this);
    Utils::setSignalListener(G_BUS_TYPE_SYSTEM, BLUEZ_SERVICE, "org.freedesktop.DBus.ObjectManager",
                             "/", "InterfacesRemoved", Bluez::handleSignal,
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
	char *result;

	GError * error = nullptr;
	GDBusProxy * managerProxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
	                            "org.bluez",
	                            "/",
	                            "org.freedesktop.DBus.ObjectManager",
	                            nullptr,&error);
	if(error)
	{
		LoggerE("could not create ObjManager proxy");
		// DebugOut(DebugOut::Error)<<"Could not create ObjectManager proxy for Bluez: "<<error->message<<endl;
		g_error_free(error);
		return "";
	}

	GVariant * objectMap = g_dbus_proxy_call_sync(managerProxy, "GetManagedObjects",nullptr, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	if(error)
	{
		LoggerE("failed to get GetManagedObj");
		//DebugOut(DebugOut::Error)<<"Failed call to GetManagedObjects: "<<error->message<<endl;
		g_object_unref(managerProxy);
		g_error_free(error);
		return "";
	}

	GVariantIter* iter;
	char* objPath;
	GVariantIter* level2Dict;

	g_variant_get(objectMap, "(a{oa{sa{sv}}})",&iter);
	while(g_variant_iter_next(iter, "{oa{sa{sv}}}",&objPath, &level2Dict))
	{
		char * interfaceName;
		GVariantIter* innerDict;
		while(g_variant_iter_next(level2Dict, "{sa{sv}}", &interfaceName, &innerDict))
		{
			if(!strcmp(interfaceName, "org.bluez.Adapter1"))
			{
				result = objPath?strdup(objPath):NULL;
			}
			g_free(interfaceName);
			g_variant_iter_free(innerDict);
		}
		g_free(objPath);
		g_variant_iter_free(level2Dict);
	}
	g_variant_iter_free(iter);

	return result;
}

bool Bluez::setAdapterPowered(bool value) {
    if(mAdapterPath) {
        GError *err = NULL;

		g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL,NULL),
		                             BLUEZ_SERVICE,
		                             mAdapterPath,
		                             "org.freedesktop.DBus.Properties",
		                             "Set",
		                             g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(value)),
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

    if(!strcmp(interface_name, "org.freedesktop.DBus.ObjectManager")) {
        if(!strcmp(signal_name, "InterfacesAdded"))
        {
			char *objPath = NULL;
			GVariantIter* iter;

			g_variant_get(parameters, "(oa{sa{sv}})", &objPath, &iter);

			if(objPath)
			{
				GVariantIter* iter2;
				char *interface = NULL;

				while(g_variant_iter_next(iter, "{sa{sv}}",&interface, &iter2))
				{

					if(!strcmp(interface, "org.bluez.Adapter1"))
					{
						LoggerD("Adapter added: " << objPath);
						if(!ctx->mAdapterPath) {
						// make added adapter as default
						ctx->mAdapterPath = strdup(objPath);
						//ctx->setupAgent();
						//ctx->registerAgent();
						ctx->defaultAdapterAdded();
						}
					}
				}
        }
	}
        else if(!strcmp(signal_name, "InterfacesRemoved")) {
            char *objPath = NULL;
			GVariantIter* iter;

			g_variant_get(parameters, "(oas)", &objPath, &iter);

			if(objPath)
			{
				char *interface = NULL;

				while(g_variant_iter_next(iter, "s", &interface));
				{

					if(!strcmp(interface, "org.bluez.Adapter1"))
					{
						LoggerD("Adapter removed: " << objPath);
						if(ctx->mAdapterPath && !strcmp(ctx->mAdapterPath, objPath)) {
							// removed the default adapter
							free(ctx->mAdapterPath);
							ctx->mAdapterPath = NULL;
							ctx->defaultAdapterRemoved();
						}
					}
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

    if(!strcmp(method_name, "AuthorizeService")) {
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

gchar* Bluez::getDeviceFromAddress(std::string &address)
{
	char *result = NULL;
	bool done = false;

	GError * error = nullptr;
	GDBusProxy * managerProxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, NULL,
	                            "org.bluez",
	                            "/",
	                            "org.freedesktop.DBus.ObjectManager",
	                            nullptr,&error);
	if(error)
	{
		LoggerE("could not create ObjManager proxy");
		// DebugOut(DebugOut::Error)<<"Could not create ObjectManager proxy for Bluez: "<<error->message<<endl;
		g_error_free(error);
		return "";
	}

	GVariant * objectMap = g_dbus_proxy_call_sync(managerProxy, "GetManagedObjects",nullptr, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

	if(error)
	{
		LoggerE("failed to get GetManagedObj");
		//DebugOut(DebugOut::Error)<<"Failed call to GetManagedObjects: "<<error->message<<endl;
		g_object_unref(managerProxy);
		g_error_free(error);
		return "";
	}

	GVariantIter* iter;
	char* objPath;
	GVariantIter* level2Dict;

	g_variant_get(objectMap, "(a{oa{sa{sv}}})",&iter);
	while(g_variant_iter_next(iter, "{oa{sa{sv}}}",&objPath, &level2Dict))
	{
		char * interfaceName;
		GVariantIter* innerDict;
		while(g_variant_iter_next(level2Dict, "{sa{sv}}", &interfaceName, &innerDict))
		{
			if(!strcmp(interfaceName, "org.bluez.Device1"))
			{
				char* propertyName;
				GVariant* value;

				while(done == false && g_variant_iter_next(innerDict,"{sv}", &propertyName, &value))
				{
					if(!strcmp(propertyName, "Address"))
					{
						char* addr;
						g_variant_get(value,"s",&addr);
						if(addr && std::string(addr) == address)
						{

							result = objPath?strdup(objPath):NULL;
							done = true;
							LoggerD("getDeviceFromAddress found : " << result);
						}

						g_free(addr);
					}
					g_free(propertyName);
					g_variant_unref(value);
				}
			}
			g_free(interfaceName);
			g_variant_iter_free(innerDict);
		}
		g_free(objPath);
		g_variant_iter_free(level2Dict);
	}
	g_variant_iter_free(iter);

	return result;
}

bool Bluez::isDevicePaired(const char *bt_addr) {
    bool paired = false;
    std::string addrStr = std::string(bt_addr);
    gchar* device = getDeviceFromAddress(addrStr);

    if(!mAdapterPath)
        return paired;

    GError *err = NULL;
    GVariant *reply = NULL;
	reply = g_dbus_connection_call_sync( g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL),
	                                     BLUEZ_SERVICE,
	                                     device,
	                                     "org.freedesktop.DBus.Properties",
	                                     "Get",
	                                     g_variant_new("(ss)", "org.bluez.Device1", "Paired"),
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

	GVariant *value;
	g_variant_get(reply, "(v)", &value);
	paired = g_variant_get_boolean(value);

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
    //mAgentIntrospectionData = g_dbus_node_info_new_for_xml(AGENT_INTERFACE_XML, NULL);

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

