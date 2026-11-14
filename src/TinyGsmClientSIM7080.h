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
#if defined(MS_TINYGSM_LOGGING) && defined(MS_LOGGER_ON)
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
    const char* TAG = "[GsmClientSim7080] ";

    GsmClientSim7080() = delete;

    explicit GsmClientSim7080(TinyGsmSim7080& _modem, uint8_t _mux = 0) {
      init(&_modem, _mux);
    }

    ~GsmClientSim7080() {
      DBGLOG(Info, "%s>> mux: %hhu", TAG, mux)
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
      // A failed attachment must never unregister another client's socket.
      if (isAttached()) {
        MS_TINY_GSM_SEM_TAKE_WAIT
        if (isAttached()) { at->sockets[mux] = nullptr; }
        MS_TINY_GSM_SEM_GIVE_WAIT
      }
      DBGLOG(Info, "%s<<", TAG)
    }

    bool init(TinyGsmSim7080* _modem, uint8_t _mux = 0) {
      DBGLOG(Info, "%s>> _mux: %hhu", TAG, _mux)
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
      // Stop before taking the AT lock, because stop/read lock internally.
      if (isAttached()) { stop(); }
      if (!isAttached() && (_modem == nullptr || _mux >= TINY_GSM_MUX_COUNT)) {
        at = nullptr;
        mux = 0xFF;
        resetSocketState();
        return false;
      }

      MS_TINY_GSM_SEM_TAKE_WAIT

      if (isAttached()) { at->sockets[mux] = nullptr; }
      at = nullptr;
      mux = 0xFF;
      resetSocketState();

      const bool r = _modem != nullptr && _mux < TINY_GSM_MUX_COUNT &&
                     _modem->sockets[_mux] == nullptr;
      if (r) {
        at = _modem;
        mux = _mux;
        at->sockets[mux] = this;
      } else {
        DBGLOG(Warn, "%sInvalid or occupied mux: %hhu", TAG, _mux)
      }

      MS_TINY_GSM_SEM_GIVE_WAIT
      DBGLOG(Info, "%s<< return: %s", TAG, DBGB2S(r))
      return r;
    } // bool GsmClientSim7080::init(...)

   public:
    // virtual 
    int connect(const char* host, uint16_t port, int timeout_s) {
      DBGLOG(Info, "%s>> (mux: %hhu)", TAG, mux)
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
      if (!isAttached() || host == nullptr || timeout_s < 0) { return 0; }
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      sock_stopped = false;
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      if (!sock_connected) { resetSocketState(); }
      DBGLOG(Info, "%s<< (mux: %hhu) return sock_connected: %s", TAG, mux, DBGB2S(sock_connected))
      return sock_connected;
    } // int GsmClientSim7080::connect(...)
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
/*
    // Added due to new C++-compiler-standard with ESP-IDF 5.3.1.
    int connect(IPAddress ip, uint16_t port, int32_t timeout) override {
      return connect(ip, port, (int)timeout);
    } 
    int connect(const char *host, uint16_t port, int32_t timeout) override {
      return connect(host, port, (int)timeout);
    }
*/
    void stop(uint32_t maxWaitMs) {
      DBGLOG(Info, "%s>> (mux: %hhu)", TAG, mux)

      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
      if (!isAttached()) { return; }

      dumpModemBuffer(maxWaitMs);

      MS_TINY_GSM_SEM_TAKE_WAIT

      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      // Clear local state even if draining timed out or modem close fails.
      resetSocketState();
      at->sendAT(GF("+CACLOSE="), mux);
      if (at->waitResponse(3000) != 1) {
        DBGLOG(Warn, "%sClose not confirmed for mux %hhu; retrying", TAG, mux)
        at->sendAT(GF("+CACLOSE="), mux);
        if (at->waitResponse(3000) != 1) {
          DBGLOG(Error, "%sCould not confirm close for mux %hhu", TAG, mux)
        }
      }
      resetSocketState();

      MS_TINY_GSM_SEM_GIVE_WAIT

      DBGLOG(Info, "%s<< (mux: %hhu)", TAG, mux)
    } // GsmClientSim7080::stop(...)
    
    void stop() override {
      stop(15000L);
    } // GsmClientSim7080::stop()

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;

   protected:
    // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
    void resetSocketState() {
      rx.clear();
      sock_available = 0;
      sock_connected = false;
      got_data = false;
      prev_check = millis();
      sock_stopped = true;
    }
  }; // class class GsmClientSim7080



  /*
   * Inner Secure Client
   */
 public:
  class GsmClientSecureSIM7080 : public GsmClientSim7080 {
   public:
    GsmClientSecureSIM7080() = delete;

    explicit GsmClientSecureSIM7080(TinyGsmSim7080& _modem, uint8_t _mux = 0)
        : GsmClientSim7080(_modem, _mux) {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> _mux: %hhu", _mux)
      DBGLOG(Info, "[GsmClientSecureSIM7080] <<")
    }

    bool setCertificate(const String& certificateName) {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> certificateName: '%s'", certificateName.c_str())
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
      if (!isAttached()) { return false; }
      bool r = at->setCertificate(certificateName, mux);
      DBGLOG(Info, "[GsmClientSecureSIM7080] << return: %s", DBGB2S(r))
      return r;
    }

    int connect(const char* host, uint16_t port, int timeout_s) 
    //override 
    {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> host: '%s', port: %hu", host == NULL ? "-" : host, port)
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
      if (!isAttached() || host == nullptr || timeout_s < 0) { return 0; }
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      sock_stopped = false;
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      if (!sock_connected) { resetSocketState(); }
      DBGLOG(Info, "[GsmClientSecureSIM7080] << return sock_connected: %s", DBGB2S(sock_connected))
      return sock_connected;
    } // GsmClientSecureSIM7080::connect(...)
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
  }; // GsmClientSecureSIM7080

 // <MS>
 public:
  // Set by handleURCs(...) when receiving "DST" from the network.
  // -1: not set, 0: standard time, 1: daylight saving time
  // Sometimes not set by the network, therefore stored in preferences by MsGsm as default for next time re-start.
  // At the moment not used (dst is detected by msSetTime() using the actual date)
  int dayLightSaving = -1;

  /*
   * Constructor
   */
 public:
  explicit TinyGsmSim7080(Stream& _stream)
      : TinyGsmSim70xx<TinyGsmSim7080>(_stream) {
    DBGLOG(Info, "[TinyGsmSim7080] >>");
    memset(sockets, 0, sizeof(sockets));
    msTinyGsmSemProcess = xSemaphoreCreateMutex();
    DBGCHK(Error, msTinyGsmSemProcess != NULL, "[TinyGsmSim7080] CRITICAL ERROR msTinyGsmSemProcess can not be created!");
    DBGLOG(Info, "[TinyGsmSim7080] <<");
  } // TinyGsmSim7080::TinyGsmSim7080(...)

  ~TinyGsmSim7080() {
    DBGLOG(Info, "[TinyGsmSim7080] >>");
    // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1a
    // The modem registers externally owned clients; detach them without deleting.
    MS_TINY_GSM_SEM_TAKE_WAIT
    for (uint8_t mux = 0; mux < TINY_GSM_MUX_COUNT; mux++) {
      GsmClientSim7080* sock = sockets[mux];
      if (sock) {
        sock->resetSocketState();
        sock->at = nullptr;
        sock->mux = 0xFF;
        sockets[mux] = NULL;
      }
    }
    MS_TINY_GSM_SEM_GIVE_WAIT
    if (msTinyGsmSemProcess) {
      vSemaphoreDelete(msTinyGsmSemProcess);
      msTinyGsmSemProcess = NULL;
    }
    DBGLOG(Info, "[TinyGsmSim7080] <<");
  } // TinyGsmSim7080::~TinyGsmSim7080()

  // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
  // Explicit setup/restart may retry a stream quarantined after a bad frame.
  bool recoverATStream() {
    MS_TINY_GSM_SEM_TAKE_WAIT
    const bool recovered = recoverATStreamImpl();
    MS_TINY_GSM_SEM_GIVE_WAIT
    return recovered;
  }

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

    MS_TINY_GSM_SEM_TAKE_WAIT

    initRunning = true;

    // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
    if (!recoverATStreamImpl()) { goto end; }

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
    DBGLOG(Debug, "[TinyGsmSim7080::maintainImpl] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT
    // xSemaphoreTake(msTinyGsmSemProcess, portMAX_DELAY);

    // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
    if (!atStreamSynchronized_) {
      MS_TINY_GSM_SEM_GIVE_WAIT
      return;
    }

    bool check_socks = false;
    for (uint8_t mux = 0; mux < TINY_GSM_MUX_COUNT; mux++) {
      GsmClientSim7080* sock = sockets[mux];
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      if (sock && !sock->sock_stopped && sock->got_data) {
        sock->got_data = false;
        check_socks    = true;
      }
    }
    // modemGetAvailable checks all socks, so we only want to do it once
    // modemGetAvailable calls modemGetConnected(), which also checks allf

    // Check all mux in use.
    if (check_socks) { modemGetAvailable((uint8_t)-1); }

    while (stream.available()) { waitResponse(15, nullptr, nullptr); }

    MS_TINY_GSM_SEM_GIVE_WAIT
    // xSemaphoreGive(msTinyGsmSemProcess); 

    DBGLOG(Debug, "[TinyGsmSim7080::maintainImpl] <<");
  } // TinyGsmSim7080::maintainImpl()

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = nullptr) {
    DBGLOG(Info, "[TinyGsmSim7080] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT

    bool success = true;
    int8_t resp;
    String s;

    bool gotATOK = false;
    // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
    if (!recoverATStreamImpl()) { success = false; goto end; }
    for (uint32_t start = millis(); millis() - start < 10000L;) {
      sendAT(GF(""));
      resp = waitResponse(200L, s, GFP(GSM_OK), GFP(GSM_ERROR), GF("AT"));
      DBGLOG(Info, "[TinyGsmSim7080] Empty AT returned: %hhi, %s", resp, s.c_str());
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
      success = false;
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

    MS_TINY_GSM_SEM_TAKE_WAIT

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
    MS_TINY_GSM_SEM_TAKE_WAIT

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
      res = (waitResponse(60000L, 
                          GF(AT_NL "+APP PDP: 0,ACTIVE"),
                          GF(AT_NL "+APP PDP: 0,DEACTIVE")) == 1);
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

    MS_TINY_GSM_SEM_TAKE_WAIT

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
    byte r = 0xFF;

    MS_TINY_GSM_SEM_TAKE_WAIT

    // Set GPRS bearer profile to associate with NTP sync
    // this may fail, it's not supported by all modules
    sendAT(GF("+CNTPCID=0"));  // CID must be 0. With 1 (like other modules)
                               // does not work!
    waitResponse(10000L);

    // Set NTP server and timezone
    sendAT(GF("+CNTP=\""), server, "\",", String(TimeZone));
    if (waitResponse(10000L) != 1) { 
      DBGLOG(Warn, "[TinyGsmSim7080] No response or not 'OK'.")
      goto end; 
    }

    // Request network synchronization
    sendAT(GF("+CNTP"));
    if (waitResponse(10000L, GF("+CNTP:"))) {
      String result = stream.readStringUntil('\n');
      DBGLOG(Info, "[TinyGsmSim7080] AT+CNTP response: %s", result.c_str());
      // Check for ',' in case the module appends the time next to the return
      // code. Eg: +CNTP: <code>[,<time>]
      int index = result.indexOf(',');
      if (index > 0) { result.remove(static_cast<unsigned int>(index)); }
      result.trim();
      if (TinyGsmIsValidNumber(result)) { 
        long rx = result.toInt(); 
        DBGCHX(Error, rx <= 0xFF, "rx > 0xFF: %ld", "rx: %ld", rx)
        r = static_cast<byte>(rx);
        goto end; 
      }
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

    MS_TINY_GSM_SEM_TAKE_WAIT
    
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
    res = streamGetInt8Before('\n');
    waitResponse();

    ret = (0 == res);

    DBGCHK(Error, ret, "[TinyGsmSim7080] (mux: %hhu) Result of +CAOPEN: %hhi-%s", mux, res, getCaopenResultText(res))

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Info, "[TinyGsmSim7080] (mux: %hhu) << return: %s, result of +CAOPEN: %hhi", mux, DBGB2S(ret), res)

    return ret;
  } // ::modemConnect(...)

  size_t modemSend(const void* buff, size_t len, uint8_t mux) {
    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %hhu, len: %u", mux, len)
    // SIM7070/7080/7090 AT+CASEND accepts at most 1460 bytes per command.
    size_t sent = 0;
    constexpr size_t maxSendSize = 1460;
    constexpr uint32_t inputTimeoutMs = 5000L;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buff);
    if (len == 0 || buff == nullptr) { goto end2; }

    MS_TINY_GSM_SEM_TAKE_WAIT

    while (sent < len) {
      const size_t chunkSize = TinyGsmMin(len - sent, maxSendSize);
      sendAT(GF("+CASEND="), mux, ',', static_cast<uint16_t>(chunkSize),
             ',', inputTimeoutMs);
      if (waitResponse(GF(">")) != 1) { break; }

      // Complete short UART writes within the already announced chunk.
      size_t written = 0;
      while (written < chunkSize) {
        const size_t n = stream.write(data + sent + written, chunkSize - written);
        if (n == 0) { break; }
        written += n;
      }
      stream.flush();

      if (written != chunkSize) {
        DBGLOG(Error, "[TinyGsmSim7080] (#%hhu) incomplete UART write: %u of %u bytes", mux, written, chunkSize)
        // Let the modem leave data-input mode before releasing the AT stream.
        waitResponse(inputTimeoutMs + 1000L);
        break;
      }

      // Count only complete chunks acknowledged by the modem.
      if (waitResponse() != 1) { break; }
      sent += chunkSize;
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

  end2:
    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %u", mux, sent);
    return sent;
  } // ::modemSend(...)

  // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
  // Caller owns the TinyGSM semaphore. Discard bytes without parsing binary
  // payload as URCs or command replies; both continuous input and silence end.
  bool discardStreamUntilQuiet(uint32_t maxWaitMs, uint32_t quietMs) {
    const uint32_t started = millis();
    uint32_t lastByte = started;
    // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
    // delay(0) alone cannot let the ESP32 idle task service its watchdog.
    uint32_t lastIdleYield = started;
    while (millis() - started < maxWaitMs) {
      if (stream.available() > 0) {
        stream.read();
        lastByte = millis();
      } else if (millis() - lastByte >= quietMs) {
        return true;
      }
      if (millis() - lastIdleYield >= 100L) {
        delay(10);
        lastIdleYield = millis();
      } else {
        TINY_GSM_YIELD();
      }
    }
    return false;
  }

  // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
  // Called with the AT semaphore held, only by explicit setup/init/restart.
  bool recoverATStreamImpl() {
    if (atStreamSynchronized_) { return true; }
    if (!discardStreamUntilQuiet(6000L, 3000L)) { return false; }
    atStreamSynchronized_ = true;
    sendAT(GF(""));
    if (waitResponse() == 1) { return true; }
    atStreamSynchronized_ = false;
    return false;
  }

  // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
  size_t modemRead(size_t size, uint8_t mux) {
    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) >> size: %u", mux, size)
    if (size == 0 || mux >= TINY_GSM_MUX_COUNT) { return 0; }

    MS_TINY_GSM_SEM_TAKE_WAIT

    size_t buffered = 0;
    GsmClientSim7080* const sock = sockets[mux];
    if (sock && sock->isAttached() && !sock->sock_stopped &&
        isATStreamSynchronized()) {
      // SIMCom CARECV accepts 1..1460 bytes. The firmware used here returns
      // '<recvlen>,<binary data>' with CASRIP disabled, not a MUX field.
      constexpr size_t maxReadSize = 1460;
      constexpr uint32_t headerTimeoutMs = 1000L;
      constexpr uint32_t payloadTimeoutMs = 5000L;
      const size_t requested = TinyGsmMin(
          TinyGsmMin(size, static_cast<size_t>(sock->rx.free())), maxReadSize);

      if (requested > 0) {
        sendAT(GF("+CARECV="), mux, ',', static_cast<uint16_t>(requested));
        bool complete = waitResponse(GF("+CARECV:")) == 1;
        size_t confirmed = 0;
        unsigned digits = 0;
        bool delimiter = false;
        const uint32_t headerStarted = millis();

        // Read the complete unsigned length token; parseInt() would skip
        // malformed signs/text and can accept a token truncated by timeout.
        while (complete && millis() - headerStarted < headerTimeoutMs) {
          const int c = stream.read();
          if (c < 0) {
            // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
            delay(10);
            continue;
          }
          if (digits == 0 && (c == ' ' || c == '\t')) { continue; }
          if (c == ',' && digits > 0) {
            delimiter = true;
            break;
          }
          if (c < '0' || c > '9' || digits == 4) {
            complete = false;
            break;
          }
          confirmed = confirmed * 10 + static_cast<unsigned>(c - '0');
          ++digits;
          if (confirmed > requested) {
            complete = false;
            break;
          }
        }
        complete = complete && delimiter;

        const uint32_t payloadStarted = millis();
        uint32_t lastByte = payloadStarted;
        while (complete && buffered < confirmed) {
          if (millis() - payloadStarted >= payloadTimeoutMs) {
            complete = false;
            break;
          }
          const int c = stream.read();
          if (c < 0) {
            if (millis() - lastByte >= sock->_timeout) {
              complete = false;
              break;
            }
            // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
            delay(10);
            continue;
          }
          lastByte = millis();
          if (c > 0xFF || !sock->rx.put(static_cast<uint8_t>(c))) {
            complete = false;
            break;
          }
          ++buffered;
        }

        // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
        // Check the documented frame boundary before AT parsing. Otherwise a
        // short payload could consume CR/LF and leave a misleading bare OK.
        if (complete) {
          const uint32_t framingStarted = millis();
          const auto readFramingByte = [&]() -> int {
            while (millis() - framingStarted < headerTimeoutMs) {
              const int c = stream.read();
              if (c >= 0) { return c; }
              // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
              delay(10);
            }
            return -1;
          };
          complete = readFramingByte() == '\r' && readFramingByte() == '\n';
          if (complete) {
            // Preserve the consumed line prefix for normal OK/URC handling.
            String response = AT_NL;
            complete = waitResponse(1000L, response) == 1;
          }
        }
        if (complete) {
          sock->sock_available = modemGetAvailable(mux);
        } else {
          DBGLOG(Error, "[TinyGsmSim7080] (#%hhu) Invalid or incomplete CARECV response; close socket.", mux)
          sock->resetSocketState();
          buffered = 0;

          // The binary frame boundary is unknown: discard raw output first.
          // Allow a bounded quiet interval for late close-command output
          // before parsing the response to a separate AT probe.
          discardStreamUntilQuiet(3000L, 100L);
          sendAT(GF("+CACLOSE="), mux);
          bool synchronized = false;
          if (discardStreamUntilQuiet(6000L, 3000L)) {
            sendAT(GF(""));
            synchronized = waitResponse() == 1;
          }
          if (!synchronized) {
            // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1c
            // Every socket shares this AT stream; none may use an unknown
            // frame boundary until explicit modem recovery succeeds.
            atStreamSynchronized_ = false;
            for (uint8_t i = 0; i < TINY_GSM_MUX_COUNT; ++i) {
              if (sockets[i]) { sockets[i]->resetSocketState(); }
            }
            DBGLOG(Error, "[TinyGsmSim7080] AT resynchronization failed; modem recovery required.")
          }
        }
      }
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %u", mux, buffered)
    return buffered;
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
    for (uint8_t muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      // after the last connection, there's an ok, so we catch it right away
      int res = waitResponse(3000, GF("+CARECV:"), GFP(GSM_OK), GFP(GSM_ERROR));
      // if we get the +CARECV: response, read the mux number and the number of
      // characters available
      if (res == 1) {
        uint8_t ret_mux = streamGetUInt8Before(',');
        size_t result  = streamGetSizeBefore('\n');

        if (ret_mux >= TINY_GSM_MUX_COUNT) {
          DBGLOG(Error, "[TinyGsmSim7080] (mux: %hhu) ret_mux out of range: %hhu (range: 0..%i), result: %u", 
            mux, ret_mux, TINY_GSM_MUX_COUNT - 1, result);
          continue;
        }

        result_sum += result;

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) muxNo: %hhu, res=1: ret_mux: %hhu, available: %u", mux, muxNo, ret_mux, result);

        GsmClientSim7080* sock    = sockets[ret_mux];
        // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
        if (sock) { sock->sock_available = sock->sock_stopped ? 0 : result; }
        // if the first returned mux isn't 0 (or is higher than expected)
        // we need to fill in the missing muxes
        if (ret_mux > muxNo) {
          for (uint8_t extra_mux = muxNo; extra_mux < ret_mux; extra_mux++) {
            GsmClientSim7080* isock = sockets[extra_mux];
            if (isock) { isock->sock_available = 0; }
          }
          muxNo = ret_mux;
        }
      } else if (res == 2) {
        // if we get an OK, we've reached the last socket with available data
        // so we set any we haven't gotten to yet to 0

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) muxNo: %hhu, res=2.", mux, muxNo);

        for (uint8_t extra_mux = muxNo; extra_mux < TINY_GSM_MUX_COUNT; extra_mux++) {
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

    for (uint8_t muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      // after the last connection, there's an ok, so we catch it right away
      int8_t res = waitResponse(3000, GF("+CASTATE:"), GFP(GSM_OK), GFP(GSM_ERROR));
      // if we get the +CASTATE: response, read the mux number and the status
      if (res == 1) {
        uint8_t ret_mux = streamGetUInt8Before(',');
        size_t status  = streamGetSizeBefore('\n');

        if (ret_mux >= TINY_GSM_MUX_COUNT) {
          DBGLOG(Error, "[TinyGsmSim7080] (mux: %hhu) ret_mux out of range: %hhu (range: 0..%i), status: %u", 
            mux, ret_mux, TINY_GSM_MUX_COUNT - 1, status);
          continue;
        }

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) muxNo: %hhu, res=ok: ret_mux: %hhu, status: %u-%s", 
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
          // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
          sock->sock_connected = !sock->sock_stopped && (status == 1);
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

        DBGLOG(Debug, "[TinyGsmSim7080] (mux: %hhu) : muxNo: %hhu, res=error.", mux, muxNo)

        for (uint8_t extra_mux = muxNo; extra_mux < TINY_GSM_MUX_COUNT; extra_mux++) {
          GsmClientSim7080* isock = sockets[extra_mux];
          if (isock) { isock->sock_connected = false; }
        }
        break;
      } else {
        // if we got an error, give up
        DBGLOG(Warn, "[TinyGsmSim7080] (mux: %hhu) waitResponse() returned error: %hhi", mux, res)
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
      uint8_t  mux = streamGetUInt8Before(',');
      size_t len = streamGetSizeBefore('\n');
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      if (mux < TINY_GSM_MUX_COUNT && sockets[mux] && !sockets[mux]->sock_stopped) {
        sockets[mux]->got_data = true;
        DBGCHK(Error, len <=1024, "[TinyGsmSim7080] len (%zu) out of range [0..1024]!", len)
        if (len <= 1024) { sockets[mux]->sock_available = len; }
      }
      data = "";
      DBGLOG(Debug, "{TinyGsmSim7080} Got Data on mux: %hhu, len: %zu", mux, len)
      return true;
    } else if (data.endsWith(GF("+CADATAIND:"))) {
      uint8_t mux = streamGetUInt8Before('\n');
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      if (mux < TINY_GSM_MUX_COUNT && sockets[mux] && !sockets[mux]->sock_stopped) {
        sockets[mux]->got_data = true;
      }
      data = "";
      DBGLOG(Debug, "{TinyGsmSim7080} Got Data on mux: %hhu.", mux)
      return true;
    } else if (data.endsWith(GF("+CASTATE:"))) {
      uint8_t mux = streamGetUInt8Before(',');
      int8_t state = streamGetInt8Before('\n');
      // 14.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 1d
      if (mux < TINY_GSM_MUX_COUNT && sockets[mux] && !sockets[mux]->sock_stopped) {
        if (state != 1) {
          sockets[mux]->sock_connected = false;
          DBGLOG(Info, "{TinyGsmSim7080} Closed mux: %hhu", mux)
        }
      }
      data = "";
      return true;
    } else if (data.endsWith(GF("*PSNWID:"))) {
      char dest[128];
      streamGetCharBefore('\n', dest, sizeof(dest));  // Refresh time zone by network
//      streamSkipUntil('\n');  // Refresh network name by network
      data = "";
      DBGLOG(Info, "\n\n{TinyGsmSim7080} Network name updated, *PSNWID: %s\n\n", dest)
      return true;
    } else if (data.endsWith(GF("*PSUTTZ:"))) {
      char dest[32];
      streamGetCharBefore('\n', dest, sizeof(dest));  // Refresh time zone by network
//      streamSkipUntil('\n');  // Refresh time and time zone by network
      data = "";
      DBGLOG(Info, "\n\n{TinyGsmSim7080} Network time and time zone updated, *PSUTTZ: %s\n\n", dest)
      return true;
//    } else if (data.endsWith(GF("+CTZV:"))) {
    } else if (data.endsWith(GF("CTZV:"))) {
      char dest[32];
      streamGetCharBefore('\n', dest, sizeof(dest));  // Refresh time zone by network
//      streamSkipUntil('\n');  // Refresh network time zone by network
      data = "";
      DBGLOG(Info, "\n\n{TinyGsmSim7080} Network time zone updated, +CTZV: %s\n\n", dest)
      return true;
    } else if (data.endsWith(GF("DST: "))) {
      int dst = streamGetIntegerBefore('\n');  // Refresh time zone by network
      data = "";
      DBGCHK(Error, (dst == 0) || (dst == 1), "{TinyGsmSim7080} Daylight savings time state updated, DST out of range: %i", dst)
      DBGCHK(Info, !((dst == 0) || (dst == 1)), "\n\n{TinyGsmSim7080} Daylight savings time state updated, DST: %i\n\n", dst)
      if ((dst == 0) || (dst == 1)) { 
        dayLightSaving = dst;
      }
      return true;
    } else if (data.endsWith(GF(AT_NL "SMS Ready" AT_NL))) {
      data = "";
      DBGLOG(Warn, "{TinyGsmSim7080} Unexpected module reset!")
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
//  String            certificates[TINY_GSM_MUX_COUNT];
}; // class TinyGsmSim7080

#endif  // SRC_TINYGSMCLIENTSIM7080_H_
