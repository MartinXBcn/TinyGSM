/**
 * @file       TinyGsmGPRS.tpp
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMGPRS_H_
#define SRC_TINYGSMGPRS_H_

#include "TinyGsmCommon.h"

#define TINY_GSM_MODEM_HAS_GPRS

enum SimStatus {
  SIM_ERROR            = 0,
  SIM_READY            = 1,
  SIM_LOCKED           = 2,
  SIM_ANTITHEFT_LOCKED = 3,
};

template <class modemType>
class TinyGsmGPRS {
 public:
  /*
   * SIM card functions
   */
  // Unlocks the SIM
  bool simUnlock(const char* pin) {
    bool b = false;

    MS_TINY_GSM_SEM_TAKE_WAIT("simUnlock")

    b = thisModem().simUnlockImpl(pin);

    MS_TINY_GSM_SEM_GIVE_WAIT

    return b;
  }
  // Gets the CCID of a sim card via AT+CCID
  String getSimCCID() {
    String s;

    MS_TINY_GSM_SEM_TAKE_WAIT("getSimCCID")

    s = thisModem().getSimCCIDImpl();

    MS_TINY_GSM_SEM_GIVE_WAIT

    return s;
  }


  // Asks for TA Serial Number Identification (IMEI)
  String getIMEI() {
    String s;

    MS_TINY_GSM_SEM_TAKE_WAIT("getIMEI")

    s = thisModem().getIMEIImpl();

    MS_TINY_GSM_SEM_GIVE_WAIT

    return s;
  }


  // Asks for International Mobile Subscriber Identity IMSI
  String getIMSI() {
    String s;

    MS_TINY_GSM_SEM_TAKE_WAIT("getIMSI")

    s = thisModem().getIMSIImpl();

    MS_TINY_GSM_SEM_GIVE_WAIT

    return s;
  }


  SimStatus getSimStatus(uint32_t timeout_ms = 10000L) {
    SimStatus simstatus;

    MS_TINY_GSM_SEM_TAKE_WAIT("getSimStatus")

    simstatus = thisModem().getSimStatusImpl(timeout_ms);

    MS_TINY_GSM_SEM_GIVE_WAIT

    return simstatus;
  }

  /*
   * GPRS functions
   */
  bool gprsConnect(const char* apn, const char* user = NULL,
                   const char* pwd = NULL) {
    return thisModem().gprsConnectImpl(apn, user, pwd);
  }
  bool gprsDisconnect() {
    return thisModem().gprsDisconnectImpl();
  }
  // Checks if current attached to GPRS/EPS service
  bool isGprsConnected() {
    return thisModem().isGprsConnectedImpl();
  }
  // Gets the current network operator
  String getOperator() {
    String o;

    MS_TINY_GSM_SEM_TAKE_WAIT("getOperator")

    o = thisModem().getOperatorImpl();

    MS_TINY_GSM_SEM_GIVE_WAIT

    return o;
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
   * SIM card functions
   */
 protected:
  // Unlocks a sim via the 3GPP TS command AT+CPIN
  bool simUnlockImpl(const char* pin) {
    if (pin && strlen(pin) > 0) {
      thisModem().sendAT(GF("+CPIN=\""), pin, GF("\""));
      return thisModem().waitResponse() == 1;
    }
    return true;
  }

  // Gets the CCID of a sim card via AT+CCID
  String getSimCCIDImpl() {
    thisModem().sendAT(GF("+CCID"));
    if (thisModem().waitResponse(GF("+CCID:")) != 1) { return ""; }
    String res = thisModem().stream.readStringUntil('\n');
    thisModem().waitResponse();
    res.trim();
    return res;
  }

  // Asks for TA Serial Number Identification (IMEI) via the V.25TER standard
  // AT+GSN command
  String getIMEIImpl() {
    thisModem().sendAT(GF("+GSN"));
    thisModem().streamSkipUntil('\n');  // skip first newline
    String res = thisModem().stream.readStringUntil('\n');
    thisModem().waitResponse();
    res.trim();
    return res;
  }

  // Asks for International Mobile Subscriber Identity IMSI via the AT+CIMI
  // command
  String getIMSIImpl() {
    thisModem().sendAT(GF("+CIMI"));
    thisModem().streamSkipUntil('\n');  // skip first newline
    String res = thisModem().stream.readStringUntil('\n');
    thisModem().waitResponse();
    res.trim();
    return res;
  }

  SimStatus getSimStatusImpl(uint32_t timeout_ms = 10000L) {
    for (uint32_t start = millis(); millis() - start < timeout_ms;) {
      thisModem().sendAT(GF("+CPIN?"));
      if (thisModem().waitResponse(GF("+CPIN:")) != 1) {
        delay(1000);
        continue;
      }
      int8_t status =
          thisModem().waitResponse(GF("READY"), GF("SIM PIN"), GF("SIM PUK"),
                                   GF("NOT INSERTED"), GF("NOT READY"));
      thisModem().waitResponse();
      switch (status) {
        case 2:
        case 3: return SIM_LOCKED;
        case 1: return SIM_READY;
        default: return SIM_ERROR;
      }
    }
    return SIM_ERROR;
  }

  /*
   * GPRS functions
   */
 protected:
  // Checks if current attached to GPRS/EPS service
  bool isGprsConnectedImpl() {
    DBGLOG(Debug, "[TinyGsmGPRS] >>")

    MS_TINY_GSM_SEM_TAKE_WAIT("isGprsConnectedImpl")

    int8_t res;
    bool ret = false;
    thisModem().sendAT(GF("+CGATT?"));
    if (thisModem().waitResponse(GF("+CGATT:")) != 1) { 

      MS_TINY_GSM_SEM_GIVE_WAIT
      
      goto end; 
    }
    res = thisModem().streamGetIntBefore('\n');
    thisModem().waitResponse();

    // localIP() also blocks, therefore "give" already here.
    MS_TINY_GSM_SEM_GIVE_WAIT

    if (res == 1) { ret = thisModem().localIP() != IPAddress(0, 0, 0, 0); }

end:
    DBGLOG(Debug, "[TinyGsmGPRS] << return: %s", DBGB2S(ret))

    return ret;
  }

  // Gets the current network operator via the 3GPP TS command AT+COPS
  String getOperatorImpl() {
    thisModem().sendAT(GF("+COPS?"));
    if (thisModem().waitResponse(GF("+COPS:")) != 1) { return ""; }
    thisModem().streamSkipUntil('"'); /* Skip mode and format */
    String res = thisModem().stream.readStringUntil('"');
    thisModem().waitResponse();
    return res;
  }
};

#endif  // SRC_TINYGSMGPRS_H_
