
/*
 *
 * A logger to be used instead of the
 * default one <Logger.h>, which prints
 * to DLOG
 *
 */


#ifndef LOGGER_H__
#define LOGGER_H__

#include <sstream>

#undef LOG_TAG
#define LOG_TAG "WRT_PLUGINS/TIZEN"

#define _LOGGER(fmt, args...) \
    do { \
        std::ostringstream platformLog; \
        platformLog << fmt; \
        char buf[1024]; \
        sprintf(buf, platformLog.str().c_str(), ##args); \
        printf("%s(%d) > %s\n", __func__, __LINE__, buf); \
    } while(0)

#define LoggerD(fmt, args...)    _LOGGER(fmt, ##args)
#define LoggerI(fmt, args...)    _LOGGER(fmt, ##args)
#define LoggerW(fmt, args...)    _LOGGER(fmt, ##args)
#define LoggerE(fmt, args...)    _LOGGER(fmt, ##args)

#endif // LOGGER_H__

