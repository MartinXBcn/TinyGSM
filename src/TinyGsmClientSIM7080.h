/**
 * @file       TinyGsmClientSim7080.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCLIENTSIM7080_H_
#define SRC_TINYGSMCLIENTSIM7080_H_


// For char-to-hex-conversion.
#include "ms_General.h"


#define TINY_GSM_MUX_COUNT 12
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE

#include "TinyGsmClientSIM70xx.h"
#include "TinyGsmTCP.tpp"
#include "TinyGsmSSL.tpp"


// Logging
#undef MS_LOGGER_LEVEL
#ifdef MS_TINYGSM_LOGGING
#define MS_LOGGER_LEVEL MS_TINYGSM_LOGGING
#endif
#include "ESP32Logger.h"



class TinyGsmSim7080 : public TinyGsmSim70xx<TinyGsmSim7080>,
                       public TinyGsmTCP<TinyGsmSim7080, TINY_GSM_MUX_COUNT>,
                       public TinyGsmSSL<TinyGsmSim7080> {
  friend class TinyGsmSim70xx<TinyGsmSim7080>;
  friend class TinyGsmTCP<TinyGsmSim7080, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSSL<TinyGsmSim7080>;

  /*
   * Inner Client
   */
 public:

  //
  // class GsmClientSim7080
  //
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

   public:
    bool setCertificate(const String& certificateName) {
      DBGLOG(Info, "[GsmClientSecureSIM7080] >> certificateName: '%s'", certificateName.c_str())
      bool r = at->setCertificate(certificateName, mux);
      DBGLOG(Info, "[GsmClientSecureSIM7080] << return: %s", DBGB2S(r))
      return r;
    }

    virtual int connect(const char* host, uint16_t port,
                        int timeout_s) override {
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




  /*
   * Constructor
   */
 public:
  explicit TinyGsmSim7080(Stream& stream)
      : TinyGsmSim70xx<TinyGsmSim7080>(stream),
        certificates() {
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
  bool initImpl(const char* pin = NULL) {
    SimStatus ret;
    bool r = false;
    DBGLOG(Info, "[TinyGsmSim7080] >> pin: '%s'", pin == NULL ? "-" : pin);
    DBGLOG(Info, "[TinyGsmSim7080] Version: %s", TINYGSM_VERSION);
    DBGLOG(Info, "[TinyGsmSim7080] Compiled Module: TinyGsmClientSIM7080");

    MS_TINY_GSM_SEM_TAKE_WAIT("initImpl")

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
      r = false; 
      goto end; 
    }

    // Enable battery checks
// <MS>
//    sendAT(GF("+CBATCHK=1"));
    // Disable battery checks
    sendAT(GF("+CBATCHK=0"));
// <MS>
    if (waitResponse() != 1) { 
      DBGLOG(Error, "[TinyGsmSim7080] Disable-bat-checks failed! Stop.")
      r = false; 
      goto end; 
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != NULL && strlen(pin) > 0) {
      simUnlock(pin);
      r = getSimStatus() == SIM_READY;
    } else {
      // if the sim is ready, or it's locked but no pin has been provided, return true
      r = (ret == SIM_READY) || (ret == SIM_LOCKED);
    }

    goto endx;

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

endx:
    DBGLOG(Info, "[TinyGsmSim7080] << return: %s", DBGB2S(r));
    return r;
  } // TinyGsmSim7080::initImpl(...)

  void maintainImpl() {
    // Keep listening for modem URC's and proactively iterate through
    // sockets asking if any data is avaiable
//    DBGLOG(Info, "[TinyGsmSim7080::maintainImpl] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT("maintainImpl")

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

// <MS> #####
//    if (check_socks) { modemGetAvailable(0); }
    // Check all mux in use (-1).
    if (check_socks) { modemGetAvailable((uint8_t)-1); }
// <MS>

    while (stream.available()) { waitResponse(15, NULL, NULL); }

    MS_TINY_GSM_SEM_GIVE_WAIT

//    DBGLOG(Info, "[TinyGsmSim7080::maintainImpl] <<");
  } // TinyGsmSim7080::maintainImpl()

  /*
   * Power functions
   */
 protected:
  // Follows the SIM70xx template

  /*
   * Generic network functions
   */
 protected:
  String getLocalIPImpl() {
    DBGLOG(Debug, "[TinyGsmSim7080] >>");

    MS_TINY_GSM_SEM_TAKE_WAIT("getLocalIPImpl")

    String res;

    sendAT(GF("+CNACT?"));
    if (waitResponse(GF(GSM_NL "+CNACT:")) != 1) { res = ""; goto end; }
    streamSkipUntil('\"');
    res = stream.readStringUntil('\"');
    waitResponse();

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Debug, "[TinyGsmSim7080] << return: %s", res.c_str());
    return res;
  } // TinyGsmSim7080::getLocalIPImpl()

  /*
   * Secure socket layer functions
   */
 protected:
  bool setCertificate(const String& certificateName, const uint8_t mux = 0) {
    bool r = false;
    DBGLOG(Info, "[TinyGsmSim7080] >> mux: %hhu, certificateName: '%s'", mux, certificateName.c_str());
    if (mux >= TINY_GSM_MUX_COUNT) {
      r = false; 
    } else {
      certificates[mux] = certificateName;
      r = true;
    }
    DBGLOG(Info, "[TinyGsmSim7080] << return: %s", DBGB2S(r));
    return r;
  }

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = NULL,
                       const char* pwd = NULL) {
    DBGLOG(Info, "[TinyGsmSim7080] >> apn: '%s', user: %s", apn == NULL ? "-" : apn, user == NULL ? "-" : user);
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
      sendAT(GF("+CNCFG=0,1,\""), apn, "\",\"", user, "\",\"", pwd, '"');
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
      res = waitResponse(60000L, GF(GSM_NL "+APP PDP: 0,ACTIVE"),
                         GF(GSM_NL "+APP PDP: 0,DEACTIVE"));
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
 protected:
  // Follows the SIM70xx template

  /*
   * Messaging functions
   */
 protected:
  // Follows all messaging functions per template

  /*
   * GPS/GNSS/GLONASS location functions
   */
 protected:
  // Follows the SIM70xx template

  /*
   * Time functions
   */
  // Can follow CCLK as per template

  /*
   * NTP server functions
   */
  // Can sync with server using CNTP as per template

  /*
   * Battery functions
   */
 protected:
  // Follows all battery functions per template

  /*
   * Client related functions
   */
 protected:
  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 75) {
    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %huu, host: '%s', port: %hu, ssl: %s, timeout: %i", 
      mux, host == NULL ? "-" : host, port, DBGB2S(ssl), timeout_s)

    MS_TINY_GSM_SEM_TAKE_WAIT("modemConnect")
    
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;
    bool ret = false;
    int8_t res = -1;

    // set the connection (mux) identifier to use
    sendAT(GF("+CACID="), mux);
    if (waitResponse(timeout_ms) != 1) { 
      DBGLOG(Warn, "[TinyGsmSim7080] ERROR set the connection mux identifier (#%hhu) to use failed.", mux)
      ret = false; 
      goto end; 
    }


    if (ssl) {
      // set the ssl version
      // AT+CSSLCFG="SSLVERSION",<ctxindex>,<sslversion>
      // <ctxindex> PDP context identifier
      // <sslversion> 0: QAPI_NET_SSL_PROTOCOL_UNKNOWN
      //              1: QAPI_NET_SSL_PROTOCOL_TLS_1_0
      //              2: QAPI_NET_SSL_PROTOCOL_TLS_1_1
      //              3: QAPI_NET_SSL_PROTOCOL_TLS_1_2
      //              4: QAPI_NET_SSL_PROTOCOL_DTLS_1_0
      //              5: QAPI_NET_SSL_PROTOCOL_DTLS_1_2
      // NOTE:  despite docs using caps, "sslversion" must be in lower case
// <MS>
//      sendAT(GF("+CSSLCFG=\"sslversion\",0,3"));  // TLS 1.2
      sendAT(GF("+CSSLCFG=\"sslversion\","), mux, GF(",3"));  // TLS 1.2
// <MS>      
      if (waitResponse(5000L) != 1)  { 
        DBGLOG(Error, "[TinyGsmSim7080] ERROR set the ssl version failed.");
        ret = false; 
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
      sendAT(GF("+CASSLCFG="), mux, ',', GF("crindex,"), mux);
      waitResponse();
// <MS>

      // set the PDP context to apply SSL to
      // AT+CSSLCFG="CTXINDEX",<ctxindex>
      // <ctxindex> PDP context identifier
      // NOTE:  despite docs using "CRINDEX" in all caps, the module only
      // accepts the command "ctxindex" and it must be in lower case
// <MS>      
//      sendAT(GF("+CSSLCFG=\"ctxindex\",0"));
      sendAT(GF("+CSSLCFG=\"ctxindex\","), mux);
// <MS>      
      if (waitResponse(5000L, GF("+CSSLCFG:")) != 1)  { 
        DBGLOG(Error, "[TinyGsmSim7080] ERROR +CSSLCFG=\'ctxindex\' with mux %hhu failed.", mux);
        ret = false; 
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
        DBGLOG(Info, "[TinyGsmSim7080] set certificate ...");
        sendAT(GF("+CASSLCFG="), mux, ",CACERT,\"", certificates[mux].c_str(),
               "\"");
        if (waitResponse(5000L) != 1)  { 
          DBGLOG(Warn, "[TinyGsmSim7080] ERROR set certificate failed.");
          ret = false; 
          goto end; 
        }
      }

      // set the SSL SNI (server name indication)
      // NOTE:  despite docs using caps, "sni" must be in lower case
      DBGLOG(Info, "[TinyGsmSim7080] +CSSLCFG=/sni/ ...")
      sendAT(GF("+CSSLCFG=\"sni\","), mux, ',', GF("\""), host, GF("\""));
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
    if (waitResponse(timeout_ms, GF(GSM_NL "+CAOPEN:")) != 1) { ret = false; goto end; }
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

end:
    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %s, res: %hhi", mux, DBGB2S(ret), res);

    return ret;
  } // ::modemConnect(...)

  int16_t modemSend(const void* buff, size_t len, uint8_t mux) {
    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %hhu", mux);

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
    if (!sockets[mux]) { return 0; }
    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %hhu", mux);
    DBGCOD(char* tmp = new char[TINY_GSM_RX_BUFFER]; tmp[0] = '\0';)

    MS_TINY_GSM_SEM_TAKE_WAIT("modemRead")

    int16_t len_confirmed;
    size_t _size = size;

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

    int i;
    for (i = 0; i < len_confirmed; i++) {
      uint32_t startMillis = millis();
      while (!stream.available() &&
             (millis() - startMillis < sockets[mux]->_timeout)) {
        TINY_GSM_YIELD();
      }
      char c = stream.read();
//      DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) %4i: '%c'", mux, i, (c < 32 || c > 126) ? 'X' : c)
      DBGCOD(tmp[i] = c;)
      sockets[mux]->rx.put(c);
    }
    DBGCOD(tmp[i] = '\0';)
    DBGLOG(Debug,"[TinyGsmSim7080] (#%hhu) Read: %hs", mux, tmp)
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

    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %hhu", mux);
    DBGCHK(Warn, MS_TINY_GSM_SEM_BLOCKED, "[TinyGsmSim7080] Not blocked by calling function")

//    if (!sockets[mux]) { return 0; }
    // If a mux is given (i.e. mux != -1) check only if this mux is in use. 
    if ((mux != (uint8_t)-1) && (!sockets[mux])) { 
      DBGLOG(Debug, "[TinyGsmSim7080] << mux #%hhu not used, return: 0", mux);
      return 0; 
    }

    size_t result_sum = 0;

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

//        DBG("<MS-TinyGsm> [TinyGsmSim7080::modemGetAvailable]-A ret_mux: ", ret_mux, ", available: ", result);

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

//        DBG("<MS-TinyGsm> [TinyGsmSim7080::modemGetAvailable]-B muxNo: ", muxNo);

        for (int extra_mux = muxNo; extra_mux < TINY_GSM_MUX_COUNT;
             extra_mux++) {
          GsmClientSim7080* isock = sockets[extra_mux];
          if (isock) { isock->sock_available = 0; }
        }
        break;
      } else {
        // if we got an error, give up
        DBGLOG(Warn, "[TinyGsmSim7080] ERROR.");
        break;
      }
      // Should be a final OK at the end.
      // If every connection was returned, catch the OK here.
      // If only a portion were returned, catch it above.
      if (muxNo == TINY_GSM_MUX_COUNT - 1) { waitResponse(); }
    }

    modemGetConnected(mux);  // check the state of all connections
    if (mux == (uint8_t)-1) {
      // No specific mux given, all active muxs updated, no specific return-value expected by caller.
      DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %u", mux, result_sum);
      // If there was at least one mux with available return > 0.
      return result_sum;
    }
    if (!sockets[mux]) { 
      DBGLOG(Warn, "[TinyGsmSim7080] (#%hhu) << ERROR (sockets[mux] is null) return: 0", mux);
      return 0; }
    DBGLOG(Debug, "[TinyGsmSim7080] (#%hhu) << return: %hu", mux, sockets[mux]->sock_available);
    return sockets[mux]->sock_available;
  } // ::modemGetAvailable(...)


  bool modemGetConnected(uint8_t mux) {
    // NOTE:  This gets the state of all connections that have been opened
    // since the last connection
    DBGLOG(Debug, "[TinyGsmSim7080] >> mux: %hhu", mux)
    DBGCHK(Warn, MS_TINY_GSM_SEM_BLOCKED, "[TinyGsmSim7080] Not blocked by calling function")

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

//        DBG("<MS-TinyGsm> [TinyGsmSim7080::modemGetConnected]-A ret_mux: ", ret_mux, ", status: ", status);
        DBGLOG(Debug, "[TinyGsmSim7080]-A ret_mux: %i, status: %u", ret_mux, status)

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

//        DBG("<MS-TinyGsm> [TinyGsmSim7080::modemGetConnected]-B muxNo: ", muxNo);
        DBGLOG(Debug, "[TinyGsmSim7080]-B muxNo: %i", muxNo)

        for (int extra_mux = muxNo; extra_mux < TINY_GSM_MUX_COUNT;
             extra_mux++) {
          GsmClientSim7080* isock = sockets[extra_mux];
          if (isock) { isock->sock_connected = false; }
        }
        break;
      } else {
        // if we got an error, give up
        DBGLOG(Warn, "[TinyGsmSim7080] waitResponse() returned error: %i", res)
        break;
      }
      // Should be a final OK at the end.
      // If every connection was returned, catch the OK here.
      // If only a portion were returned, catch it above.
      if (muxNo == TINY_GSM_MUX_COUNT - 1) { waitResponse(); }
    }

    if (mux == (uint8_t)-1) {
      // No specific mux given, all active muxs updated, no specific return-value expected by caller.
      DBGLOG(Debug, "[TinyGsmSim7080] mux: %hhu, << return: %s", mux, DBGB2S(connected_sum))
      return connected_sum;
    }

    DBGLOG(Debug, "[TinyGsmSim7080] mux: %hhu, << return: %s", mux, DBGB2S(sockets[mux]->sock_connected))
    return sockets[mux]->sock_connected;
  } // ::modemGetConnected(...)

  /*
   * Utilities
   */
 public:
  // TODO(vshymanskyy): Optimize this!

  int msCallLevelWaitResponse = 0;
  int8_t waitResponse(uint32_t timeout_ms, String& data,
                      GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    msCallLevelWaitResponse++;
    DBGLOG(Debug, "[TinyGsmSim7080]-%i >>", msCallLevelWaitResponse)
    DBGCHK(Warn, MS_TINY_GSM_SEM_BLOCKED, "[TinyGsmSim7080]-%i Not blocked by calling function", msCallLevelWaitResponse)

    data.reserve(64);
    uint8_t  index       = 0;
    uint32_t startMillis = millis();
    do {
      TINY_GSM_YIELD();
      while (stream.available() > 0) {
        TINY_GSM_YIELD();
        int8_t a = stream.read();
        if (a <= 0) continue;  // Skip 0x00 bytes, just in case
        data += static_cast<char>(a);
        if (r1 && data.endsWith(r1)) {
          index = 1;
          goto finish;
        } else if (r2 && data.endsWith(r2)) {
          index = 2;
          goto finish;
        } else if (r3 && data.endsWith(r3)) {
#if defined TINY_GSM_DEBUG
          if (r3 == GFP(GSM_CME_ERROR)) {
            streamSkipUntil('\n');  // Read out the error
          }
#endif
          index = 3;
          goto finish;
        } else if (r4 && data.endsWith(r4)) {
          index = 4;
          goto finish;
        } else if (r5 && data.endsWith(r5)) {
          index = 5;
          goto finish;
        } else if (data.endsWith(GF("+CARECV:"))) {
          int8_t  mux = streamGetIntBefore(',');
          int16_t len = streamGetIntBefore('\n');

          DBGLOG(Info, "[TinyGsmSim7080]-%i mux: %hhi, len: %hi", msCallLevelWaitResponse, mux, len);

          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->got_data = true;
// <MS>            
//            if (len >= 0 && len <= 1024) { sockets[mux]->sock_available = len; }
/* */            
            if (len >= 0) { sockets[mux]->sock_available = len; }
            if ((len < 0) || (len > TINY_GSM_RX_BUFFER)) {
              DBGLOG(Warn, "[TinyGsmSim7080]-%i WARN len out of range: %hi", msCallLevelWaitResponse, len);
            }
           
// <MS>            
          }
          data = "";
          DBGLOG(Info, "[TinyGsmSim7080]-%i Got Data: %hi on mux: %hhi", msCallLevelWaitResponse, len, mux);
        } else if (data.endsWith(GF("+CADATAIND:"))) {
          int8_t mux = streamGetIntBefore('\n');
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            sockets[mux]->got_data = true;
          }
          data = "";
          DBGLOG(Info, "[TinyGsmSim7080]-%i Got data on mux: %hhi", msCallLevelWaitResponse, mux);
        } else if (data.endsWith(GF("+CASTATE:"))) {
          int8_t mux   = streamGetIntBefore(',');
          int8_t state = streamGetIntBefore('\n');
          if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
            if (state != 1) {
              sockets[mux]->sock_connected = false;
              DBGLOG(Info, "[TinyGsmSim7080]-%i Closed, mux: %hhi, state: %hhi", msCallLevelWaitResponse, mux, state);
            }
          }
          data = "";
        } else if (data.endsWith(GF("*PSNWID:"))) {
          streamSkipUntil('\n');  // Refresh network name by network
          data = "";
          DBGLOG(Info, "[TinyGsmSim7080]-%i Network name updated.", msCallLevelWaitResponse);
        } else if (data.endsWith(GF("*PSUTTZ:"))) {
          streamSkipUntil('\n');  // Refresh time and time zone by network
          data = "";
          DBGLOG(Info, "[TinyGsmSim7080]-%i Network time and time zone updated.", msCallLevelWaitResponse);
        } else if (data.endsWith(GF("+CTZV:"))) {
          streamSkipUntil('\n');  // Refresh network time zone by network
          data = "";
          DBGLOG(Info, "[TinyGsmSim7080]-%i Network time zone updated.", msCallLevelWaitResponse);
        } else if (data.endsWith(GF("DST: "))) {
          streamSkipUntil(
              '\n');  // Refresh Network Daylight Saving Time by network
          data = "";
          DBGLOG(Info, "[TinyGsmSim7080]-%i Daylight savings time state updated.", msCallLevelWaitResponse);
        } else if (data.endsWith(GF(GSM_NL "SMS Ready" GSM_NL))) {
          data = "";
          DBGLOG(Error, "[TinyGsmSim7080]-%i Unexpected module reset!", msCallLevelWaitResponse);
// <MS>          
//          init();
// <MS>          
          data = "";
        }
      }
    } while (millis() - startMillis < timeout_ms);
  finish:
    if (!index) {
      data.trim();
      if (data.length()) { DBGLOG(Warn, "[TinyGsmSim7080]-%i Unhandled: %s", msCallLevelWaitResponse, data.c_str()); }
      data = "";
    }
    data.replace(GSM_NL, "/");
    // DBG('<', index, '>', data);
