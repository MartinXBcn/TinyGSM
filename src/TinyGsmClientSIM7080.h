/**
 * @file       TinyGsmClientSim7080.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCLIENTSIM7080_H_
#define SRC_TINYGSMCLIENTSIM7080_H_

// #define TINY_GSM_DEBUG Serial
// #define TINY_GSM_USE_HEX


// For char-to-hex-conversion.
#include "ms_General.h"


#define TINY_GSM_MUX_COUNT 12
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE

#include "TinyGsmClientSIM70xx.h"
#include "TinyGsmTCP.tpp"
#include "TinyGsmSSL.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmGSMLocation.tpp"
#include "TinyGsmTime.tpp"
#include "TinyGsmNTP.tpp"
#include "TinyGsmBattery.tpp"


// Logging
#undef MS_LOGGER_LEVEL
#ifdef MS_TINYGSM_LOGGING
#define MS_LOGGER_LEVEL MS_TINYGSM_LOGGING
#endif
#include "ESP32Logger.h"



class TinyGsmSim7080 : public TinyGsmSim70xx<TinyGsmSim7080>,
                       public TinyGsmTCP<TinyGsmSim7080, TINY_GSM_MUX_COUNT>,
                       public TinyGsmSSL<TinyGsmSim7080, TINY_GSM_MUX_COUNT>,
                       public TinyGsmSMS<TinyGsmSim7080>,
                       public TinyGsmGSMLocation<TinyGsmSim7080>,
                       public TinyGsmTime<TinyGsmSim7080>,
                       public TinyGsmNTP<TinyGsmSim7080>,
                       public TinyGsmBattery<TinyGsmSim7080> {
  friend class TinyGsmSim70xx<TinyGsmSim7080>;
  friend class TinyGsmModem<TinyGsmSim7080>;
  friend class TinyGsmGPRS<TinyGsmSim7080>;
  friend class TinyGsmTCP<TinyGsmSim7080, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSSL<TinyGsmSim7080, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSMS<TinyGsmSim7080>;
  friend class TinyGsmGSMLocation<TinyGsmSim7080>;
  friend class TinyGsmGPS<TinyGsmSim7080>;
  friend class TinyGsmTime<TinyGsmSim7080>;
  friend class TinyGsmNTP<TinyGsmSim7080>;
  friend class TinyGsmBattery<TinyGsmSim7080>;

  /*
   * Inner Client
   */
 public:
  class GsmClientSim7080 : public GsmClient {
    friend class TinyGsmSim7080;

   public:
    GsmClientSim7080() {
      DBGLOG(Info, "[GsmClientSim7080] >>")
      DBGLOG(Info, "[GsmClientSim7080] ATTENTION: constructor does not do anything!")
      DBGLOG(Info, "[GsmClientSim7080] <<")
    }

    explicit GsmClientSim7080(TinyGsmSim7080& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    ~GsmClientSim7080() {
      DBGLOG(Info, "[GsmClientSim7080] >> mux: ", mux)
      at->sockets[this->mux] = NULL;
      DBGLOG(Info, "[GsmClientSim7080] <<")
    }

    bool init(TinyGsmSim7080* modem, uint8_t mux = 0) {
      DBGLOG(Info, "[GsmClientSim7080] >> mux: %hhu", mux)
      this->at       = modem;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      if (mux < TINY_GSM_MUX_COUNT) {
        this->mux = mux;
      } else {
        this->mux = (mux % TINY_GSM_MUX_COUNT);
      }

      bool r = false;
      if (at->sockets[this->mux] == NULL) {
        at->sockets[this->mux] = this;
        r = true;
      } else {
        DBGLOG(Warn, "[GsmClientSim7080] ERROR.")
        r = false;
      }
      DBGLOG(Info, "[GsmClientSim7080] << return: %s", DBGB2S(r))
      return r;
    } // bool GsmClientSim7080::init(...)

   public:
    virtual int connect(const char* host, uint16_t port, int timeout_s) {
      DBGLOG(Info, "[GsmClientSim7080] >>")
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      DBGLOG(Info, "[GsmClientSim7080] << return sock_connected: %s", DBGB2S(sock_connected))
      return sock_connected;
    } // GsmClientSim7080::int connect(...)
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    void stop(uint32_t maxWaitMs) {
      DBGLOG(Info, "[GsmClientSim7080] >>")

      dumpModemBuffer(maxWaitMs);

      MS_TINY_GSM_SEM_TAKE_WAIT("stop")

      at->sendAT(GF("+CACLOSE="), mux);
      sock_connected = false;
      at->waitResponse(3000);

      MS_TINY_GSM_SEM_GIVE_WAIT

      DBGLOG(Info, "[GsmClientSim7080] <<")
    } // GsmClientSim7080::stop(...)
    
    void stop() override {
      stop(15000L);
    } // GsmClientSim7080::stop()

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  }; // class class GsmClientSim7080



  /*
   * Inner Secure Client
   */
 public:
  class GsmClientSecureSIM7080 : public GsmClientSim7080 {
   public:
    GsmClientSecureSIM7080() {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >>")
      DBGLOG(Info, "[GsmClientSecureSIM7080] ATTENTION: constructor does not do anything!")
      DBGLOG(Info, "[GsmClientSecureSIM7080] <<")
    }

    explicit GsmClientSecureSIM7080(TinyGsmSim7080& modem, uint8_t mux = 0)
        : GsmClientSim7080(modem, mux) {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> mux: %hhu", mux)
      DBGLOG(Info, "[GsmClientSecureSIM7080] <<")
    }

    bool setCertificate(const String& certificateName) {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> certificateName: '%s'", certificateName.c_str())
      bool r = at->setCertificate(certificateName, mux);
      DBGLOG(Info, "[GsmClientSecureSIM7080] << return: %s", DBGB2S(r))
      return r;
    }

    int connect(const char* host, uint16_t port, int timeout_s) override {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> host: '%s', port: %hu", host == NULL ? "-" : host, port)
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      DBGLOG(Info, "[GsmClientSecureSIM7080] << return sock_connected: %s", DBGB2S(sock_connected))
      return sock_connected;
    } // GsmClientSecureSIM7080::connect(...)
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
  }; // GsmClientSecureSIM7080

 // <MS>
 public:
  // Set by handleURCs(...) when receiving "DST" from the network.
  // -1: not set, 0: standard time, 1: daylight saving time
  int dayLightSaving = -1;

  /*
   * Constructor
   */
 public:
  explicit TinyGsmSim7080(Stream& stream)
      : TinyGsmSim70xx<TinyGsmSim7080>(stream) {
    DBGLOG(Info, "[TinyGsmSim7080] >>");
    memset(sockets, 0, sizeof(sockets));
    msTinyGsmSemCriticalProcess = xSemaphoreCreateMutex();
    DBGCHK(Error, msTinyGsmSemCriticalProcess != NULL, "[TinyGsmSim7080] CRITICAL ERROR msTinyGsmSemCriticalProcess can not be created!");
    DBGLOG(Info, "[TinyGsmSim7080] <<");
  } // TinyGsmSim7080::TinyGsmSim7080(...)

  /*
   * Basic functions
   */
 protected:
  bool initRunning = false;
  bool initImpl(const char* pin = nullptr) {
    SimStatus ret;
    bool r = false;
    bool gotATOK = false;
    DBGLOG(Info, "[TinyGsmSim7080] >> pin: '%s'", pin == NULL ? "-" : pin);
    DBGLOG(Info, "[TinyGsmSim7080] Version: %s", TINYGSM_VERSION);
    DBGLOG(Info, "[TinyGsmSim7080] Compiled Module: TinyGsmClientSIM7080");

    if (initRunning) {
      DBGLOG(Warn, "[TinyGsmSim7080] Initialization already running!")
      goto endxx;
    }

    MS_TINY_GSM_SEM_TAKE_WAIT("initImpl")

    initRunning = true;

/* <MS> old 0.11.5
    if (!testAT()) { 
      DBGLOG(Error, "[TinyGsmSim7080] testAT() failed! Stop.")
      r = false; 
      goto end; 
    }

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { 
      DBGLOG(Error, "[TinyGsmSim7080] Echo-off failed! Stop.")
      r = false; 
      goto end; 
    }
*/

    for (uint32_t start = millis(); millis() - start < 10000L;) {
      sendAT(GF(""));
      int8_t resp = waitResponse(200L, GFP(GSM_OK), GFP(GSM_ERROR), GF("AT"));
      if (resp == 1) {
        gotATOK = true;
        break;
      } else if (resp == 3) {
        waitResponse(200L);  // get the OK
        sendAT(GF("E0"));    // Echo Off
        DBGLOG(Info, "[TinyGsmSim7080] Turning off echo!")
        waitResponse(2000L);
      }
      delay(100);
    }
    if (!gotATOK) { goto end; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    waitResponse();

    // Enable Local Time Stamp for getting network time
    sendAT(GF("+CLTS=1"));
    if (waitResponse(10000L) != 1) { 
      DBGLOG(Error, "[TinyGsmSim7080] Enable-local-timestamp failed! Stop.")
      goto end; 
    }

    // Enable battery checks
// <MS>
// IMPORTANT: 
// +CBATCHK=1 has to be set! Or not?
//    sendAT(GF("+CBATCHK=0"));
    sendAT(GF("+CBATCHK=1"));
    // Disable battery checks
    if (waitResponse() != 1) { 
      DBGLOG(Error, "[TinyGsmSim7080] Disable-bat-checks failed! Stop.")
      goto end; 
    }


    MS_TINY_GSM_SEM_GIVE_WAIT

    ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != nullptr && strlen(pin) > 0) {
      simUnlock(pin);
      r = (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      r = ((ret == SIM_READY) || (ret == SIM_LOCKED));
    }

    goto endx;

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

endx:
    initRunning = false;

endxx:
    DBGLOG(Info, "[TinyGsmSim7080] << return: %s", DBGB2S(r));
    return r;
  } // TinyGsmSim7080::initImpl(...)

  void maintainImpl() {
    // Keep listening for modem URC's and proactively iterate through
    // sockets asking if any data is avaiable
//    DBGLOG(Info, "[TinyGsmSim7080::maintainImpl] >>");

    // MS_TINY_GSM_SEM_TAKE_WAIT("maintainImpl")
    xSemaphoreTake(msTinyGsmSemCriticalProcess, portMAX_DELAY);

    bool check_socks = false;
    for (int mux = 0; mux < TINY_GSM_MUX_COUNT; mux++) {
      GsmClientSim7080* sock = sockets[mux];
      if (sock && sock->got_data) {
        sock->got_data = false;
        check_socks    = true;
      }
    }
    // modemGetAvailable checks all socks, so we only want to do it once
    // modemGetAvailable calls modemGetConnected(), which also checks allf

    // Check all mux in use.
    if (check_socks) { modemGetAvailable((uint8_t)-1); }

    while (stream.available()) { waitResponse(15, nullptr, nullptr); }

    // MS_TINY_GSM_SEM_GIVE_WAIT
    xSemaphoreGive(msTinyGsmSemCriticalProcess); 

//    DBGLOG(Info, "[TinyGsmSim7080::maintainImpl] <<");
  } // TinyGsmSim7080::maintainImpl()

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = nullptr) {
    DBGLOG(Info, "[TinyGsmSim7080] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT("getLocalIPImpl")

    bool success = true;
    int8_t resp;
    String s;

    bool gotATOK = false;
    for (uint32_t start = millis(); millis() - start < 10000L;) {
      sendAT(GF(""));
      resp = waitResponse(200L, s, GFP(GSM_OK), GFP(GSM_ERROR), GF("AT"));
      DBGLOG(Info, "[TinyGsmSim7080] Empty AT returned: %i, %s", resp, s.c_str());
      if (resp == 1) {
        gotATOK = true;
        break;
      } else if (resp == 3) {
        waitResponse(200L);  // get the OK
        DBGLOG(Info, "[TinyGsmSim7080] Turning off echo!");
        sendAT(GF("E0"));  // Echo Off
        waitResponse(2000L);
      }
      delay(100);
    }
    DBGCHK(Error, gotATOK, "[TinyGsmSim7080] Restart failed, no answer 'AT' received!")
    if (!gotATOK) { success = false; goto end; }

    DBGLOG(Info, "[TinyGsmSim7080] setPhoneFunctionality(0) ...");
    if (!setPhoneFunctionality(0)) { 
      DBGLOG(Error, "[TinyGsmSim7080] Restart failed, setPhoneFunctionality(0) failed!")
      goto end; 
    }
    DBGLOG(Info, "[TinyGsmSim7080] setPhoneFunctionality(1, true) ...");
//    if (!setPhoneFunctionality(1, true)) { 
    if (!setPhoneFunctionality(1)) { 
      DBGLOG(Error, "[TinyGsmSim7080] Restart failed, setPhoneFunctionality(1, true) failed!")
      goto end; 
    }

    DBGLOG(Info, "[TinyGsmSim7080] Rebooting ...");
    sendAT(GF("+CREBOOT"));  // Reboot
    success &= waitResponse(30000L, s) == 1;
    DBGCHK(Error, success, "[TinyGsmSim7080] Reboot failed: %s", s.c_str())
    DBGCHK(Info, !success, "[TinyGsmSim7080] Reboot initiated successful: %s", s.c_str())
/**/    
    resp = waitResponse(30000L, s);
//    resp = waitResponse(30000L, s, GF("SMS Ready"));
    DBGLOG(Info, "[TinyGsmSim7080] Reboot response: %i, %s", resp, s.c_str());

    MS_TINY_GSM_SEM_GIVE_WAIT

    success &= initImpl(pin);
    goto endx;

  end:
    MS_TINY_GSM_SEM_GIVE_WAIT

  endx:
    DBGLOG(Info, "[TinyGsmSim7080] << return: %s", DBGB2S(success));
    return success;
  } // TinyGsmSim7080::restartImpl(...)

  /*
   * Generic network functions
   */
 protected:
  String getLocalIPImpl() {
    DBGLOG(Debug, "[TinyGsmSim7080] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT("getLocalIPImpl")

    String res;

    sendAT(GF("+CNACT?"));
    if (waitResponse(GF(AT_NL "+CNACT:")) != 1) { res = ""; goto end; }
    streamSkipUntil('\"');
    res = stream.readStringUntil('\"');
    waitResponse();

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Debug, "[TinyGsmSim7080] << return: %s", res.c_str());
    return res;
  } // String TinyGsmSim7080::getLocalIPImpl()

  /*
   * Secure socket layer (SSL) functions
   */
  // Follows functions as inherited from TinyGsmSSL.tpp

  /*
   * WiFi functions
   */
  // No functions of this type supported

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = nullptr,
                       const char* pwd = nullptr) {
    DBGLOG(Info, "[TinyGsmSim7080] >> apn: '%s', user: %s", apn == nullptr ? "-" : apn, user == nullptr ? "-" : user);
    bool res    = false;
    int  ntries = 0;

    gprsDisconnect();

    // gprsDisconnect() has its own "blocking".
    MS_TINY_GSM_SEM_TAKE_WAIT("gprsConnectImpl")

    DBGLOG(Info, "[TinyGsmSim7080] -1- CGDCONT")

    // Define the PDP context
    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, '"');
    waitResponse();

    DBGLOG(Info, "[TinyGsmSim7080] -2- CGATT")

    // Attach to GPRS
    sendAT(GF("+CGATT=1"));
    if (waitResponse(60000L) != 1) { res = false; goto end; }

    // NOTE:  **DO NOT** activate the PDP context
    // For who only knows what reason, doing so screws up the rest of the
    // process

    DBGLOG(Info, "[TinyGsmSim7080] -3- CGNAPN")

    // Check the APN returned by the server
    // not sure why, but the connection is more consistent with this
    sendAT(GF("+CGNAPN"));
    waitResponse();

    DBGLOG(Info, "[TinyGsmSim7080] -4- CNCFG")

    // Bearer settings for applications based on IP
    // Set the user name and password
    // AT+CNCFG=<pdpidx>,<ip_type>,[<APN>,[<usename>,<password>,[<authentication>]]]
    // <pdpidx> PDP Context Identifier - for reasons not understood by me,
    //          use PDP context identifier of 0 for what we defined as 1 above
    // <ip_type> 0: Dual PDN Stack
    //           1: Internet Protocol Version 4
    //           2: Internet Protocol Version 6
    // <authentication> 0: NONE
    //                  1: PAP
    //                  2: CHAP
    //                  3: PAP or CHAP
    if (pwd && strlen(pwd) > 0 && user && strlen(user) > 0) {
      sendAT(GF("+CNCFG=0,1,\""), apn, "\",\"", user, "\",\"", pwd, "\",3");
      waitResponse();
    } else if (user && strlen(user) > 0) {
      // Set the user name only
      sendAT(GF("+CNCFG=0,1,\""), apn, "\",\"", user, '"');
      waitResponse();
    } else {
      // Set the APN only
      sendAT(GF("+CNCFG=0,1,\""), apn, '"');
      waitResponse();
    }

    // Activate application network connection
    // AT+CNACT=<pdpidx>,<action>
    // <pdpidx> PDP Context Identifier - for reasons not understood by me,
    //          use PDP context identifier of 0 for what we defined as 1 above
    // <action> 0: Deactive
    //          1: Active
    //          2: Auto Active
    while (!res && ntries < 5) {
      DBGLOG(Info, "[TinyGsmSim7080] CNACT ntries: %i", ntries);
      sendAT(GF("+CNACT=0,1"));
      res = waitResponse(60000L, GF(AT_NL "+APP PDP: 0,ACTIVE"),
                         GF(AT_NL "+APP PDP: 0,DEACTIVE"));
      waitResponse();
      ntries++;
    }

  end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Info, "[TinyGsmSim7080] << return: %s", DBGB2S(res));
    return res;
  } // TinyGsmSim7080::gprsConnectImpl(...)

  bool gprsDisconnectImpl() {
    // Shut down the general application TCP/IP connection
    // CNACT will close *all* open application connections
    DBGLOG(Info, "[TinyGsmSim7080] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT("gprsDisconnectImpl")

    bool ret = false;

    sendAT(GF("+CNACT=0,0"));
    if (waitResponse(60000L) == 1) {
      sendAT(GF("+CGATT=0"));  // Deactivate the bearer context
      if (waitResponse(60000L) == 1) { ret = true; }
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Info, "[TinyGsmSim7080] << return: %s", DBGB2S(ret));
    return ret;
  } // TinyGsmSim7080::gprsDisconnectImpl()

  /*
   * SIM card functions
   */
  // Follows functions as inherited from TinyGsmClientSIM70xx.h

  /*
   * Phone Call functions
   */
  // No functions of this type supported

  /*
   * Audio functions
   */
  // No functions of this type supported

  /*
   * Text messaging (SMS) functions
   */
  // Follows all text messaging (SMS) functions as inherited from TinyGsmSMS.tpp

  /*
   * GSM Location functions
   */
  // Follows all GSM-based location functions as inherited from
  // TinyGsmGSMLocation.tpp

  /*
   * GPS/GNSS/GLONASS location functions
   */
  // Follows functions as inherited from TinyGsmClientSIM70xx.h

  /*
   * Time functions
   */
  // Follows all clock functions as inherited from TinyGsmTime.tpp

  /*
   * NTP server functions
   */
 protected:
  byte NTPServerSyncImpl(String server = "pool.ntp.org", int TimeZone = 0) {
    DBGLOG(Info, "[TinyGsmSim7080] >> server: %s, TimeZone: %i", server.c_str(), TimeZone);
    byte r = -1;

    MS_TINY_GSM_SEM_TAKE_WAIT("NTPServerSyncImpl")

    // Set GPRS bearer profile to associate with NTP sync
    // this may fail, it's not supported by all modules
    sendAT(GF("+CNTPCID=0"));  // CID must be 0. With 1 (like other modules)
                               // does not work!
    waitResponse(10000L);

    // Set NTP server and timezone
    sendAT(GF("+CNTP=\""), server, "\",", String(TimeZone));
    if (waitResponse(10000L) != 1) { goto end; }

    // Request network synchronization
    sendAT(GF("+CNTP"));
    if (waitResponse(10000L, GF("+CNTP:"))) {
      String result = stream.readStringUntil('\n');
      // Check for ',' in case the module appends the time next to the return
      // code. Eg: +CNTP: <code>[,<time>]
      int index = result.indexOf(',');
      if (index > 0) { result.remove(index); }
      result.trim();
      if (TinyGsmIsValidNumber(result)) { r = result.toInt(); goto end; }
    } else {
      DBGLOG(Error, "[TinyGsmSim7080] ERROR: Request network syncrhonisation failed.");
    }

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Info, "[TinyGsmSim7080] << return: %hhu", r);
    return r;
  } // TinyGsmSim7080::NTPServerSyncImpl(...)

  String ShowNTPErrorImpl(byte error) {
    switch (error) {
      case 1: return "Network time synchronization is successful";
      case 61: return "Network error";
      case 62: return "DNS resolution error";
      case 63: return "Connection error";
      case 64: return "Service response error";
      case 65: return "Service response timeout";
      default: return "Unknown error: " + String(error);
    }
  }

  /*
   * BLE functions
   */
  // No functions of this type supported

  /*
   * Battery functions
   */
  // Follows all battery functions as inherited from TinyGsmBattery.tpp

  /*
   * Temperature functions
   */
  // No functions of this type supported

  /*
   * Client related functions
   */
 protected:
