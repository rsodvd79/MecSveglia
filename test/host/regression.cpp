#include "rss.h"
#include "meteo.h"
#include "eyes.h"
#include "network_io.h"
#include "refresh_schedule.h"
#include "web_template.h"
#include "wifi_credentials.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

unsigned long fakeMillis = 0;
FakeResponse fakeResponse;

static void response(const std::string &body) {
  fakeResponse = FakeResponse{};
  fakeResponse.body = body;
}

static RssParser parse(const std::string &xml) {
  RssParser parser;
  for (char c : xml) parser.push(c);
  return parser;
}

static void parserAndCache() {
  auto parser = parse("<?xml version='1.0'?><rss><channel>"
      "<!-- <item><title>ignore</title></item> -->"
      "<item><title><![CDATA[one > two < three]]></title></item>"
      "<item><title attr='>'>four</title></item></channel></rss>");
  assert(parser.valid() && parser.count() == 2);
  assert(std::string(parser.title(0)) == "one > two < three");
  assert(std::string(parser.title(1)) == "four");
  assert(!parse("<rss><item><title>unfinished").valid());
  assert(!parse("<rss><item><title>ok</title></item>").valid());
  assert(!parse("<rss><item><title>bad</item></rss>").valid());
  assert(!parse("<rss></rss>").valid());
  parser = parse("<rss><item/><item><title /></item>"
                 "<item><title>ok</title></item></rss>");
  assert(parser.valid() && parser.count() == 1);
  assert(std::string(parser.title(0)) == "ok");
  parser = parse("<rss><item><title>" + std::string(100000, 'x') +
                 "</title></item></rss>");
  assert(parser.valid() && parser.truncated(0));
  assert(std::strlen(parser.title(0)) == RssParser::TITLE_CAPACITY);
  parser = parse("<rss><item><description>" + std::string(100000, 'x') +
                 "</description><title>ok</title></item></rss>");
  assert(parser.valid() && std::string(parser.title(0)) == "ok");
  parser = parse("<rss><" + std::string(100000, 'x') +
                 "><item><title>ok</title></item></rss>");
  assert(parser.valid() && std::string(parser.title(0)) == "ok");
  std::string manyItems = "<rss>";
  for (size_t i = 0; i < 6; ++i) {
    manyItems += "<item><title>ok</title></item>";
  }
  parser = parse(manyItems);
  assert(parser.valid() && parser.count() == RssParser::MAX_ITEMS);

  classRss rss;
  rss.begin();
  response("<rss><item><title>A &amp; B</title></item></rss>");
  assert(rss.refresh());
  assert(rss.itemCount() == 1 && rss.item(0) == "A & B");
  const unsigned long stamp = rss.lastFetchMillis();
  response("<rss><item><title>partial");
  assert(!rss.refresh());
  assert(rss.item(0) == "A & B" && rss.lastFetchMillis() == stamp);
  response("<rss></rss>");
  assert(!rss.refresh() && rss.item(0) == "A & B");
  response("<rss><item><title>" + std::string(10000, 'x') +
           "</title></item></rss>");
  assert(rss.refresh() && rss.item(0).length() == 96);
  assert(rss.item(0).substring(93, 96) == "...");

  response("<rss><item><title>old category</title></item></rss>");
  fakeResponse.duringGet = [&rss]() {
    assert(rss.lock(0));
    rss.setCategory("sport");
    rss.setCategory(classRss::kDefaultCategory); // A -> B -> A.
    rss.unlock();
  };
  assert(!rss.refresh() && !rss.hasData());
  response("<rss><item><title>new category</title></item></rss>");
  assert(rss.refresh() && rss.item(0) == "new category");
  response("chunk framing must never reach the parser");
  fakeResponse.encoding = "chunked";
  assert(!rss.refresh() && rss.item(0) == "new category");
  fakeMillis = 0;
  response("<rss><item><title>" + std::string(20000, 'x') +
           "</title></item></rss>");
  fakeResponse.byteInterval = 1;
  assert(!rss.refresh() && rss.item(0) == "new category");
  assert(millis() == 15000);
}

