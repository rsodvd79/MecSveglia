from pathlib import Path

# Compile the actual handler bodies without emulating the entire ESP32 setup.
source = Path("src/main.cpp").read_text()
for start, end in [
    ("void handleSetupPost() {", "void handleScreenStartGet()"),
    ("int dBmtoPercentage(", "void showScreen0("),
    ("void time_is_set_scheduled() {", "String normalizeTimezone("),
    ("static void maintainTimeSync() {", "int dBmtoPercentage("),
]:
    print(source[source.index(start):source.index(end, source.index(start))])

for callback, signature in [
    ("onStart", "void simulateOtaStart()"),
    ("onEnd", "void simulateOtaEnd()"),
    ("onProgress", "void simulateOtaProgress(unsigned int progress, unsigned int total)"),
    ("onError", "void simulateOtaError(ota_error_t error)"),
]:
    body = source.split(f"ArduinoOTA.{callback}([](", 1)[1]
    body = body.split("{", 1)[1].split("\n  });", 1)[0]
    print(signature + " {" + body + "\n}")