// <MS>
DBGCOD(
  const char* getCaopenResultText(int8_t res) {
    // See "SIM7070_SIM7080_SIM7090 Series_AT Command Manual_V1.07.pdf", "AT+CAOPEN"
    switch(res) {
      case 0: return "Success";
      case 1: return "Socket error";
      case 2: return "No memory";
      case 3: return "Connection limit";
      case 4: return "Parameter invalid";
      case 6: return "Invalid IP address";
      case 7: return "Not support the function";
      case 8: return "Session types do not match";
      case 9: return "The session has been closed but not released";
      case 10: return "Illegal operation";
      case 11: return "Unable to close socket";
      case 12: return "Can’t bind the port";
      case 13: return "Can’t listen the port";
      case 18: return "Connect failed";
      case 20: return "Can’t resolve the host";
      case 21: return "Network not active";
      case 23: return "Remote refuse";
      case 24: return "Certificate’s time expired";
      case 25: return "Certificate’s common name does not match";
      case 26: return "Certificate’s common name does not match and time expired";
      case 27: return "Connect failed";
      default: return "Unknown error";
    }
  } // const char* getCaopenResultText(...)
)
// <MS>

  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 75) {
    DBGLOG(Info, "[TinyGsmSim7080] (mux: %hhu) >> host: '%s', port: %hu, ssl: %s, timeout: %is", 
      mux, host == nullptr ? "-" : host, port, DBGB2S(ssl), timeout_s)

    MS_TINY_GSM_SEM_TAKE_WAIT("modemConnect")
    
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;
    bool ret = false;
    int8_t res = -1;

    // set the connection (mux) identifier to use
    sendAT(GF("+CACID="), mux);
    if (waitResponse(timeout_ms) != 1) { 
      DBGLOG(Warn, "[TinyGsmSim7080] (mux: %hhu)Set the connection (mux) identifier to use failed.", mux)
      goto end; 
    }


    if (ssl) {
      // set the ssl version
      // AT+CSSLCFG="SSLVERSION",<ctxindex>,<sslversion>
      // <ctxindex> PDP context identifier - for reasons not understood by me,
      //            use PDP context identifier of 0 for what we defined as 1 in
      //            the gprsConnect function
      // <sslversion> 0: QAPI_NET_SSL_PROTOCOL_UNKNOWN
      //              1: QAPI_NET_SSL_PROTOCOL_TLS_1_0
      //              2: QAPI_NET_SSL_PROTOCOL_TLS_1_1
      //              3: QAPI_NET_SSL_PROTOCOL_TLS_1_2
      //              4: QAPI_NET_SSL_PROTOCOL_DTLS_1_0
      //              5: QAPI_NET_SSL_PROTOCOL_DTLS_1_2
      // NOTE:  despite docs using caps, "sslversion" must be in lower case
// <MS>
      sendAT(GF("+CSSLCFG=\"sslversion\",0,3"));  // TLS 1.2
//      sendAT(GF("+CSSLCFG=\"sslversion\","), mux, GF(",3"));  // TLS 1.2
// <MS>      
      if (waitResponse(5000L) != 1)  { 
        DBGLOG(Error, "[TinyGsmSim7080] (mux: %hhu) Set the ssl version failed.", mux);
        goto end; 
      }
    }

    // enable or disable ssl
    // AT+CASSLCFG=<cid>,"SSL",<sslFlag>
    // <cid> Application connection ID (set with AT+CACID above)
    // <sslFlag> 0: Not support SSL
    //           1: Support SSL
    sendAT(GF("+CASSLCFG="), mux, ',', GF("SSL,"), ssl);
    waitResponse();

    if (ssl) {
// <MS> NEW
//      sendAT(GF("+CASSLCFG="), mux, ',', GF("crindex,"), mux);
//      waitResponse();
// <MS>

      // set the PDP context to apply SSL to
      // AT+CSSLCFG="CTXINDEX",<ctxindex>
      // <ctxindex> PDP context identifier - for reasons not understood by me,
      //            use PDP context identifier of 0 for what we defined as 1 in
      //            the gprsConnect function
      // NOTE:  despite docs using "CRINDEX" in all caps, the module only
      // accepts the command "ctxindex" and it must be in lower case
// <MS>      
      sendAT(GF("+CSSLCFG=\"ctxindex\",0"));
//      sendAT(GF("+CSSLCFG=\"ctxindex\","), mux);
// <MS>      
      if (waitResponse(5000L, GF("+CSSLCFG:")) != 1)  { 
        DBGLOG(Error, "[TinyGsmSim7080] (mux: %hhu) +CSSLCFG=\'ctxindex\' failed.", mux);
        goto end; 
      }
      streamSkipUntil('\n');  // read out the certificate information
      waitResponse();

      if (certificates[mux] != "") {
        // <MS> Looks like that it is not possible to upload the certificate itself,
        // but only link the cert-name which was "uploaded" in an other way before!
        // Resume: IT DOES NOT WORK!

        // apply the correct certificate to the connection
        // AT+CASSLCFG=<cid>,"CACERT",<caname>
        // <cid> Application connection ID (set with AT+CACID above)
        // <certname> certificate name
        DBGLOG(Info, "[TinyGsmSim7080] (mux: %hhu) set certificate ...", mux);
        sendAT(GF("+CASSLCFG="), mux, ",CACERT,\"", certificates[mux].c_str(),
               "\"");
        if (waitResponse(5000L) != 1)  { 
          DBGLOG(Warn, "[TinyGsmSim7080] (mux: %hhu) Set certificate failed.");
          goto end; 
        }
      }

      // set the SSL SNI (server name indication)
      // AT+CSSLCFG="SNI",<ctxindex>,<servername>
      // <ctxindex> PDP context identifier - for reasons not understood by me,
      //            use PDP context identifier of 0 for what we defined as 1 in
      //            the gprsConnect function
      // NOTE:  despite docs using caps, "sni" must be in lower case
      DBGLOG(Info, "[TinyGsmSim7080] (mux: %hhu)  +CSSLCFG=/sni/ ...", mux)
//      sendAT(GF("+CSSLCFG=\"sni\","), mux, ',', GF("\""), host, GF("\""));
      sendAT(GF("+CSSLCFG=\"sni\",0,"), GF("\""), host, GF("\""));
      waitResponse();
    }

    // actually open the connection
    // AT+CAOPEN=<cid>,<pdp_index>,<conn_type>,<server>,<port>[,<recv_mode>]
    // <cid> TCP/UDP identifier
    // <pdp_index> Index of PDP connection; we set up PCP context 1 above
    // <conn_type> "TCP" or "UDP"
    // <recv_mode> 0: The received data can only be read manually using
    // AT+CARECV=<cid>
    //             1: After receiving the data, it will automatically report
    //             URC:
    //                +CAURC:
    //                "recv",<id>,<length>,<remoteIP>,<remote_port><CR><LF><data>
    // NOTE:  including the <recv_mode> fails
    sendAT(GF("+CAOPEN="), mux, GF(",0,\"TCP\",\""), host, GF("\","), port);
    if (waitResponse(timeout_ms, GF(AT_NL "+CAOPEN:")) != 1) { goto end; }
    // returns OK/r/n/r/n+CAOPEN: <cid>,<result>
    // <result> 0: Success
    //          1: Socket error
    //          2: No memory
    //          3: Connection limit
    //          4: Parameter invalid
    //          6: Invalid IP address
    //          7: Not support the function
    //          12: Can’t bind the port
    //          13: Can’t listen the port
    //          20: Can’t resolve the host
    //          21: Network not active
    //          23: Remote refuse
    //          24: Certificate’s time expired
    //          25: Certificate’s common name does not match
    //          26: Certificate’s common name does not match and time expired
    //          27: Connect failed
    streamSkipUntil(',');  // Skip mux

    // make sure the connection really opened
    res = streamGetIntBefore('\n');
    waitResponse();

    ret = (0 == res);

    DBGCHK(Error, ret, "[TinyGsmSim7080] (mux: %hhu) Result of +CAOPEN: %hhi-%s", mux, DBGB2S(ret), res, getCaopenResultText(res))

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Info, "[TinyGsmSim7080] (mux: %hhu) << return: %s, result of +CAOPEN: %hhi", mux, DBGB2S(ret), res)

    return ret;
  } // ::modemConnect(...)

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %hhu", mux)

    MS_TINY_GSM_SEM_TAKE_WAIT("modemSend")

    size_t _len = len;

    // send data on prompt
    sendAT(GF("+CASEND="), mux, ',', (uint16_t)len);
    if (waitResponse(GF(">")) != 1) { _len = 0; goto end; }

    stream.write(reinterpret_cast<const uint8_t*>(buff), len);
    _len = len;
    stream.flush();

    // OK after posting data
    if (waitResponse() != 1) { _len = 0; goto end; }

  end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %u", mux, _len);
    return _len;
  } // ::modemSend(...)

  size_t modemRead(size_t size, uint8_t mux) {
    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) >> size: %u", mux, size);
    DBGCHK(Error, sockets[mux] != nullptr, "[TinyGsmSim7080] (#%hhu) socket #%hhu does not exist!", mux, mux)
    if (!sockets[mux]) { return 0; }
    DBGCOD(char* tmp = new char[TINY_GSM_RX_BUFFER]; tmp[0] = '\0';)

    MS_TINY_GSM_SEM_TAKE_WAIT("modemRead")

    int16_t len_confirmed;
    size_t _size = size;
    int i;
    uint32_t startMillis;

    sendAT(GF("+CARECV="), mux, ',', (uint16_t)size);

    if (waitResponse(GF("+CARECV:")) != 1) { _size = 0; goto end; }

    // uint8_t ret_mux = stream.parseInt();
    // streamSkipUntil(',');
    // const int16_t len_confirmed = streamGetIntBefore('\n');
    // DBG("### READING:", len_confirmed, "from", ret_mux);

    // if (ret_mux != mux) {
    //   DBG("### Data from wrong mux! Got", ret_mux, "expected", mux);
    //   waitResponse();
    //   sockets[mux]->sock_available = modemGetAvailable(mux);
    //   return 0;
    // }

    // NOTE:  manual says the mux number is returned before the number of
    // characters available, but in tests only the number is returned

    len_confirmed = stream.parseInt();
    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) len_confirmed: %hi", mux, len_confirmed)
    streamSkipUntil(',');  // skip the comma
    if (len_confirmed <= 0) {
      waitResponse();
      sockets[mux]->sock_available = modemGetAvailable(mux);
      _size = 0;
      goto end;
    }

    for (i = 0; i < len_confirmed; i++) {
      startMillis = millis();
      while (!stream.available() &&
             (millis() - startMillis < sockets[mux]->_timeout)) {
        TINY_GSM_YIELD();
      }
      char c = stream.read();
      DBGCOD(tmp[i] = c;)
      sockets[mux]->rx.put(c);
    } // for i
    DBGCHK(Error, i == len_confirmed, "[TinyGsmSim7080] i(%i) != len_confirmed(%" PRIi16 "), i.e. time-out.", i, len_confirmed)
    DBGCOD(tmp[i] = '\0';)
    DBGLOG(Debug,"[TinyGsmSim7080] (#%hhu) Read: \n%s\n", mux, tmp)
    waitResponse();
    // make sure the sock available number is accurate again
    sockets[mux]->sock_available = modemGetAvailable(mux);

    _size = len_confirmed;

  end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGCOD(delete[] tmp;)
    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %u,  sock_available: %" PRIu16, mux, _size, sockets[mux]->sock_available);
    return _size;
  } // ::modemRead(...)

  size_t modemGetAvailable(uint8_t mux) {
    // If the socket doesn't exist, just return
    size_t result_sum = 0;
    DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) >>", mux);
    DBGCHK(Warn, MS_TINY_GSM_SEM_BLOCKED, "[TinyGsmSim7080] Not blocked by calling function")
    DBGCHK(Error, (mux == (uint8_t)-1) || (sockets[mux] != nullptr), "[TinyGsmSim7080] (mux: %hhu) socket (mux) does not exist!", mux)
    if (!((mux == (uint8_t)-1) || (sockets[mux] != nullptr))) { goto end; }

    // NOTE: This gets how many characters are available on all connections that
    // have data.  It does not return all the connections, just those with data.
    sendAT(GF("+CARECV?"));
    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      // after the last connection, there's an ok, so we catch it right away
      int res = waitResponse(3000, GF("+CARECV:"), GFP(GSM_OK), GFP(GSM_ERROR));
      // if we get the +CARECV: response, read the mux number and the number of
      // characters available
      if (res == 1) {
        int               ret_mux = streamGetIntBefore(',');
        size_t            result  = streamGetIntBefore('\n');

        result_sum += result;

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) muxNo: %i, res=1: ret_mux: %i, available: %u", mux, muxNo, ret_mux, result);

        GsmClientSim7080* sock    = sockets[ret_mux];
        if (sock) { sock->sock_available = result; }
        // if the first returned mux isn't 0 (or is higher than expected)
        // we need to fill in the missing muxes
        if (ret_mux > muxNo) {
          for (int extra_mux = muxNo; extra_mux < ret_mux; extra_mux++) {
            GsmClientSim7080* isock = sockets[extra_mux];
            if (isock) { isock->sock_available = 0; }
          }
          muxNo = ret_mux;
        }
      } else if (res == 2) {
        // if we get an OK, we've reached the last socket with available data
        // so we set any we haven't gotten to yet to 0

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) muxNo: %i, res=2.", mux, muxNo);

        for (int extra_mux = muxNo; extra_mux < TINY_GSM_MUX_COUNT;
             extra_mux++) {
          GsmClientSim7080* isock = sockets[extra_mux];
          if (isock) { isock->sock_available = 0; }
        }
        break;
      } else {
        // if we got an error, give up
        DBGLOG(Warn, "[TinyGsmSim7080] (mux: %hhu) ERROR.", mux);
        break;
      }
      // Should be a final OK at the end.
      // If every connection was returned, catch the OK here.
      // If only a portion were returned, catch it above.
      if (muxNo == TINY_GSM_MUX_COUNT - 1) { waitResponse(); }
    } // for muxNo

    modemGetConnected(mux);  // check the state of all connections
    if (mux == (uint8_t)-1) {
      // No specific mux given, all active muxs updated, no specific return-value expected by caller.
      // If there was at least one mux with available return > 0.
      goto end;
    }
    if (!sockets[mux]) { 
      result_sum = 0; 
      goto end;
    }
    result_sum = sockets[mux]->sock_available;

