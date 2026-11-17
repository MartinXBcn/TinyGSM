/**
 * @file       TinyGsmSMS.tpp
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMSMS_H_
#define SRC_TINYGSMSMS_H_

#include "TinyGsmCommon.h"
#include <limits.h>

#define TINY_GSM_MODEM_HAS_SMS

template <class modemType>
class TinyGsmSMS {
  /* =========================================== */
  /* =========================================== */
  /*
   * Define the interface
   */
 public:
  /*
   * Text messaging (SMS) functions
   */
  String sendUSSD(const String& code) {
    return thisModem().sendUSSDImpl(code);
  }
  bool sendSMS(const String& number, const String& text) {
    return thisModem().sendSMSImpl(number, text);
  }
  bool sendSMS_UTF16(const char* const number, const void* text, size_t len) {
    return thisModem().sendSMS_UTF16Impl(number, text, len);
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
  ~TinyGsmSMS() {}

  /*
   * Utilities
   */
 protected:
  // 17.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 4d
  // Reject incomplete units and invalid nibbles before producing any output.
  // size_t indices also avoid wrapping on responses longer than 255/65535 chars.
  static inline int8_t TinyGsmHexCharToNibble(char c) {
    if (c >= '0' && c <= '9') { return static_cast<int8_t>(c - '0'); }
    if (c >= 'a' && c <= 'f') { return static_cast<int8_t>(c - 'a' + 10); }
    if (c >= 'A' && c <= 'F') { return static_cast<int8_t>(c - 'A' + 10); }
    return -1;
  }

  static inline bool TinyGsmIsValidHex(const String& instr, size_t unitChars) {
    const size_t length = instr.length();
    if (!unitChars || length % unitChars != 0) { return false; }
    for (size_t i = 0; i < length; ++i) {
      if (TinyGsmHexCharToNibble(instr[i]) < 0) { return false; }
    }
    return true;
  }

  // Used only after validating the complete input, so both nibbles exist.
  static inline uint8_t TinyGsmHexByte(const String& instr, size_t offset) {
    return static_cast<uint8_t>((TinyGsmHexCharToNibble(instr[offset]) << 4) |
        TinyGsmHexCharToNibble(instr[offset + 1]));
  }

  static inline String TinyGsmDecodeHex7bit(String& instr) {
    String result;
    if (!TinyGsmIsValidHex(instr, 2)) { return result; }
    const size_t bytes = instr.length() / 2;
    if (!bytes) { return result; }
    // Keep the existing packed-septet/padding behavior; no septet count is
    // supplied by this API, so a final padding character cannot be inferred.
    const size_t outputLength = bytes + bytes / 7;
    if (!result.reserve(static_cast<unsigned int>(outputLength))) { return ""; }
    byte   reminder = 0;
    int8_t bitstate = 7;
    for (size_t i = 0; i < instr.length(); i += 2) {
      const byte b = TinyGsmHexByte(instr, i);

      byte bb = b << (7 - bitstate);
      char c  = (bb + reminder) & 0x7F;
      if (!result.concat(c)) { return ""; }
      reminder = b >> bitstate;
      bitstate--;
      if (bitstate == 0) {
        char cc = reminder;
        if (!result.concat(cc)) { return ""; }
        reminder = 0;
        bitstate = 7;
      }
    }
    return result;
  }

  static inline String TinyGsmDecodeHex8bit(String& instr) {
    String result;
    if (!TinyGsmIsValidHex(instr, 2)) { return result; }
    if (!instr.length()) { return result; }
    if (!result.reserve(instr.length() / 2)) { return ""; }
    for (size_t i = 0; i < instr.length(); i += 2) {
      if (!result.concat(static_cast<char>(TinyGsmHexByte(instr, i)))) { return ""; }
    }
    return result;
  }

  static inline String TinyGsmDecodeHex16bit(String& instr) {
    String result;
    if (!TinyGsmIsValidHex(instr, 4)) { return result; }
    if (!instr.length()) { return result; }
    size_t outputLength = 0;
    for (size_t i = 0; i < instr.length(); i += 4) {
#if defined(TINY_GSM_UNICODE_TO_HEX)
      const size_t needed = TinyGsmHexByte(instr, i) ? 6 : 1;
#else
      const size_t needed = 1;
#endif
      if (outputLength > UINT_MAX - needed) { return ""; }
      outputLength += needed;
    }
    if (!result.reserve(static_cast<unsigned int>(outputLength))) { return ""; }
    for (size_t i = 0; i < instr.length(); i += 4) {
      const byte b = TinyGsmHexByte(instr, i);
      if (b) {  // If high byte is non-zero, we can't handle it ;(
#if defined(TINY_GSM_UNICODE_TO_HEX)
        if (!result.concat("\\x")) { return ""; }
        for (size_t j = 0; j < 4; ++j) {
          if (!result.concat(instr[i + j])) { return ""; }
        }
#else
        if (!result.concat('?')) { return ""; }
#endif
      } else {
        if (!result.concat(static_cast<char>(TinyGsmHexByte(instr, i + 2)))) {
          return "";
        }
      }
    }
    return result;
  }

  /* =========================================== */
  /* =========================================== */
  /*
   * Define the default function implementations
   */

  /*
   * Text messaging (SMS) functions
   */

  String sendUSSDImpl(const String& code) {
    // Set preferred message format to text mode
    thisModem().sendAT(GF("+CMGF=1"));
    thisModem().waitResponse();
    // Set 8-bit hexadecimal alphabet (3GPP TS 23.038)
    thisModem().sendAT(GF("+CSCS=\"HEX\""));
    thisModem().waitResponse();
    // Send the message
    thisModem().sendAT(GF("+CUSD=1,\""), code, GF("\""));
    if (thisModem().waitResponse() != 1) { return ""; }
    if (thisModem().waitResponse(10000L, GF("+CUSD:")) != 1) { return ""; }
    thisModem().stream.readStringUntil('"');
    String hex = thisModem().stream.readStringUntil('"');
    thisModem().stream.readStringUntil(',');
    // 17.11.2026 - Audit vs. TinyGSM 0.13.0-rc.1 - 4d
    // Keep the parsed value wide until the DCS octet has been validated.
    const int dcs = thisModem().streamGetIntBefore('\n');
    if (dcs < 0 || dcs > 255) { return ""; }

    if (dcs == 15) {
      return TinyGsmDecodeHex8bit(hex);
    } else if (dcs == 72) {
      return TinyGsmDecodeHex16bit(hex);
    } else {
      return hex;
    }
  }

  bool sendSMSImpl(const String& number, const String& text) {
    // Set preferred message format to text mode
    thisModem().sendAT(GF("+CMGF=1"));
    thisModem().waitResponse();
    // Set GSM 7 bit default alphabet (3GPP TS 23.038)
    thisModem().sendAT(GF("+CSCS=\"GSM\""));
    thisModem().waitResponse();
    thisModem().sendAT(GF("+CMGS=\""), number, GF("\""));
    if (thisModem().waitResponse(GF(">")) != 1) { return false; }
    thisModem().stream.print(text);  // Actually send the message
    thisModem().stream.write(static_cast<char>(0x1A));  // Terminate the message
    thisModem().stream.flush();
    return thisModem().waitResponse(60000L) == 1;
  }

  // Common methods for UTF8/UTF16 SMS.
  // Supported by: BG96, M95, MC60, SIM5360, SIM7000, SIM7600, SIM800
  class UTF8Print : public Print {
   public:
    explicit UTF8Print(Print& _p) : p(_p) {}
    size_t write(const uint8_t c) override {
      if (prv < 0xC0) {
        if (c < 0xC0) printHex(c);
        prv = c;
      } else {
        uint16_t v = uint16_t(prv) << 8 | c;
        v -= (v >> 8 == 0xD0) ? 0xCC80 : 0xCD40;
        printHex(v);
        prv = 0;
      }
      return 1;
    }

   private:
    Print&  p;
    uint8_t prv = 0;
    void    printHex(const uint16_t v) {
         uint8_t c = v >> 8;
         if (c < 0x10) p.print('0');
      p.print(c, HEX);
         c = v & 0xFF;
         if (c < 0x10) p.print('0');
      p.print(c, HEX);
    }
  };

  bool sendSMS_UTF8_begin(const char* const number) {
    thisModem().sendAT(GF("+CMGF=1"));
    thisModem().waitResponse();
    thisModem().sendAT(GF("+CSCS=\"HEX\""));
    thisModem().waitResponse();
    thisModem().sendAT(GF("+CSMP=17,167,0,8"));
    thisModem().waitResponse();

    thisModem().sendAT(GF("+CMGS=\""), number, GF("\""));
    return thisModem().waitResponse(GF(">")) == 1;
  }
  bool sendSMS_UTF8_end() {
    thisModem().stream.write(static_cast<char>(0x1A));
    thisModem().stream.flush();
    return thisModem().waitResponse(60000L) == 1;
  }
  UTF8Print sendSMS_UTF8_stream() {
    return UTF8Print(thisModem().stream);
  }

  bool sendSMS_UTF16Impl(const char* const number, const void* text,
                         size_t len) {
    if (!sendSMS_UTF8_begin(number)) { return false; }

    uint16_t* t =
        const_cast<uint16_t*>(reinterpret_cast<const uint16_t*>(text));
    for (size_t i = 0; i < len; i++) {
      uint8_t c = t[i] >> 8;
      if (c < 0x10) { thisModem().stream.print('0'); }
      thisModem().stream.print(c, HEX);
      c = t[i] & 0xFF;
      if (c < 0x10) { thisModem().stream.print('0'); }
      thisModem().stream.print(c, HEX);
    }

    return sendSMS_UTF8_end();
  }
};

#endif  // SRC_TINYGSMSMS_H_
