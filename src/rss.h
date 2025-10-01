#pragma once

#include <Arduino.h>

class classRss {
public:
    static constexpr size_t MAX_ITEMS = 5;
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
    bool fetchFeed(String& out);
    void parseFeed(const String& payload);
    static String buildUrl(const String& category);
    static String cleanupField(const String& input);

    String _category;
    String _items[MAX_ITEMS];
    size_t _itemCount = 0;
    unsigned long _lastFetchMillis = 0;
};