end:
    DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) << return: %u", mux, result_sum);
    return result_sum;
  } // ::modemGetAvailable(...)


  bool modemGetConnected(uint8_t mux) {
    // NOTE:  This gets the state of all connections that have been opened
    // since the last connection
    DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) >>", mux)
    DBGCHK(Warn, MS_TINY_GSM_SEM_BLOCKED, "[TinyGsmSim7080] (mux: %hhu) Not blocked by calling function", mux)

    sendAT(GF("+CASTATE?"));

// <MS>
    bool connected_sum = false;
// <MS>        

    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      // after the last connection, there's an ok, so we catch it right away
      int res = waitResponse(3000, GF("+CASTATE:"), GFP(GSM_OK),
                             GFP(GSM_ERROR));
      // if we get the +CASTATE: response, read the mux number and the status
      if (res == 1) {
        int    ret_mux = streamGetIntBefore(',');
        size_t status  = streamGetIntBefore('\n');

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) muxNo: %i, res=ok: ret_mux: %i, status: %u-%s", 
          mux, muxNo, ret_mux, status,
          (status == 0) ? "Closed by remote server or internal error" :
          (status == 1) ? "Connected to remote server" :
          (status == 2) ? "Listening (server mode)" : "Unknown status"
          )

        // 0: Closed by remote server or internal error
        // 1: Connected to remote server
        // 2: Listening (server mode)
        GsmClientSim7080* sock = sockets[ret_mux];
        if (sock) { 
          sock->sock_connected = (status == 1); 
// <MS>
          connected_sum = connected_sum || sock->sock_connected;
// <MS>          
        }
        // if the first returned mux isn't 0 (or is higher than expected)
        // we need to fill in the missing muxes
        if (ret_mux > muxNo) {
          for (int extra_mux = muxNo; extra_mux < ret_mux; extra_mux++) {
            GsmClientSim7080* isock = sockets[extra_mux];
            if (isock) { isock->sock_connected = false; }
          }
          muxNo = ret_mux;
        }
      } else if (res == 2) {
        // if we get an OK, we've reached the last socket with available data
        // so we set any we haven't gotten to yet to 0

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) : muxNo: %i, res=error.", mux, muxNo)

        for (int extra_mux = muxNo; extra_mux < TINY_GSM_MUX_COUNT;
             extra_mux++) {
          GsmClientSim7080* isock = sockets[extra_mux];
          if (isock) { isock->sock_connected = false; }
        }
        break;
      } else {
        // if we got an error, give up
        DBGLOG(Warn, "[TinyGsmSim7080] (mux: %hhu) waitResponse() returned error: %i", mux, res)
        break;
      }
      // Should be a final OK at the end.
      // If every connection was returned, catch the OK here.
      // If only a portion were returned, catch it above.
      if (muxNo == TINY_GSM_MUX_COUNT - 1) { waitResponse(); }
    }

    if (mux == (uint8_t)-1) {
      // No specific mux given, all active muxs updated, no specific return-value expected by caller.
      goto end;
    }
    if (!sockets[mux]) { 
      connected_sum = false; 
      goto end;
    }
    connected_sum = sockets[mux]->sock_connected;

