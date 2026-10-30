#pragma once

#include <stddef.h>
#include <string.h>

inline size_t credentialTextLength(const char *text, size_t length) {
  // TXT files may end in LF/CRLF; spaces are part of the credential.
  if (length && text[length - 1] == '\n') {
    --length;
    if (length && text[length - 1] == '\r') --length;
  }
  return length;
}

inline bool validWifiCredentials(const char *ssid, size_t ssidLength,
                                 const char *password, size_t passwordLength) {
  if (ssidLength == 0 || ssidLength > 32 ||
      memchr(ssid, '\0', ssidLength) ||
      memchr(password, '\0', passwordLength)) {
    return false;
  }
  if (passwordLength == 0) return true;
  if (passwordLength == 64) {
    for (size_t i = 0; i < passwordLength; ++i) {
      char c = password[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
  }
  return passwordLength >= 8 && passwordLength <= 63;
}
