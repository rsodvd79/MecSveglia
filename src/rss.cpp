#include "rss.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const char* const classRss::kDefaultCategory = "tecnologia";
const char* const classRss::kCategories[] = {
    "tecnologia",
    "politica",
    "mondo",
    "economia",
    "sport",
    "cultura",
    "scienza",
    "cronaca"
};

struct CategoryFeedOverride { const char* name; const char* url; };
static const CategoryFeedOverride kCategoryFeedOverrides[] = {
    { "cronaca", "https://www.ansa.it/sito/notizie/cronaca/cronaca_rss.xml" },
    { "politica", "https://www.ansa.it/sito/notizie/politica/politica_rss.xml" },
    { "mondo", "https://www.ansa.it/sito/notizie/mondo/mondo_rss.xml" },
    { "economia", "https://www.ansa.it/sito/notizie/economia/economia_rss.xml" },
    { "sport", "https://www.ansa.it/sito/notizie/sport/sport_rss.xml" },
    { "cultura", "https://www.ansa.it/sito/notizie/cultura/cultura_rss.xml" },
    { "scienza", "https://www.ansa.it/canale_scienza_tecnica/notizie/scienzaetecnica_rss.xml" },
    { "tecnologia", "https://www.ansa.it/canale_tecnologia/notizie/tecnologia_rss.xml" }
};
static const size_t kCategoryFeedOverridesCount = sizeof(kCategoryFeedOverrides) / sizeof(kCategoryFeedOverrides[0]);

const size_t classRss::kCategoryCount = sizeof(classRss::kCategories) / sizeof(classRss::kCategories[0]);

void classRss::begin() {
    _category = kDefaultCategory;
    _itemCount = 0;
    _lastFetchMillis = 0;
}

String classRss::normalizeCategory(const String& category) {
    String trimmed = category;
    trimmed.trim();
    trimmed.toLowerCase();
    trimmed.replace(' ', '_');
    return trimmed;
}

bool classRss::isValidCategory(const String& category) {
    String norm = normalizeCategory(category);
    for (size_t i = 0; i < kCategoryCount; ++i) {
        if (norm.equals(kCategories[i])) {
            return true;
        }
    }
    return false;
}

void classRss::setCategory(const String& category) {
    String norm = normalizeCategory(category);
    if (!isValidCategory(norm)) {
        norm = kDefaultCategory;
    }
    _category = norm;
}

bool classRss::refresh() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    String payload;
    if (!fetchFeed(payload)) {
        return false;
    }
    parseFeed(payload);
    _lastFetchMillis = millis();
    return _itemCount > 0;
}

const String& classRss::item(size_t index) const {
    static String empty;
    if (index >= _itemCount) {
        empty = "";
        return empty;
    }
    return _items[index];
}

bool classRss::fetchFeed(String& out) {
    String url = buildUrl(_category);
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, url)) {
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    out = http.getString();
    http.end();
    return true;
}

void classRss::parseFeed(const String& payload) {
    _itemCount = 0;
    size_t pos = 0;
    while (_itemCount < MAX_ITEMS) {
        int itemStart = payload.indexOf("<item", pos);
        if (itemStart < 0) {
            break;
        }
        int titleOpen = payload.indexOf("<title", itemStart);
        if (titleOpen < 0) {
            pos = itemStart + 5;
            continue;
        }
        int titleStart = payload.indexOf('>', titleOpen);
        if (titleStart < 0) {
            break;
        }
        ++titleStart;
        int titleEnd = payload.indexOf("</title>", titleStart);
        if (titleEnd < 0) {
            break;
        }
        String title = payload.substring(titleStart, titleEnd);
        title.replace("<![CDATA[", "");
        title.replace("]]>", "");
        title = cleanupField(title);
        if (!title.length()) {
            pos = titleEnd + 8;
            continue;
        }
        if (title.length() > 96) {
            title = title.substring(0, 93) + "...";
        }
        _items[_itemCount++] = title;
        pos = titleEnd + 8;
    }
}

String classRss::buildUrl(const String& category) {
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

String classRss::cleanupField(const String& input) {
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