end:
    DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) << return: %s", mux, DBGB2S(connected_sum))
    return connected_sum;
  } // ::modemGetConnected(...)

  /*
   * Utilities
   */
 public:
  bool handleURCs(String& data) {
    if (data.endsWith(GF("+CARECV:"))) {
      int8_t  mux = streamGetIntBefore(',');
      int16_t len = streamGetIntBefore('\n');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
        sockets[mux]->got_data = true;
        DBGCHK(Error, (len >= 0) && (len <=1024), "[TinyGsmSim7080] len (%hi) out of range [0..1024]!", len)
        if (len >= 0 && len <= 1024) { sockets[mux]->sock_available = len; }
      }
      data = "";
      DBGLOG(Debug, "{TinyGsmSim7080} Got Data on mux: %hhi, len: %hi", mux, len)
      return true;
    } else if (data.endsWith(GF("+CADATAIND:"))) {
      int8_t mux = streamGetIntBefore('\n');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
        sockets[mux]->got_data = true;
      }
      data = "";
      DBGLOG(Debug, "{TinyGsmSim7080} Got Data on mux: %hhi.", mux)
      return true;
    } else if (data.endsWith(GF("+CASTATE:"))) {
      int8_t mux   = streamGetIntBefore(',');
      int8_t state = streamGetIntBefore('\n');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
        if (state != 1) {
          sockets[mux]->sock_connected = false;
          DBGLOG(Info, "{TinyGsmSim7080} Closed mux: %hhi", mux)
        }
      }
      data = "";
      return true;
    } else if (data.endsWith(GF("*PSNWID:"))) {
      streamSkipUntil('\n');  // Refresh network name by network
      data = "";
      DBGLOG(Info, "{TinyGsmSim7080} Network name updated.")
      return true;
    } else if (data.endsWith(GF("*PSUTTZ:"))) {
      char dest[32];
      streamGetCharBefore('\n', dest, sizeof(dest));  // Refresh time zone by network
//      streamSkipUntil('\n');  // Refresh time and time zone by network
      data = "";
      DBGLOG(Info, "{TinyGsmSim7080} Network time and time zone updated, *PSUTTZ: %s", dest)
      return true;
    } else if (data.endsWith(GF("+CTZV:"))) {
      char dest[32];
      streamGetCharBefore('\n', dest, sizeof(dest));  // Refresh time zone by network
//      streamSkipUntil('\n');  // Refresh network time zone by network
      data = "";
      DBGLOG(Info, "{TinyGsmSim7080} Network time zone updated, +CTZV: %s", dest)
      return true;
    } else if (data.endsWith(GF("DST: "))) {
      int dst = streamGetIntBefore('\n');  // Refresh time zone by network
      data = "";
      DBGCHK(Error, (dst == 0) || (dst == 1), "{TinyGsmSim7080} Daylight savings time state updated, DST out of range: %i", dst)
      DBGCHK(Info, !((dst == 0) || (dst == 1)), "{TinyGsmSim7080} Daylight savings time state updated, DST: %i", dst)
      if ((dst == 0) || (dst == 1)) { 
        dayLightSaving = dst;
      }
      return true;
    } else if (data.endsWith(GF(AT_NL "SMS Ready" AT_NL))) {
      data = "";
      DBGLOG(Error, "{TinyGsmSim7080} Unexpected module reset!")
//      init();
      data = "";
      return true;
    } 
