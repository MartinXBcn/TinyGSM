/**
 * @file       TinyGsmCommon.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCOMMON_H_
#define SRC_TINYGSMCOMMON_H_

// The current library version number
#define TINYGSM_VERSION "0.11.5"




// <MS> >>

#ifdef TINY_GSM_DEBUG
#define ESP32DEBUGGING 
#else
#undef ESP32DEBUGGING
#endif
#include "ESP32Logger.h"

//#define msTinyGsmLogLevel Debug
#define msTinyGsmLogLevel Info

extern SemaphoreHandle_t msTinyGsmSemCriticalProcess;

//#define logLevelInfo Info
#define logLevelInfo Debug
#define logLevelWarn Warn

#define MS_TINY_GSM_SEM_BLOCKEDBY_MAXLEN 64
extern char msTinyGsmSemBlockedBy[MS_TINY_GSM_SEM_BLOCKEDBY_MAXLEN];

// Check [msTinyGsmSemCriticalProcess] and wait if necessary until it becomes available.
#define MS_TINY_GSM_SEM_TAKE_WAIT(blockedby) \
	if (xSemaphoreTake(msTinyGsmSemCriticalProcess, 0) == pdFALSE) { \
		DBGLOG(logLevelWarn, "### TINY_GSM ### ---> sem not available, wait until it becomes available. Blocked by: %s", msTinyGsmSemBlockedBy); \
		xSemaphoreTake(msTinyGsmSemCriticalProcess, portMAX_DELAY); \
		ms_strncpy(msTinyGsmSemBlockedBy, blockedby, strlen(blockedby), MS_TINY_GSM_SEM_BLOCKEDBY_MAXLEN); \
		DBGLOG(logLevelWarn, "### TINY_GSM ### ---> sem now available, proceed with code."); \
	} else { \
		ms_strncpy(msTinyGsmSemBlockedBy, blockedby, strlen(blockedby), MS_TINY_GSM_SEM_BLOCKEDBY_MAXLEN); \
  	DBGLOG(logLevelInfo, "### TINY_GSM ### ---> sem available, proceed with code."); \
	}

// Check [msTinyGsmSemCriticalProcess] w/o waiting, if available proceed with program code,
// otherwise skip program code w/o waiting.
#define MS_TINY_GSM_SEM_TAKE_IF_AVAILABLE(blockedby) \
	if (xSemaphoreTake(msTinyGsmSemCriticalProcess, 0) == pdFALSE) { \
		DBGLOG(logLevelWarn, "### TINY_GSM ### ---> sem not available, do not wait, skip. Blocked by: %s", msTinyGsmSemBlockedBy); \
	} else { \
		ms_strncpy(msTinyGsmSemBlockedBy, blockedby, strlen(blockedby), MS_TINY_GSM_SEM_BLOCKEDBY_MAXLEN); \
		DBGLOG(logLevelInfo, "### TINY_GSM ### ---> sem available, proceed with code."); \
  }

// End of a block started with [MS_TINY_GSM_SEM_TAKE_WAIT].
#define MS_TINY_GSM_SEM_GIVE_WAIT \
	{ \
		DBGLOG(logLevelInfo, "### TINY_GSM ### <--- sem-give"); \
		ms_strncpy(msTinyGsmSemBlockedBy, "", 0, MS_TINY_GSM_SEM_BLOCKEDBY_MAXLEN); \
		DBGCOD(BaseType_t r =) \
    xSemaphoreGive(msTinyGsmSemCriticalProcess); \
		DBGCHK(Error, r == pdTRUE, "### TINY_GSM ### <--- sem-give returned error."); \
	} 

// End of a block started with [MS_TINY_GSM_SEM_TAKE_IF_AVAILABLE].
#define MS_TINY_GSM_SEM_GIVE_IF_AVAILABLE \
		MS_TINY_GSM_SEM_GIVE_WAIT \
	} 

// Returns true if the semaphore is in use by an other function, 
// or false if it is available.
// Does not block anything, just checks.
#define MS_TINY_GSM_SEM_BLOCKED \
	(uxSemaphoreGetCount(msTinyGsmSemCriticalProcess) == 0)

// <MS> <<





#if defined(SPARK) || defined(PARTICLE)
#include "Particle.h"
#elif defined(ARDUINO)
#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#endif

#if defined(ARDUINO_DASH)
#include <ArduinoCompat/Client.h>
#else
#include <Client.h>
#endif

#ifndef TINY_GSM_YIELD_MS
#define TINY_GSM_YIELD_MS 0
#endif

#ifndef TINY_GSM_YIELD
#define TINY_GSM_YIELD() \
  { delay(TINY_GSM_YIELD_MS); }
#endif

#define TINY_GSM_ATTR_NOT_AVAILABLE \
  __attribute__((error("Not available on this modem type")))
#define TINY_GSM_ATTR_NOT_IMPLEMENTED __attribute__((error("Not implemented")))

#if defined(__AVR__) && !defined(__AVR_ATmega4809__)
#define TINY_GSM_PROGMEM PROGMEM
typedef const __FlashStringHelper* GsmConstStr;
#define GFP(x) (reinterpret_cast<GsmConstStr>(x))
#define GF(x) F(x)
#else
#define TINY_GSM_PROGMEM
typedef const char* GsmConstStr;
#define GFP(x) x
#define GF(x) x
#endif

#ifdef TINY_GSM_DEBUG
namespace {
template <typename T>
static void DBG_PLAIN(T last) {
  TINY_GSM_DEBUG.println(last);
}

template <typename T, typename... Args>
static void DBG_PLAIN(T head, Args... tail) {
  TINY_GSM_DEBUG.print(head);
  TINY_GSM_DEBUG.print(' ');
  DBG_PLAIN(tail...);
}

template <typename... Args>
static void DBG(Args... args) {
  TINY_GSM_DEBUG.print(GF("["));
  TINY_GSM_DEBUG.print(millis());
  TINY_GSM_DEBUG.print(GF("] "));
  DBG_PLAIN(args...);
}
}  // namespace
#else
#define DBG_PLAIN(...)
#define DBG(...)
#endif

template <class T>
const T& TinyGsmMin(const T& a, const T& b) {
  return (b < a) ? b : a;
}

template <class T>
const T& TinyGsmMax(const T& a, const T& b) {
  return (b < a) ? a : b;
}

template <class T>
uint32_t TinyGsmAutoBaud(T& SerialAT, uint32_t minimum = 9600,
                         uint32_t maximum = 115200) {
  static uint32_t rates[] = {115200, 57600,  38400, 19200, 9600,  74400, 74880,
                             230400, 460800, 2400,  4800,  14400, 28800};

  for (uint8_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
    uint32_t rate = rates[i];
    if (rate < minimum || rate > maximum) continue;

    DBG("Trying baud rate", rate, "...");
    SerialAT.begin(rate);
    delay(10);
    for (int j = 0; j < 10; j++) {
      SerialAT.print("AT\r\n");
      String input = SerialAT.readString();
      if (input.indexOf("OK") >= 0) {
        DBG("Modem responded at rate", rate);
        return rate;
      }
    }
  }
  SerialAT.begin(minimum);
  return 0;
}

#endif  // SRC_TINYGSMCOMMON_H_
