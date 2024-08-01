/**
 * @file       TinyGsmClientSIM70xx.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCLIENTSIM70XX_H_
#define SRC_TINYGSMCLIENTSIM70XX_H_

// #define TINY_GSM_DEBUG Serial
// #define TINY_GSM_USE_HEX
#ifdef AT_NL
#undef AT_NL
#endif
#define AT_NL "\r\n"

#ifdef MODEM_MANUFACTURER
#undef MODEM_MANUFACTURER
#endif
#define MODEM_MANUFACTURER "SIMCom"

#ifdef MODEM_MODEL
#undef MODEM_MODEL
#endif
#if defined(TINY_GSM_MODEM_SIM7070)
#define MODEM_MODEL "SIM7070";
#elif defined(TINY_GSM_MODEM_SIM7080)
#define MODEM_MODEL "SIM7080";
#elif defined(TINY_GSM_MODEM_SIM7090)
#define MODEM_MODEL "SIM7090";
#elif defined(TINY_GSM_MODEM_SIM7000) || defined(TINY_GSM_MODEM_SIM7000SSL)
#define MODEM_MODEL "SIM7000";
#else
#define MODEM_MODEL "SIM70xx";
#endif

#include "TinyGsmModem.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmGPS.tpp"


// Logging
#undef MS_LOGGER_LEVEL
#ifdef MS_TINYGSM_LOGGING
#define MS_LOGGER_LEVEL MS_TINYGSM_LOGGING
#endif
#include "ESP32Logger.h"


enum SIM70xxRegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};

template <class SIM70xxType>
class TinyGsmSim70xx : public TinyGsmModem<SIM70xxType>,
                       public TinyGsmGPRS<SIM70xxType>,
                       public TinyGsmGPS<SIM70xxType> {
  friend class TinyGsmModem<SIM70xxType>;
  friend class TinyGsmGPRS<SIM70xxType>;
  friend class TinyGsmGPS<SIM70xxType>;

  /*
   * CRTP Helper
   */
 protected:
  inline const SIM70xxType& thisModem() const {
    return static_cast<const SIM70xxType&>(*this);
  }
  inline SIM70xxType& thisModem() {
    return static_cast<SIM70xxType&>(*this);
  }
  ~TinyGsmSim70xx() {}

  /*
   * Constructor
   */
 public:
  explicit TinyGsmSim70xx(Stream& stream) : stream(stream) {}

  /*
   * Basic functions
   */
 protected:
  bool factoryDefaultImpl() {
    return false;
  }

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = nullptr) {
    DBGLOG(Info, "[GsmClientSim70xx] >>")
    bool ret = false;
//    thisModem().sendAT(GF("E0"));  // Echo Off
//    thisModem().waitResponse();
    if (!thisModem().setPhoneFunctionality(0)) { goto end; }
    if (!thisModem().setPhoneFunctionality(1, true)) { goto end; }
    thisModem().waitResponse(30000L, GF("SMS Ready"));
    ret = thisModem().initImpl(pin);
  end:    
    DBGLOG(Info, "[GsmClientSim70xx] << return: %s", DBGB2S(ret))
    return ret;
  }

  bool powerOffImpl() {
    thisModem().sendAT(GF("+CPOWD=1"));
    return thisModem().waitResponse(GF("NORMAL POWER DOWN")) == 1;
  }

  // During sleep, the SIM70xx module has its serial communication disabled.
  // In order to reestablish communication pull the DRT-pin of the SIM70xx
  // module LOW for at least 50ms. Then use this function to disable sleep
  // mode. The DTR-pin can then be released again.
  bool sleepEnableImpl(bool enable = true) {
    thisModem().sendAT(GF("+CSCLK="), enable);
    return thisModem().waitResponse() == 1;
  }

  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false) {
    DBGLOG(Info, "[GsmClientSim70xx] >> AT+CFUN=%hhu%s", fun, reset ? ",1" : "")
    thisModem().sendAT(GF("+CFUN="), fun, reset ? ",1" : "");
    bool ret = thisModem().waitResponse(10000L) == 1;
    DBGLOG(Info, "[GsmClientSim70xx] << return: %s", DBGB2S(ret))
    return ret;
  }

  /*
   * Generic network functions
   */
 public:
  SIM70xxRegStatus getRegistrationStatus() {
    DBGLOG(Info, "[TinyGsmSim70xx] >>")

    MS_TINY_GSM_SEM_TAKE_WAIT("getRegistrationStatus")

    SIM70xxRegStatus epsStatus = (SIM70xxRegStatus)thisModem().getRegistrationStatusXREG("CEREG");

    // If we're connected on EPS, great!
    if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
    } else {
      // Otherwise, check GPRS network status
      // We could be using GPRS fall-back or the board could be being moody
      epsStatus = (SIM70xxRegStatus)thisModem().getRegistrationStatusXREG("CGREG");
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    if (epsStatus > REG_OK_ROAMING) {
      DBGLOG(Warn, "[TinyGsmSim70xx] registration status unknown: %i", epsStatus)
    }

    DBGLOG(Info, "[TinyGsmSim70xx] << return registration status: %i", epsStatus)
    return epsStatus;
  }

 protected:
  bool isNetworkConnectedImpl() {
    SIM70xxRegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

 public:
  String getNetworkModes() {
    String res;

    MS_TINY_GSM_SEM_TAKE_WAIT("getNetworkModes")

    // Get the help string, not the setting value
    thisModem().sendAT(GF("+CNMP=?"));
    if (thisModem().waitResponse(GF(AT_NL "+CNMP:")) != 1) {  
    } else {
      res = stream.readStringUntil('\n');
      thisModem().waitResponse();
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    return res;
  }

  int16_t getNetworkMode() {
    int16_t mode = 0;

    MS_TINY_GSM_SEM_TAKE_WAIT("getNetworkMode")

    thisModem().sendAT(GF("+CNMP?"));
    if (thisModem().waitResponse(GF(AT_NL "+CNMP:")) != 1) {
    } else {
        mode = thisModem().streamGetIntBefore('\n');
        thisModem().waitResponse();
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    return mode;
  }

  bool setNetworkMode(uint8_t mode) {
    // 2 Automatic
    // 13 GSM only
    // 38 LTE only
    // 51 GSM and LTE only
    thisModem().sendAT(GF("+CNMP="), mode);
    return thisModem().waitResponse() == 1;
  }

  String getPreferredModes() {
    String res;

    MS_TINY_GSM_SEM_TAKE_WAIT("getPreferredModes")

    // Get the help string, not the setting value
    thisModem().sendAT(GF("+CMNB=?"));
    if (thisModem().waitResponse(GF(AT_NL "+CMNB:")) != 1) { 
    } else {
      res = stream.readStringUntil('\n');
      thisModem().waitResponse();
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    return res;
  }

  int16_t getPreferredMode() {
    int16_t mode = 0;

    MS_TINY_GSM_SEM_TAKE_WAIT("getPreferredMode")

    thisModem().sendAT(GF("+CMNB?"));
    if (thisModem().waitResponse(GF(AT_NL "+CMNB:")) != 1) {
    } else {
      mode = thisModem().streamGetIntBefore('\n');
      thisModem().waitResponse();
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    return mode;
  }

  bool setPreferredMode(uint8_t mode) {
    // 1 CAT-M
    // 2 NB-IoT
    // 3 CAT-M and NB-IoT
    thisModem().sendAT(GF("+CMNB="), mode);
    return thisModem().waitResponse() == 1;
  }

  bool getNetworkSystemMode(bool& n, int16_t& stat) {
    // n: whether to automatically report the system mode info
    // stat: the current service. 0 if it not connected

    MS_TINY_GSM_SEM_TAKE_WAIT("getNetworkSystemMode")

    thisModem().sendAT(GF("+CNSMOD?"));
    if (thisModem().waitResponse(GF(AT_NL "+CNSMOD:")) != 1) { goto end; }
    n    = thisModem().streamGetIntBefore(',') != 0;
    stat = thisModem().streamGetIntBefore('\n');
    thisModem().waitResponse();

end:
    MS_TINY_GSM_SEM_GIVE_WAIT
    
    return true;
  }

  bool setNetworkSystemMode(bool n) {
    // n: whether to automatically report the system mode info
    thisModem().sendAT(GF("+CNSMOD="), int8_t(n));
    return thisModem().waitResponse() == 1;
  }

  /*
   * GPRS functions
   */
 protected:
  // should implement in sub-classes

  /*
   * SIM card functions
   */
 protected:
  // Doesn't return the "+CCID" before the number
  String getSimCCIDImpl() {
    thisModem().sendAT(GF("+CCID"));
    if (thisModem().waitResponse(GF(AT_NL)) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    thisModem().waitResponse();
    res.trim();
    return res;
  }

  /*
   * GPS/GNSS/GLONASS location functions
   */
 protected:
  // enable GPS
  bool enableGPSImpl() {
    thisModem().sendAT(GF("+CGNSPWR=1"));
    if (thisModem().waitResponse() != 1) { return false; }
    return true;
  }

  bool disableGPSImpl() {
    thisModem().sendAT(GF("+CGNSPWR=0"));
    if (thisModem().waitResponse() != 1) { return false; }
    return true;
  }

  // get the RAW GPS output
  String getGPSrawImpl() {
    thisModem().sendAT(GF("+CGNSINF"));
    if (thisModem().waitResponse(10000L, GF(AT_NL "+CGNSINF:")) != 1) {
      return "";
    }
    String res = stream.readStringUntil('\n');
    thisModem().waitResponse();
    res.trim();
    return res;
  }

  // get GPS informations
  bool getGPSImpl(float* lat, float* lon, float* speed = 0, float* alt = 0,
                  int* vsat = 0, int* usat = 0, float* accuracy = 0,
                  int* year = 0, int* month = 0, int* day = 0, int* hour = 0,
                  int* minute = 0, int* second = 0) {
    thisModem().sendAT(GF("+CGNSINF"));
    if (thisModem().waitResponse(10000L, GF(AT_NL "+CGNSINF:")) != 1) {
      return false;
    }

    thisModem().streamSkipUntil(',');                // GNSS run status
    if (thisModem().streamGetIntBefore(',') == 1) {  // fix status
      // init variables
      float ilat         = 0;
      float ilon         = 0;
      float ispeed       = 0;
      float ialt         = 0;
      int   ivsat        = 0;
      int   iusat        = 0;
      float iaccuracy    = 0;
      int   iyear        = 0;
      int   imonth       = 0;
      int   iday         = 0;
      int   ihour        = 0;
      int   imin         = 0;
      float secondWithSS = 0;

      // UTC date & Time
      iyear        = thisModem().streamGetIntLength(4);  // Four digit year
      imonth       = thisModem().streamGetIntLength(2);  // Two digit month
      iday         = thisModem().streamGetIntLength(2);  // Two digit day
      ihour        = thisModem().streamGetIntLength(2);  // Two digit hour
      imin         = thisModem().streamGetIntLength(2);  // Two digit minute
      secondWithSS = thisModem().streamGetFloatBefore(
          ',');  // 6 digit second with subseconds

      ilat = thisModem().streamGetFloatBefore(',');  // Latitude
      ilon = thisModem().streamGetFloatBefore(',');  // Longitude
      ialt = thisModem().streamGetFloatBefore(
          ',');  // MSL Altitude. Unit is meters
      ispeed = thisModem().streamGetFloatBefore(
          ',');                          // Speed Over Ground. Unit is knots.
      thisModem().streamSkipUntil(',');  // Course Over Ground. Degrees.
      thisModem().streamSkipUntil(',');  // Fix Mode
      thisModem().streamSkipUntil(',');  // Reserved1
      iaccuracy = thisModem().streamGetFloatBefore(
          ',');                          // Horizontal Dilution Of Precision
      thisModem().streamSkipUntil(',');  // Position Dilution Of Precision
      thisModem().streamSkipUntil(',');  // Vertical Dilution Of Precision
      thisModem().streamSkipUntil(',');  // Reserved2
      ivsat = thisModem().streamGetIntBefore(',');  // GNSS Satellites in View
      iusat = thisModem().streamGetIntBefore(',');  // GNSS Satellites Used
      thisModem().streamSkipUntil(',');             // GLONASS Satellites Used
      thisModem().streamSkipUntil(',');             // Reserved3
      thisModem().streamSkipUntil(',');             // C/N0 max
      thisModem().streamSkipUntil(',');             // HPA
      thisModem().streamSkipUntil('\n');            // VPA

      // Set pointers
      if (lat != nullptr) *lat = ilat;
      if (lon != nullptr) *lon = ilon;
      if (speed != nullptr) *speed = ispeed;
      if (alt != nullptr) *alt = ialt;
      if (vsat != nullptr) *vsat = ivsat;
      if (usat != nullptr) *usat = iusat;
      if (accuracy != nullptr) *accuracy = iaccuracy;
      if (iyear < 2000) iyear += 2000;
      if (year != nullptr) *year = iyear;
      if (month != nullptr) *month = imonth;
      if (day != nullptr) *day = iday;
      if (hour != nullptr) *hour = ihour;
      if (minute != nullptr) *minute = imin;
      if (second != nullptr) *second = static_cast<int>(secondWithSS);

      thisModem().waitResponse();
      return true;
    }

    thisModem().streamSkipUntil('\n');  // toss the row of commas
    thisModem().waitResponse();
    return false;
  }
  bool handleURCs(String& data) {
    return thisModem().handleURCs(data);
  }

 public:
  // <MS>
  /**
  // @brief       Logs actual settings of AT+SLEDS, AT+CNETLIGHT, AT+CSGS.
  // @return      true = okay
  // @note        Details see latest "SIM7070_SIM7080_SIM7090 Series_AT Command Manual".
  **/
  bool reportNetlightStatus() {
  
    MS_TINY_GSM_SEM_TAKE_WAIT("reportNetlightStatus")

    DBGLOG(Info, "[TinyGsmSim70xx] AT+SLEDS=?")
    thisModem().sendAT(GF("+SLEDS=?"));
    thisModem().waitResponse(2000L);

    DBGLOG(Info, "[TinyGsmSim70xx] AT+SLEDS?")
    thisModem().sendAT(GF("+SLEDS?"));
    thisModem().waitResponse(2000L);

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CNETLIGHT=?")
    thisModem().sendAT(GF("+CNETLIGHT=?"));
    thisModem().waitResponse(2000L);

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CNETLIGHT?")
    thisModem().sendAT(GF("+CNETLIGHT?"));
    thisModem().waitResponse(2000L);

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CSGS=?")
    thisModem().sendAT(GF("+CSGS=?"));
    thisModem().waitResponse(2000L);

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CSGS?")
    thisModem().sendAT(GF("+CSGS?"));
    thisModem().waitResponse(2000L);

    MS_TINY_GSM_SEM_GIVE_WAIT

    return true;
  } // TinyGsmSim70xx.reportNetlightStatus


  // <MS>
  /**
  // @brief       Sets behaviour of netlight-led (AT+SLEDS).
  // @param       mode      1-not registered (e.g. no sim), 2-registered to net, 3-connected.
  // @param       timerOn   duration (0-65535ms) led is on.
  // @param       timerOff  duration (0-65535ms) led is off.
  // @return      true = okay
  // @note        Details see latest "SIM7070_SIM7080_SIM7090 Series_AT Command Manual".
  // @note        Will only be effective if activated with setNetlightIndication(2).
  **/
  bool setNetlightTimerPeriod(uint8_t mode, uint16_t timerOn, uint16_t timerOff) {
    DBGLOG(Info, "[TinyGsmSim70xx] >>")
    DBGCHK(Warn, (mode >= 1) && (mode <= 3), "[TinyGsmSim70xx] Invalid mode (must be 1, 2, 3): %hhu", mode)
    DBGCHK(Warn, (timerOn == 0) || (timerOn >= 40), "[TinyGsmSim70xx] Invalid timerOn (0 or 40..65535): %hu", timerOn)
    DBGCHK(Warn, (timerOff == 0) || (timerOff >= 40), "[TinyGsmSim70xx] Invalid timerOff (0 or 40..65535): %hu", timerOff)
    DBGLOG(Info, "[TinyGsmSim70xx] AT+SLEDS=%hhu,%hu,%hu", mode, timerOn, timerOff)
  
    MS_TINY_GSM_SEM_TAKE_WAIT("setNetlightTimerPeriod")

    thisModem().sendAT(GF("+SLEDS="), mode, GF(","), timerOn, GF(","), timerOff);
    int8_t ret = thisModem().waitResponse(2000L);

    MS_TINY_GSM_SEM_GIVE_WAIT
    
    DBGLOG(Info, "[TinyGsmSim70xx] << return: %s (ret: %hhi)", DBGB2S(ret == 1), ret)
    return ret == 1;
  } // TinyGsmSim70xx.setNetlightTimerPeriod


  // <MS>
  /**
  // @brief       Turns netlight-led on and off (AT+CNETLIGHT).
  // @param       mode      0-off, 1-on.
  // @return      true = okay
  // @note        Details see latest "SIM7070_SIM7080_SIM7090 Series_AT Command Manual".
  **/
  bool setNetlightOn(uint8_t mode) {
    DBGLOG(Info, "[TinyGsmSim70xx] >>")
    DBGCHK(Warn, (mode >= 0) && (mode <= 1), "[TinyGsmSim70xx] Invalid mode: %hhu", mode)
    DBGLOG(Info, "[TinyGsmSim70xx] AT+CNETLIGHT=%hhu", mode)
  
    MS_TINY_GSM_SEM_TAKE_WAIT("setNetlightOn")

    thisModem().sendAT(GF("+CNETLIGHT="), mode);
    int8_t ret = thisModem().waitResponse(2000L);

    MS_TINY_GSM_SEM_GIVE_WAIT
    
    DBGLOG(Info, "[TinyGsmSim70xx] << return: %s (ret: %hhi)", DBGB2S(ret == 1), ret)
    return ret == 1;
  } // TinyGsmSim70xx.setNetlightOn


  // <MS>
  /**
  // @brief       Sets behaviour of netlight-led (AT+CSGS).
  // @param       mode      0-disable, 1-default/standard, 2-individual, as set by +SLEDS/setNetlightTimerPeriod.
  // @return      true = okay
  // @note        Details see latest "SIM7070_SIM7080_SIM7090 Series_AT Command Manual".
  **/
  bool setNetlightIndication(uint8_t mode) {
    DBGLOG(Info, "[TinyGsmSim70xx] >>")
    DBGCHK(Warn, (mode >= 0) && (mode <= 2), "[TinyGsmSim70xx] Invalid mode: %hhu", mode)
    DBGLOG(Info, "[TinyGsmSim70xx] AT+CSGS=%hhu", mode)
  
    MS_TINY_GSM_SEM_TAKE_WAIT("setNetlightIndication")

    thisModem().sendAT(GF("+CSGS="), mode);
    int8_t ret = thisModem().waitResponse(2000L);

    MS_TINY_GSM_SEM_GIVE_WAIT
    
    DBGLOG(Info, "[TinyGsmSim70xx] << return: %s (ret: %hhi)", DBGB2S(ret == 1), ret)
    return ret == 1;
  } // TinyGsmSim70xx.setNetlightIndication

  // <MS>
  /**
  // @brief       RequestTA Revision Identification of Software Release (AT+CGMR).
  // @return      Software Release Identification, or empty "" if failed.
  // @note        Details see latest "SIM7070_SIM7080_SIM7090 Series_AT Command Manual".
  **/
  String getModemRevisionSoftwareRelease() {
  
    MS_TINY_GSM_SEM_TAKE_WAIT("getModemRevisionSoftwareRelease")

    thisModem().sendAT(GF("+CGMR"));
    String res;
    if (thisModem().waitResponse(1000L, res) != 1) { return ""; }

    MS_TINY_GSM_SEM_GIVE_WAIT
    
    // Do the replaces twice so we cover both \r and \r\n type endings
    res.replace("\r\nOK\r\n", "");
    res.replace("\rOK\r", "");
    res.replace("\r\n", " ");
    res.replace("\r", " ");
    // <MS> Additionally:
    res.replace("OK", "");
    res.replace("/", "");
    res.replace("Revision:", "");
    res.trim();
    return res;
  } // TinyGsmSim70xx.getModemRevisionSoftwareRelease


 public:
  Stream& stream;
};

#endif  // SRC_TINYGSMCLIENTSIM70XX_H_
