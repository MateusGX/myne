#pragma once

class MyneSettings;
class MyneState;
class WifiCredentialStore;

namespace JsonSettingsIO {

bool saveSettings(const MyneSettings& s, const char* path);
bool loadSettings(MyneSettings& s, const char* json, bool* needsResave = nullptr);

bool saveState(const MyneState& s, const char* path);
bool loadState(MyneState& s, const char* json);

bool saveWifi(const WifiCredentialStore& store, const char* path);
bool loadWifi(WifiCredentialStore& store, const char* json, bool* needsResave = nullptr);

}  // namespace JsonSettingsIO
