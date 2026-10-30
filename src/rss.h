#pragma once

#include <Arduino.h>
#include "rss_parser.h"

class classRss {
public:
    static constexpr size_t MAX_ITEMS = RssParser::MAX_ITEMS;
    static const char* const kDefaultCategory;
    static const char* const kCategories[];
    static const size_t kCategoryCount;

    void begin();
    void setCategory(const String& category);
    const String& category() const { return _category; }
    static bool isValidCategory(const String& category);
    static String normalizeCategory(const String& category);

    bool refresh();
    size_t itemCount() const { return _itemCount; }
    const String& item(size_t index) const;
    unsigned long lastFetchMillis() const { return _lastFetchMillis; }
    bool hasData() const { return _itemCount > 0; }

private:
    static String buildUrl(const String& category);
    static String cleanupField(const String& input);

    String _category;
    uint32_t _categoryRevision = 0;
    String _items[MAX_ITEMS];
    size_t _itemCount = 0;
    unsigned long _lastFetchMillis = 0;
    SemaphoreHandle_t _mutex = NULL;

public:
    bool lock(TickType_t wait = portMAX_DELAY) {
        if (!_mutex) return false; // mutex not initialized — refuse to proceed unprotected
        return xSemaphoreTake(_mutex, wait) == pdTRUE;
    }
    void unlock() {
        if (_mutex) xSemaphoreGive(_mutex);
    }
};
