#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

template <typename Source, typename Send>
void streamIndexTemplate(Source &source, Send send, const char *name,
                         size_t nameLength) {
  const char token[] = "__MDNS__";
  const size_t tokenLength = sizeof(token) - 1;
  uint8_t buffer[512];
  size_t pending = 0;
  for (;;) {
    size_t read = source.read(buffer + pending, sizeof(buffer) - pending);
    if (read == 0) break;
    size_t used = pending + read;
    size_t start = 0;
    size_t scan = 0;
    while (scan + tokenLength <= used) {
      if (memcmp(buffer + scan, token, tokenLength) == 0) {
        if (scan > start) {
          send(reinterpret_cast<const char *>(buffer + start), scan - start);
        }
        if (nameLength) send(name, nameLength);
        scan += tokenLength;
        start = scan;
      } else {
        ++scan;
      }
    }
    if (scan > start) {
      send(reinterpret_cast<const char *>(buffer + start), scan - start);
    }
    pending = used - scan;
    memmove(buffer, buffer + scan, pending);
  }
  if (pending) send(reinterpret_cast<const char *>(buffer), pending);
}
