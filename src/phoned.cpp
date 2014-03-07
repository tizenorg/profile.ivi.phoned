
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "phone.h"

#include "Logger.h"

GMainLoop *loop = NULL;

int main (int argc, char *argv[])
{
    PhoneD::Phone *phone = new PhoneD::Phone();
    if(!phone) {
        LoggerD("Error initializing Phone Service");
        return -1;
    }

    LoggerD("Starting GMainLoop");
    loop = g_main_loop_new(NULL, TRUE);
    if(!loop) {
        LoggerD("Failed to create GMainLoop");
        return -2;
    }
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    delete phone;

    return 0;
}