/**/
#if defined TINY_GSM_DEBUG
//    if (index != 1) {
      String r1s(r1); r1s.trim();
      String r2s(r2); r2s.trim();
      String r3s(r3); r3s.trim();
      String r4s(r4); r4s.trim();
      String r5s(r5); r5s.trim();
      DBGLOG(Info, "[TinyGsmSim7080]-%i Input r1:%s, r2:%s, r3:%s, r4:%s, r5:%s", 
        msCallLevelWaitResponse, 
        asCharString(r1s.c_str(), 0, r1s.length()).c_str(),
        asCharString(r2s.c_str(), 0, r2s.length()).c_str(),
        asCharString(r3s.c_str(), 0, r3s.length()).c_str(),
        asCharString(r4s.c_str(), 0, r4s.length()).c_str(),
        asCharString(r5s.c_str(), 0, r5s.length()).c_str()
        )
      DBGLOG(Info, "[TinyGsmSim7080]-%i Return data: %s, return index: %hhu", msCallLevelWaitResponse, asCharString(data.c_str(), 0, data.length()).c_str(), index)
//    }
#endif

    DBGLOG(Debug, "[TinyGsmSim7080]-%i << return index: %hhu", msCallLevelWaitResponse, index)
    msCallLevelWaitResponse--;

    return index;
  } // waitResponse(...)

  int8_t waitResponse(uint32_t timeout_ms, GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    String data;
    return waitResponse(timeout_ms, data, r1, r2, r3, r4, r5);
  }

  int8_t waitResponse(GsmConstStr r1 = GFP(GSM_OK),
                      GsmConstStr r2 = GFP(GSM_ERROR),
#if defined TINY_GSM_DEBUG
                      GsmConstStr r3 = GFP(GSM_CME_ERROR),
                      GsmConstStr r4 = GFP(GSM_CMS_ERROR),
#else
                      GsmConstStr r3 = NULL, GsmConstStr r4 = NULL,
#endif
                      GsmConstStr r5 = NULL) {
    return waitResponse(1000, r1, r2, r3, r4, r5);
  }

 protected:
  GsmClientSim7080* sockets[TINY_GSM_MUX_COUNT];
  String            certificates[TINY_GSM_MUX_COUNT];
}; // class TinyGsmSim7080

#endif  // SRC_TINYGSMCLIENTSIM7080_H_