// <MS>
/*
    else if (data.indexOf(GF("VOLTAGE")) >= 0) {
      DBGLOG(Error, "[TinyGsmSim7080] Voltage-message: %s.", asCharString(data.c_str(), 0, data.length()).c_str())
      streamSkipUntil('\n'); 
      data = "";
    } else {
      String tmp = data;
      tmp.replace(AT_NL, "/");
      tmp.trim();
      if ((tmp.length() > 10) && 
          (tmp.indexOf(GF("CNACT")) < 0) && 
          (tmp.indexOf(GF("SLEDS")) < 0) && 
          (tmp.indexOf(GF("CNET")) < 0) && 
          (tmp.indexOf(GF("CSGS")) < 0) && 
          (tmp.indexOf(GF("CGNAP")) < 0) && 
          (tmp.indexOf(GF("CLTS")) < 0) )
        {
        DBGLOG(Warn, "[TinyGsmSim7080] Unknown message: '%s' (%s)", 
          asCharString(tmp.c_str(), 0, tmp.length()).c_str(), asHexString(tmp.c_str(), 0, tmp.length()).c_str())
      }
    }
*/
// <MS>

    return false;
  } // ::handleURCs(...)

 protected:
  GsmClientSim7080* sockets[TINY_GSM_MUX_COUNT];
  String            certificates[TINY_GSM_MUX_COUNT];
}; // class TinyGsmSim7080

#endif  // SRC_TINYGSMCLIENTSIM7080_H_
