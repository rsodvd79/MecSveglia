#include "rss.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>


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
  HTTPClient http;

  String url = buildUrl(_category);
  if (!http.begin(client, url)) {
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  // Lock before modifying shared data
  if (!lock()) {
    http.end();
    return false;
  }

  // Stream parsing
  _itemCount = 0;
  WiFiClient &stream = http.getStream();

  // Simple state machine for XML parsing to avoid loading full string
  // We look for <item> ... <title> ... </title> ... </item>
  // This is a simplified parser for memory efficiency

  String currentTag = "";
  String title = "";
  bool insideItem = false;
  bool insideTitle = false;

  while (http.connected() && (stream.available() || stream.connected())) {
    if (!stream.available()) {
      delay(1);
      continue;
    }

    // Read until next tag start
    String content = stream.readStringUntil('<');
    if (insideTitle && insideItem) {
      title += content;
    }

    if (stream.peek() != '/') {
      // Opening tag
      String tag = stream.readStringUntil('>');
      // Handle CDATA
      if (tag.startsWith("![CDATA[")) {
        if (insideTitle && insideItem) {
          // Extract CDATA content
          int cdataEnd = tag.indexOf("]]");
          if (cdataEnd >= 0) {
            title += tag.substring(8, cdataEnd);
          } else {
            title += tag.substring(8);
            // If CDATA is split, we might need more logic, but for now assume
            // it's small enough or we catch the rest
          }
        }
      } else {
        // Normal tag
        // Remove attributes if any
        int space = tag.indexOf(' ');
        if (space > 0)
          tag = tag.substring(0, space);

        if (tag.equalsIgnoreCase("item")) {
          insideItem = true;
          title = "";
        } else if (tag.equalsIgnoreCase("title") && insideItem) {
          insideTitle = true;
          title = "";
        }
      }
    } else {
      // Closing tag
      stream.read(); // consume '/'
      String tag = stream.readStringUntil('>');

      if (tag.equalsIgnoreCase("title") && insideItem) {
        insideTitle = false;
        // Cleanup and store
        title = cleanupField(title);
        if (title.length() > 0) {
          if (title.length() > 96) {
            title = title.substring(0, 93) + "...";
          }
          if (_itemCount < MAX_ITEMS) {
            _items[_itemCount++] = title;
          }
        }
      } else if (tag.equalsIgnoreCase("item")) {
        insideItem = false;
        if (_itemCount >= MAX_ITEMS)
          break;
      }
    }
  }

  http.end();
  _lastFetchMillis = millis();
  unlock();
  return _itemCount > 0;
}

// fetchFeed and parseFeed are merged/replaced by the stream logic in refresh
bool classRss::fetchFeed(String &out) { return false; }
void classRss::parseFeed(const String &payload) {}

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
    _category = normalizeCategory(category);
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
