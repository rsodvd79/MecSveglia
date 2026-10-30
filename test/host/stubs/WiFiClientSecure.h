#pragma once

#include "HTTPClient.h"

class WiFiClientSecure : public WiFiClient {
public:
  void setInsecure() {}
  void setHandshakeTimeout(int) {}
};
