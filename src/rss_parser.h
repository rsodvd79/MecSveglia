#pragma once

#include <stddef.h>
#include <string.h>

class RssParser {
public:
  static constexpr size_t MAX_ITEMS = 5;
  static constexpr size_t TITLE_CAPACITY = 384;

  void push(char c) {
    if (done_ || invalid_) {
      return;
    }
    switch (state_) {
    case Text:
      if (c == '<') {
        state_ = Tag;
        tagLength_ = 0;
        tagEnded_ = false;
        selfClosing_ = false;
        quote_ = 0;
      } else {
        append(c);
      }
      break;
    case Tag:
      if (!quote_ && c != '>' && c != ' ' && c != '\t' &&
          c != '\r' && c != '\n') {
        selfClosing_ = c == '/';
      }
      if (quote_) {
        if (c == quote_) quote_ = 0;
      } else if (c == '"' || c == '\'') {
        quote_ = c;
      } else if (c == '>') {
        tag_[tagLength_] = '\0';
        finishTag();
        state_ = Text;
      } else if (!tagEnded_) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          tagEnded_ = true;
        } else if (tagLength_ < sizeof(tag_) - 1) {
          tag_[tagLength_++] = c;
          if (tagLength_ == 3 && memcmp(tag_, "!--", 3) == 0) {
            state_ = Comment;
            pending_ = 0;
          } else if (tagLength_ == 8 &&
                     memcmp(tag_, "![CDATA[", 8) == 0) {
            state_ = CData;
            pending_ = 0;
          }
        } else {
          tagEnded_ = true;
        }
      }
      break;
    case Comment:
      if (c == '>' && pending_ == 2) {
        state_ = Text;
        pending_ = 0;
      } else {
        pending_ = c == '-' ? (pending_ < 2 ? pending_ + 1 : 2) : 0;
      }
      break;
    case CData:
      if (c == ']') {
        if (pending_ == 2) append(']');
        else ++pending_;
      } else if (c == '>' && pending_ == 2) {
        state_ = Text;
        pending_ = 0;
      } else {
        while (pending_) {
          append(']');
          --pending_;
        }
        append(c);
      }
      break;
    }
  }

  bool done() const { return done_ || invalid_; }
  bool valid() const { return done_ && !invalid_ && count_ > 0; }
  size_t count() const { return count_; }
  const char *title(size_t index) const { return titles_[index]; }
  bool truncated(size_t index) const { return truncated_[index]; }

private:
  enum State { Text, Tag, Comment, CData } state_ = Text;
  char titles_[MAX_ITEMS][TITLE_CAPACITY + 1]{};
  bool truncated_[MAX_ITEMS]{};
  char tag_[16]{};
  size_t tagLength_ = 0;
  size_t titleLength_ = 0;
  size_t count_ = 0;
  unsigned int pending_ = 0;
  char quote_ = 0;
  bool tagEnded_ = false;
  bool selfClosing_ = false;
  bool insideItem_ = false;
  bool insideTitle_ = false;
  bool titleClosed_ = false;
  bool rootSeen_ = false;
  bool done_ = false;
  bool invalid_ = false;

  void append(char c) {
    if (!insideItem_ || !insideTitle_) return;
    if (titleLength_ < TITLE_CAPACITY) {
      titles_[count_][titleLength_++] = c;
      titles_[count_][titleLength_] = '\0';
    } else {
      truncated_[count_] = true;
    }
  }

  void finishTag() {
    if (selfClosing_ && tagLength_ && tag_[tagLength_ - 1] == '/') {
      tag_[--tagLength_] = '\0';
    }
    for (size_t i = 0; i < tagLength_; ++i) {
      if (tag_[i] >= 'A' && tag_[i] <= 'Z') tag_[i] += 'a' - 'A';
    }
    if (strcmp(tag_, "rss") == 0) {
      rootSeen_ = true;
      if (selfClosing_) done_ = true;
    } else if (strcmp(tag_, "item") == 0) {
      if (!rootSeen_ || insideItem_) {
        invalid_ = true;
        return;
      }
      insideItem_ = !selfClosing_;
      insideTitle_ = titleClosed_ = false;
      titleLength_ = 0;
      titles_[count_][0] = '\0';
      truncated_[count_] = false;
    } else if (strcmp(tag_, "title") == 0 && insideItem_) {
      insideTitle_ = !selfClosing_;
      titleClosed_ = selfClosing_;
      titleLength_ = 0;
      titles_[count_][0] = '\0';
    } else if (strcmp(tag_, "/title") == 0 && insideTitle_) {
      insideTitle_ = false;
      titleClosed_ = true;
    } else if (strcmp(tag_, "/item") == 0 && insideItem_) {
      if (insideTitle_) {
        invalid_ = true;
        return;
      }
      insideItem_ = false;
      if (titleClosed_ && titleLength_ > 0) ++count_;
      if (count_ == MAX_ITEMS) done_ = true;
    } else if (strcmp(tag_, "/rss") == 0) {
      invalid_ = !rootSeen_ || insideItem_;
      done_ = true;
    }
  }
};
