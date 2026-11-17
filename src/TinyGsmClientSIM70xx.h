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


// For ms_strncpy(...)
#include "ms_General.h"


// Logging
#undef MS_LOGGER_LEVEL
#if defined(MS_TINYGSM_LOGGING) && defined(MS_LOGGER_ON)
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


// UserEquipmentSystemInfo (UESI)
#define MS_UESI_SYSTEMMODE_MAXLEN 16
#define MS_UESI_OPERATIONMODE_MAXLEN 32
#define MS_UESI_MCC_MNC_MAXLEN 8
#define MS_UESI_TAC_MAXLEN 16
#define MS_UESI_SCELLID_MAXLEN 16
#define MS_UESI_PCELLID_MAXLEN 16
#define MS_UESI_FREQUENCYBAND_MAXLEN 16

// <MS>
class MsUserEquipmentSystemInfo {
public:
  char systemMode[MS_UESI_SYSTEMMODE_MAXLEN] = "";
  char operationMode[MS_UESI_OPERATIONMODE_MAXLEN] = "";
  char MCC_MNC[MS_UESI_MCC_MNC_MAXLEN] = "";            // <MCC>-<MNC>
  char TAC[MS_UESI_TAC_MAXLEN] = "";
  char sCellID[MS_UESI_SCELLID_MAXLEN] = "";
  char pCellID[MS_UESI_PCELLID_MAXLEN] = "";
  char frequencyBand[MS_UESI_FREQUENCYBAND_MAXLEN] = "";
  int earfcn = 0;
  int dlbw = 0;
  int ulbw = 0;
  int RSRQ = 0;
  int RSRP = 0;
  int RSSI ;
  int RSSNR = 0;
};
// <MS>

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
  explicit TinyGsmSim70xx(Stream& _stream) : stream(_stream) {}

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
    String s;
    bool ret = thisModem().waitResponse(10000L, s) == 1;
    DBGLOG(Info, "AT+CFUN=%hhu%s returned - 1: %s, %s", fun, reset ? ",1" : "", DBGB2S(ret), s.c_str())
    thisModem().waitResponse(10000L, s);
    DBGLOG(Info, "AT+CFUN=%hhu%s returned - 2: %s", fun, reset ? ",1" : "", s.c_str())
    DBGLOG(Info, "[GsmClientSim70xx] << return: %s", DBGB2S(ret))
    return ret;
  }

  /*
   * Generic network functions
   */
 public:
  // <MS>
  const char* getRegistrationStatus(SIM70xxRegStatus espStatus) {
    switch(espStatus) {
      case REG_NO_RESULT: return "REG_NO_RESULT";
      case REG_UNREGISTERED: return "REG_UNREGISTERED";
      case REG_SEARCHING: return "REG_SEARCHING";
      case REG_DENIED: return "REG_DENIED";
      case REG_OK_HOME: return "REG_OK_HOME";
      case REG_OK_ROAMING: return "REG_OK_ROAMING";
      case REG_UNKNOWN: return "REG_UNKNOWN";
      default:
        return "Error";
    }
  }

  SIM70xxRegStatus getRegistrationStatus() {
    DBGLOG(Info, "[TinyGsmSim70xx] >>")

    MS_TINY_GSM_SEM_TAKE_WAIT

    SIM70xxRegStatus epsStatus = (SIM70xxRegStatus)thisModem().getRegistrationStatusXREG("CEREG");

    // If we're connected on EPS, great!
    if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
    } else {
      // Otherwise, check GPRS network status
      // We could be using GPRS fall-back or the board could be being moody
      epsStatus = (SIM70xxRegStatus)thisModem().getRegistrationStatusXREG("CGREG");
    }

    MS_TINY_GSM_SEM_GIVE_WAIT

    DBGLOG(Info, "[TinyGsmSim70xx] << return registration status: %i - %s", epsStatus, getRegistrationStatus(epsStatus))
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

    MS_TINY_GSM_SEM_TAKE_WAIT

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

    MS_TINY_GSM_SEM_TAKE_WAIT

    thisModem().sendAT(GF("+CNMP?"));
    if (thisModem().waitResponse(GF(AT_NL "+CNMP:")) != 1) {
    } else {
        mode = thisModem().streamGetInt16Before('\n');
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

    MS_TINY_GSM_SEM_TAKE_WAIT

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

    MS_TINY_GSM_SEM_TAKE_WAIT

    thisModem().sendAT(GF("+CMNB?"));
    if (thisModem().waitResponse(GF(AT_NL "+CMNB:")) != 1) {
    } else {
      mode = thisModem().streamGetInt16Before('\n');
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
    bool success = false;

    MS_TINY_GSM_SEM_TAKE_WAIT

    thisModem().sendAT(GF("+CNSMOD?"));
    if (thisModem().waitResponse(GF(AT_NL "+CNSMOD:")) != 1) { goto end; }
    n    = thisModem().streamGetIntegerBefore(',') != 0;
    stat = thisModem().streamGetInt16Before('\n');
    thisModem().waitResponse();
    success = true;

end:
    MS_TINY_GSM_SEM_GIVE_WAIT
    
    return success;
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

  // 17.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 6
  // SIM7070/7080/7090 return 18 fields; SIM7000 returns 21. Read one complete
  // row instead of waiting for commas that do not exist in the shorter layout.
  bool getGPSImpl(float* lat, float* lon, float* speed = 0, float* alt = 0,
                  int* vsat = 0, int* usat = 0, float* accuracy = 0,
                  int* year = 0, int* month = 0, int* day = 0, int* hour = 0,
                  int* minute = 0, int* second = 0) {
    MS_TINY_GSM_SEM_TAKE_WAIT

    const bool success = [&]() -> bool {
      thisModem().sendAT(GF("+CGNSINF"));
      if (thisModem().waitResponse(10000L, GF(AT_NL "+CGNSINF:")) != 1) {
        return false;
      }

      char row[512];
      const bool complete = thisModem().streamGetCharBefore('\n', row, sizeof(row)) != nullptr;
      const bool acknowledged = thisModem().waitResponse() == 1;
      if (!complete || !acknowledged) { return false; }

      char* fields[21] = {row};
      size_t count = 1;
      for (char* p = row; *p; ++p) {
        if (*p == ',') {
          if (count == 21) { return false; }
          *p = '\0';
          fields[count++] = p + 1;
        }
      }
      if (count != 18 && count != 21) { return false; }
      for (size_t i = 0; i < count; ++i) {
        while (*fields[i] && this->isIntegerWhitespace(*fields[i])) { ++fields[i]; }
        char* end = fields[i] + strlen(fields[i]);
        while (end > fields[i] && this->isIntegerWhitespace(end[-1])) { --end; }
        *end = '\0';
      }

      const auto readInt = [&](size_t field, int& value) -> bool {
        return this->parseIntegerToken(fields[field], strlen(fields[field]), value);
      };
      const auto readFloat = [&](size_t field, float& value) -> bool {
        return this->parseFloatToken(fields[field], strlen(fields[field]), value);
      };
      int running = 0;
      int fixed = 0;
      if (!readInt(0, running) || !readInt(1, fixed) || running != 1 || fixed != 1) {
        return false;
      }

      // YYYYMMDDhhmmss[.fraction], UTC. Empty timestamps are valid without a
      // fix, but a successful position must include a complete valid date.
      char* utc = fields[2];
      const size_t utcLength = strlen(utc);
      if (utcLength < 14) { return false; }
      for (size_t i = 0; i < 14; ++i) {
        if (utc[i] < '0' || utc[i] > '9') { return false; }
      }
      if (utcLength > 14) {
        if (utc[14] != '.' || utcLength == 15) { return false; }
        for (size_t i = 15; i < utcLength; ++i) {
          if (utc[i] < '0' || utc[i] > '9') { return false; }
        }
      }

      int iyear = 0, imonth = 0, iday = 0, ihour = 0, imin = 0;
      float secondWithSS = 0;
      if (!this->parseIntegerToken(utc, 4, iyear) ||
          !this->parseIntegerToken(utc + 4, 2, imonth) ||
          !this->parseIntegerToken(utc + 6, 2, iday) ||
          !this->parseIntegerToken(utc + 8, 2, ihour) ||
          !this->parseIntegerToken(utc + 10, 2, imin) ||
          !this->parseFloatToken(utc + 12, utcLength - 12, secondWithSS)) {
        return false;
      }
      if (iyear < 1980 || imonth < 1 || imonth > 12 || iday < 1 ||
          ihour > 23 || imin > 59 || secondWithSS < 0 || secondWithSS >= 61) {
        return false;
      }
      const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      const bool leapYear = iyear % 4 == 0 && (iyear % 100 != 0 || iyear % 400 == 0);
      const int maxDay = daysInMonth[imonth - 1] + (imonth == 2 && leapYear ? 1 : 0);
      if (iday > maxDay) { return false; }

      float ilat = 0, ilon = 0, ialt = 0, ispeed = 0, iaccuracy = 0;
      int ivsat = 0, iusat = 0;
      if (!readFloat(3, ilat) || !readFloat(4, ilon) || !readFloat(5, ialt) ||
          !readFloat(6, ispeed) || !readFloat(10, iaccuracy) || !readInt(14, ivsat) ||
          ilat < -90 || ilat > 90 || ilon < -180 || ilon > 180 ||
          ispeed < 0 || ispeed > 999.99f || iaccuracy < 0 || iaccuracy > 99.9f ||
          ivsat < 0 || ivsat > 99) {
        return false;
      }
      // The short 18-field layout has no satellites-used value. Its field 16
      // is reserved, not USAT; return 0 for that unavailable measurement.
      if (count == 21 && (!readInt(15, iusat) || iusat < 0 || iusat > 99)) {
        return false;
      }

      // Publish only after a complete row, final OK and all required values.
      if (lat != nullptr) *lat = ilat;
      if (lon != nullptr) *lon = ilon;
      if (speed != nullptr) *speed = ispeed;  // Speed over ground, km/h.
      if (alt != nullptr) *alt = ialt;
      if (vsat != nullptr) *vsat = ivsat;
      if (usat != nullptr) *usat = iusat;
      if (accuracy != nullptr) *accuracy = iaccuracy;  // HDOP, not meters.
      if (year != nullptr) *year = iyear;
      if (month != nullptr) *month = imonth;
      if (day != nullptr) *day = iday;
      if (hour != nullptr) *hour = ihour;
      if (minute != nullptr) *minute = imin;
      if (second != nullptr) *second = static_cast<int>(secondWithSS);

      return true;
    }();

    MS_TINY_GSM_SEM_GIVE_WAIT
    return success;
  }
  bool handleURCs(String& data) {
    return thisModem().handleURCs(data);
  }

 public:

  // <MS>
  /**
  // @brief       
  // @return      
  // @note        
  **/
  bool getUserEquipmentSystemInfo(MsUserEquipmentSystemInfo& si) {
    bool ret = false;
    // 17.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 4e
    // Publish the new snapshot only after all bounded text fields and OK arrive.
    MsUserEquipmentSystemInfo pending{};
    bool responsePending = false;

    MS_TINY_GSM_SEM_TAKE_WAIT

    thisModem().sendAT(GF("+CPSI?"));
    if (thisModem().waitResponse(GF(AT_NL "+CPSI: ")) != 1) goto end;
    responsePending = true;
    if (!thisModem().streamGetCharBefore(',', pending.systemMode, sizeof(pending.systemMode))) goto end;
    if (strcmp(pending.systemMode, "GSM") == 0) {
      DBGLOG(Error, "[TinyGsmSim70xx] Only 'LTE' is supported, but 'GSM' was found!")
      goto end;
    }
    if (strcmp(pending.systemMode, "NO SERVICE") == 0) {
      DBGLOG(Warn, "[TinyGsmSim70xx] 'NO SERVICE' was found!")
      goto end;
    }
    // Only read data concerning "LTE"
    if (!thisModem().streamGetCharBefore(',', pending.operationMode, sizeof(pending.operationMode))) goto end;
    if (!thisModem().streamGetCharBefore(',', pending.MCC_MNC, sizeof(pending.MCC_MNC))) goto end;
    if (!thisModem().streamGetCharBefore(',', pending.TAC, sizeof(pending.TAC))) goto end;
    if (!thisModem().streamGetCharBefore(',', pending.sCellID, sizeof(pending.sCellID))) goto end;
    if (!thisModem().streamGetCharBefore(',', pending.pCellID, sizeof(pending.pCellID))) goto end;
    if (!thisModem().streamGetCharBefore(',', pending.frequencyBand, sizeof(pending.frequencyBand))) goto end;
    pending.earfcn = thisModem().streamGetIntegerBefore(',');
    pending.dlbw = thisModem().streamGetIntegerBefore(',');
    pending.ulbw = thisModem().streamGetIntegerBefore(',');
    pending.RSRQ = thisModem().streamGetIntegerBefore(',');
    pending.RSRP = thisModem().streamGetIntegerBefore(',');
    pending.RSSI = thisModem().streamGetIntegerBefore(',');
    pending.RSSNR = thisModem().streamGetIntegerBefore('\n');
    ret = thisModem().waitResponse() == 1;
    responsePending = false;
    if (!ret) goto end;

    si = pending;
    ret = true;

end:
    if (responsePending) thisModem().waitResponse();
    MS_TINY_GSM_SEM_GIVE_WAIT
    
    return ret;
  } // TinyGsmSim70xx.getUserEquipmentSystemInfo

  // <MS>
  /**
  // @brief       
  // @return      
  // @note        
  **/
  bool getNetworkAPN(char* apn, size_t maxlen) {
    // 17.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 4e
    if (!apn || maxlen == 0) { return false; }
    bool ret = false;
    bool responsePending = false;
    apn[0] = 0;

    MS_TINY_GSM_SEM_TAKE_WAIT

    thisModem().sendAT(GF("+CGNAPN"));
    if (thisModem().waitResponse(GF(AT_NL "+CGNAPN: ")) != 1) goto end;
    responsePending = true;
    if (thisModem().streamGetIntegerBefore(',') != 1) goto end;
    if (!thisModem().streamSkipUntil('"')) goto end;
    if (!thisModem().streamGetCharBefore('"', apn, maxlen)) goto end;
    ret = thisModem().waitResponse() == 1;
    responsePending = false;

end:
    if (responsePending) thisModem().waitResponse();
    if (!ret) apn[0] = '\0';
    MS_TINY_GSM_SEM_GIVE_WAIT
    
    return ret;
  } // TinyGsmSim70xx.getNetworkAPN

  // <MS>
  /**
  // @brief       Logs actual settings of AT+SLEDS, AT+CNETLIGHT, AT+CSGS.
  // @return      true = okay
  // @note        Details see latest "SIM7070_SIM7080_SIM7090 Series_AT Command Manual".
  **/
  bool reportNetlightStatus() {
    String s;

    MS_TINY_GSM_SEM_TAKE_WAIT

    DBGLOG(Info, "[TinyGsmSim70xx] AT+SLEDS=?")
    thisModem().sendAT(GF("+SLEDS=?"));
    thisModem().waitResponse(2000L, s);
    DBGLOG(Info, "[TinyGsmSim70xx] +SLEDS=?: %s", s.c_str());

    DBGLOG(Info, "[TinyGsmSim70xx] AT+SLEDS?")
    thisModem().sendAT(GF("+SLEDS?"));
    thisModem().waitResponse(2000L, s);
    DBGLOG(Info, "[TinyGsmSim70xx] +SLEDS?: %s", s.c_str());

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CNETLIGHT=?")
    thisModem().sendAT(GF("+CNETLIGHT=?"));
    thisModem().waitResponse(2000L, s);
    DBGLOG(Info, "[TinyGsmSim70xx] +CNETLIGHT=?: %s", s.c_str());

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CNETLIGHT?")
    thisModem().sendAT(GF("+CNETLIGHT?"));
    thisModem().waitResponse(2000L, s);
    DBGLOG(Info, "[TinyGsmSim70xx] +CNETLIGHT?: %s", s.c_str());

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CSGS=?")
    thisModem().sendAT(GF("+CSGS=?"));
    thisModem().waitResponse(2000L, s);
    DBGLOG(Info, "[TinyGsmSim70xx] +CSGS=?: %s", s.c_str());

    DBGLOG(Info, "[TinyGsmSim70xx] AT+CSGS?")
    thisModem().sendAT(GF("+CSGS?"));
    thisModem().waitResponse(2000L, s);
    DBGLOG(Info, "[TinyGsmSim70xx] +CSGS?: %s", s.c_str());

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
  
    MS_TINY_GSM_SEM_TAKE_WAIT

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
  
    MS_TINY_GSM_SEM_TAKE_WAIT

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
  
    MS_TINY_GSM_SEM_TAKE_WAIT

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
    String res;
  
    MS_TINY_GSM_SEM_TAKE_WAIT

    thisModem().sendAT(GF("+CGMR"));
    if (thisModem().waitResponse(1000L, res) != 1) res = ""; 

    MS_TINY_GSM_SEM_GIVE_WAIT
    
    if (res != "") {
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
    } // if (res != "")

    return res;
  } // TinyGsmSim70xx.getModemRevisionSoftwareRelease


 public:
  Stream& stream;
};

#endif  // SRC_TINYGSMCLIENTSIM70XX_H_
