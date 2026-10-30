#include "rss.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include "network_io.h"

const char *const classRss::kDefaultCategory = "tecnologia";
const char *const classRss::kCategories[] = {
    "tecnologia", "politica", "mondo",   "economia",
    "sport",      "cultura",  "scienza", "cronaca"};

struct CategoryFeedOverride {
  const char *name;
  const char *url;
};
static const CategoryFeedOverride kCategoryFeedOverrides[] = {
    {"cronaca", "https://www.ansa.it/sito/notizie/cronaca/cronaca_rss.xml"},
    {"politica", "https://www.ansa.it/sito/notizie/politica/politica_rss.xml"},
    {"mondo", "https://www.ansa.it/sito/notizie/mondo/mondo_rss.xml"},
    {"economia", "https://www.ansa.it/sito/notizie/economia/economia_rss.xml"},
    {"sport", "https://www.ansa.it/sito/notizie/sport/sport_rss.xml"},
    {"cultura", "https://www.ansa.it/sito/notizie/cultura/cultura_rss.xml"},
    {"scienza", "https://www.ansa.it/canale_scienza_tecnica/notizie/"
                "scienzaetecnica_rss.xml"},
    {"tecnologia",
     "https://www.ansa.it/canale_tecnologia/notizie/tecnologia_rss.xml"}};
static const size_t kCategoryFeedOverridesCount =
    sizeof(kCategoryFeedOverrides) / sizeof(kCategoryFeedOverrides[0]);

const size_t classRss::kCategoryCount =
    sizeof(classRss::kCategories) / sizeof(classRss::kCategories[0]);

void classRss::begin() {
  _category = kDefaultCategory;
  _itemCount = 0;
  _lastFetchMillis = 0;
  if (!_mutex) {
    _mutex = xSemaphoreCreateMutex();
  }
}

bool classRss::refresh() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  // Use a local client to stream
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(5);
  HTTPClient http;

  String url;
  uint32_t revision;
  // Lock to safely read category
  if (lock()) {
    url = buildUrl(_category);
    revision = _categoryRevision;
    unlock();
  } else {
    return false;
  }

  if (!http.begin(client, url)) {
    return false;
  }

  configureFeedRequest(http);
  int code = http.GET();
  if (code != HTTP_CODE_OK || !feedResponseIsReadable(http)) {
    http.end();
    return false;
  }
  esp_task_wdt_reset();

  FeedReader reader(http);
  RssParser parser;
  int c;
  while (!parser.done() && (c = reader.read()) >= 0) {
    parser.push(static_cast<char>(c));
  }
  http.end();
  if (!parser.valid()) {
    return false;
  }

  String localItems[MAX_ITEMS];
  size_t localCount = 0;
  for (size_t i = 0; i < parser.count(); ++i) {
    String title = cleanupField(parser.title(i));
    if (title.isEmpty()) continue;
    if (title.length() > 96 || parser.truncated(i)) {
      title = title.substring(0, 93) + "...";
    }
    localItems[localCount++] = title;
  }
  if (localCount == 0) return false;

  // Lock only to commit parsed results to shared state
  if (!lock()) {
    return false;
  }
  if (revision != _categoryRevision) {
    unlock();
    return false;
  }
  _itemCount = localCount;
  for (size_t i = 0; i < localCount; i++) {
    _items[i] = localItems[i];
  }
  _lastFetchMillis = millis();
  unlock();
  return localCount > 0;
}

String classRss::buildUrl(const String &category) {
  for (size_t i = 0; i < kCategoryFeedOverridesCount; ++i) {
    if (category.equalsIgnoreCase(kCategoryFeedOverrides[i].name)) {
      return String(kCategoryFeedOverrides[i].url);
    }
  }
  String url = F("https://www.ansa.it/sito/notizie/");
  url += category;
  url += '/';
  url += category;
  url += F("_rss.xml");
  return url;
}

String classRss::cleanupField(const String &input) {
  String out = input;
  // Strip possible CDATA markers that may leak through the stream parser
  out.replace("]]>", " ");
  out.replace("<![CDATA[", "");
  out.replace("]]", " ");
  out.replace("&amp;", "&");
  out.replace("&apos;", "'");
  out.replace("&quot;", "\"");
  out.replace("&lt;", "<");
  out.replace("&gt;", ">");
  while (true) {
    int tagStart = out.indexOf('<');
    if (tagStart < 0) {
      break;
    }
    int tagEnd = out.indexOf('>', tagStart);
    if (tagEnd < 0) {
      break;
    }
    out.remove(tagStart, (tagEnd - tagStart) + 1);
  }
  out.replace('\r', ' ');
  out.replace('\n', ' ');
  while (out.indexOf("  ") >= 0) {
    out.replace("  ", " ");
  }
  out.trim();
  return out;
}

void classRss::setCategory(const String &category) {
  if (isValidCategory(category)) {
    String normalized = normalizeCategory(category);
    if (_category != normalized) {
      _category = normalized;
      ++_categoryRevision;
      _itemCount = 0;
      _lastFetchMillis = 0;
    }
  }
}

bool classRss::isValidCategory(const String &category) {
  for (size_t i = 0; i < kCategoryCount; ++i) {
    if (category.equalsIgnoreCase(kCategories[i])) {
      return true;
    }
  }
  return false;
}

String classRss::normalizeCategory(const String &category) {
  for (size_t i = 0; i < kCategoryCount; ++i) {
    if (category.equalsIgnoreCase(kCategories[i])) {
      return String(kCategories[i]);
    }
  }
  return String(kDefaultCategory);
}

const String &classRss::item(size_t index) const {
  if (index < _itemCount) {
    return _items[index];
  }
  static const String empty = "";
  return empty;
}