static void deadlinesAndWeather() {
  fakeMillis = UINT32_MAX - 1000UL;
  response(std::string(100, 'x'));
  fakeResponse.byteInterval = 1000;
  HTTPClient http;
  FeedReader reader(http);
  unsigned int count = 0;
  while (reader.read() >= 0) ++count;
  assert(count <= 15);
  assert(static_cast<uint32_t>(millis() - (UINT32_MAX - 1000UL)) >= 15000);

  fakeMillis = 0;
  response("");
  fakeResponse.keepOpen = true;
  HTTPClient idleHttp;
  FeedReader idleReader(idleHttp);
  assert(idleReader.read() == -1 && millis() == 5000);

  classMeteo meteo;
  const std::string valid = R"({"main":{"temp":21.5,"humidity":42},)"
      R"("wind":{"speed":2},"weather":[{"description":"sereno","icon":"01d"}]})";
  response(valid);
  assert(meteo.Update());
  assert(meteo.temp == 21.5f && meteo.humidity == 42);
  assert(meteo.description == "SERENO" && meteo.icon == "01D");
  response("{}");
  assert(!meteo.Update() && meteo.temp == 21.5f && meteo.icon == "01D");
  response(valid.substr(0, valid.length() - 1));
  assert(!meteo.Update() && meteo.temp == 21.5f);
  response(valid);
  fakeResponse.code = 301;
  assert(!meteo.Update() && meteo.temp == 21.5f);
  response(valid);
  fakeResponse.encoding = "chunked";
  assert(!meteo.Update() && meteo.temp == 21.5f);
  response(R"({"main":{"temp":1,"humidity":200},"wind":{"speed":2},)"
           R"("weather":[{"description":"bad","icon":"01d"}]})");
  assert(!meteo.Update() && meteo.humidity == 42);
  response(R"({"main":{"temp":1,"humidity":42},"wind":{"speed":2},)"
           R"("weather":[{"description":")" + std::string(5000, 'x') +
           R"(","icon":"01d"}]})");
  assert(!meteo.Update() && meteo.temp == 21.5f);
}

static void schedulingAndEyes() {
  RefreshSchedule rss(3600000);
  RefreshSchedule meteo(1800000);
  uint32_t now = UINT32_MAX - 100;
  assert(rss.due(now));
  rss.completed(now, false);
  assert(!rss.due(now + 59999) && rss.due(now + 60000));
  assert(rss.due(now, true));
  rss.completed(now + 60000, true);
  assert(!rss.due(now + 3659999) && rss.due(now + 3660000));
  unsigned int rssFetches = 0, meteoFetches = 0;
  rss = RefreshSchedule(3600000);
  for (now = 0; now < 2000; now += 1000) {
    if (rss.due(now)) {
      ++rssFetches;
      rss.completed(now, true);
    } else if (meteo.due(now)) {
      ++meteoFetches;
      meteo.completed(now, true);
    }
  }
  assert(rssFetches == 1 && meteoFetches == 1);

  fakeMillis = 0;
  Adafruit_SSD1306 display;
  classEye eye(&display);
  eye.loop();
  assert(display.flushes == 1);
  fakeMillis = 24;
  eye.loop();
  assert(display.flushes == 1);
  fakeMillis = 25;
  eye.loop();
  assert(display.flushes == 2);
  eye.loop();
  assert(display.flushes == 2);
  for (fakeMillis = 26; fakeMillis < 8000; ++fakeMillis) {
    unsigned int previous = display.flushes;
    eye.loop();
    assert(display.flushes - previous <= 1);
  }
}

struct Source {
  std::string data;
  size_t block;
  size_t offset = 0;
  Source(const std::string &text, size_t size) : data(text), block(size) {}
  size_t read(uint8_t *buffer, size_t size) {
    size_t count = std::min({block, size, data.size() - offset});
    std::memcpy(buffer, data.data() + offset, count);
    offset += count;
    return count;
  }
};

static void webAndCredentials() {
  std::ifstream file("data/INDEX.HTM");
  assert(file);
  const std::string page((std::istreambuf_iterator<char>(file)), {});
  const std::string token = "__MDNS__";
  std::string expected = page;
  for (size_t pos = 0; (pos = expected.find(token, pos)) != std::string::npos;) {
    expected.replace(pos, token.size(), "CLOCK");
    pos += 5;
  }
  for (size_t block : {size_t(1), size_t(7), size_t(8), size_t(511), size_t(512)}) {
    for (const std::string &text : {page,
        std::string(509, 'x') + "__MDNS____MDNS__tail__MD"}) {
      Source source(text, block);
      std::string output;
      streamIndexTemplate(source, [&output](const char *bytes, size_t count) {
        assert(count <= 512);
        output.append(bytes, count);
      }, "CLOCK", 5);
      if (text == page) {
        assert(output == expected);
      } else {
        assert(output == std::string(509, 'x') + "CLOCKCLOCKtail__MD");
      }
    }
  }
  const std::string ssid(32, 's'), pwd = " password ";
  assert(validWifiCredentials(ssid.data(), ssid.size(), pwd.data(), pwd.size()));
  assert(validWifiCredentials(ssid.data(), ssid.size(), "", 0));
  assert(!validWifiCredentials("", 0, "", 0));
  assert(!validWifiCredentials(ssid.data(), 33, "", 0));
  assert(!validWifiCredentials("ok", 2, "short", 5));
  const std::string hex(64, 'a'), invalidHex(64, 'z');
  assert(validWifiCredentials("ok", 2, hex.data(), hex.size()));
  assert(!validWifiCredentials("ok", 2, invalidHex.data(), invalidHex.size()));
  assert(credentialTextLength(" ssid \r\n", 8) == 6);
  assert(credentialTextLength(" ssid ", 6) == 6);
}

int main() {
  parserAndCache();
  deadlinesAndWeather();
  schedulingAndEyes();
  webAndCredentials();
  std::cout << "Firmware host regressions passed\n";
}
