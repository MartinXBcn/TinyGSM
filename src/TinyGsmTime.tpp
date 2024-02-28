/**
 * @file       TinyGsmTime.tpp
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMTIME_H_
#define SRC_TINYGSMTIME_H_

#include "TinyGsmCommon.h"

#define TINY_GSM_MODEM_HAS_TIME

enum TinyGSMDateTimeFormat { DATE_FULL = 0, DATE_TIME = 1, DATE_DATE = 2 };

template <class modemType>
class TinyGsmTime {
 public:
  /*
   * Time functions
   */
  String getGSMDateTime(TinyGSMDateTimeFormat format) {
    String s;

    MS_TINY_GSM_SEM_TAKE_WAIT("getGSMDateTime")

    s = thisModem().getGSMDateTimeImpl(format);

    MS_TINY_GSM_SEM_GIVE_WAIT

    return s;
  }
  
  bool getNetworkTime(int* year, int* month, int* day, int* hour, int* minute,
                      int* second, float* timezone) {
    bool b = false;

    MS_TINY_GSM_SEM_TAKE_WAIT("getNetworkTime")

    b = thisModem().getNetworkTimeImpl(year, month, day, hour, minute,
                                          second, timezone);

    MS_TINY_GSM_SEM_GIVE_WAIT

    return b;
  }

  /*
   * CRTP Helper
   */
 protected:
  inline const modemType& thisModem() const {
    return static_cast<const modemType&>(*this);
  }
  inline modemType& thisModem() {
    return static_cast<modemType&>(*this);
  }

  /*
   * Time functions
   */
 protected:
  String getGSMDateTimeImpl(TinyGSMDateTimeFormat format) {
    thisModem().sendAT(GF("+CCLK?"));
    if (thisModem().waitResponse(2000L, GF("+CCLK: \"")) != 1) { return ""; }

    String res;

    switch (format) {
      case DATE_FULL: res = thisModem().stream.readStringUntil('"'); break;
      case DATE_TIME:
        thisModem().streamSkipUntil(',');
        res = thisModem().stream.readStringUntil('"');
        break;
      case DATE_DATE: res = thisModem().stream.readStringUntil(','); break;
    }
    thisModem().waitResponse();  // Ends with OK
    return res;
  }

  bool getNetworkTimeImpl(int* year, int* month, int* day, int* hour,
                          int* minute, int* second, float* timezone) {
    DBGLOG(Info, "[TinyGsmTime] >>")
    int iyear     = 0;
    int imonth    = 0;
    int iday      = 0;
    int ihour     = 0;
    int imin      = 0;
    int isec      = 0;
    int itimezone = 0;
    char tzSign;
    bool ret = false;


    DBGLOG(Info, "[TinyGsmTime] AT+CLTS?")
    thisModem().sendAT(GF("+CLTS?"));
    thisModem().waitResponse(2000L);

    DBGLOG(Info, "[TinyGsmTime] AT+CLTS=?")
    thisModem().sendAT(GF("+CLTS=?"));
    thisModem().waitResponse(2000L);


    DBGLOG(Info, "[TinyGsmTime] AT+CCLK?")
    thisModem().sendAT(GF("+CCLK?"));
    if (thisModem().waitResponse(2000L, GF("+CCLK: \"")) != 1) { 
      DBGLOG(Error, "[TinyGsmTime] read-time failed!")
      goto end; 
    }

    // Date & Time
    iyear       = thisModem().streamGetIntBefore('/');
    imonth      = thisModem().streamGetIntBefore('/');
    iday        = thisModem().streamGetIntBefore(',');
    ihour       = thisModem().streamGetIntBefore(':');
    imin        = thisModem().streamGetIntBefore(':');
    isec        = thisModem().streamGetIntLength(2);
    tzSign      = thisModem().stream.read();
    itimezone   = thisModem().streamGetIntBefore('\n');
    if (tzSign == '-') { itimezone = itimezone * -1; }

    // Set pointers
    if (iyear < 2000) iyear += 2000;
    if (year != NULL) *year = iyear;
    if (month != NULL) *month = imonth;
    if (day != NULL) *day = iday;
    if (hour != NULL) *hour = ihour;
    if (minute != NULL) *minute = imin;
    if (second != NULL) *second = isec;
    if (timezone != NULL) *timezone = static_cast<float>(itimezone) / 4.0;

    // Final OK
    thisModem().waitResponse();
    ret = true;

end:    
    DBGLOG(Info, "[TinyGsmTime] >> return: %s", DBGB2S(ret))
    return ret;
  }
};

#endif  // SRC_TINYGSMTIME_H_
