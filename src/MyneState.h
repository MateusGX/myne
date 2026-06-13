#pragma once
#include <cstdint>

class MyneState {
  static MyneState instance;

 public:
  static constexpr uint8_t SLEEP_RECENT_COUNT = 16;

  uint16_t recentSleepImages[SLEEP_RECENT_COUNT] = {};
  uint8_t recentSleepPos = 0;
  uint8_t recentSleepFill = 0;

  bool isRecentSleep(uint16_t idx, uint8_t checkCount) const;
  void pushRecentSleep(uint16_t idx);

  ~MyneState() = default;

  static MyneState& getInstance() { return instance; }

  bool saveToFile() const;
  bool loadFromFile();
};

#define APP_STATE MyneState::getInstance()
