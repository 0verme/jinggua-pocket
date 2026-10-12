#include "jinggua/hardware/preferences_slot_storage.h"

#include <cstdio>
#include <cstring>

#include "jinggua/application/history_store.h"  // kHistoryCapacity

#if defined(ARDUINO)
#include <Preferences.h>
#include <nvs.h>
#endif

namespace jinggua::hardware {
namespace {

#if defined(ARDUINO)
Preferences& preferences() {
  static Preferences instance;
  return instance;
}
#endif

constexpr const char* kNamespace = "jinggua";
constexpr const char* kIndexKey = "idx";
constexpr const char* kRecoveryJournalKey = "recovery";

#if defined(ARDUINO)
enum class NvsBlobReadResult { Missing, Ready, Error };

NvsBlobReadResult readNvsBlob(const char* key, std::uint8_t* data,
                              std::size_t capacity,
                              std::size_t& sizeOut) noexcept {
  nvs_handle_t handle{};
  if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
    return NvsBlobReadResult::Error;
  }

  std::size_t stored = 0;
  const esp_err_t lengthResult = nvs_get_blob(handle, key, nullptr, &stored);
  if (lengthResult == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    return NvsBlobReadResult::Missing;
  }
  if (lengthResult != ESP_OK || stored == 0 || stored > capacity) {
    nvs_close(handle);
    return NvsBlobReadResult::Error;
  }

  std::size_t read = capacity;
  const esp_err_t readResult = nvs_get_blob(handle, key, data, &read);
  nvs_close(handle);
  if (readResult != ESP_OK || read != stored) {
    return NvsBlobReadResult::Error;
  }
  sizeOut = read;
  return NvsBlobReadResult::Ready;
}

bool eraseNvsKey(const char* key) noexcept {
  nvs_handle_t handle{};
  if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return false;
  }

  const esp_err_t eraseResult = nvs_erase_key(handle, key);
  if (eraseResult == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    return true;
  }
  if (eraseResult != ESP_OK) {
    nvs_close(handle);
    return false;
  }
  const esp_err_t commitResult = nvs_commit(handle);
  nvs_close(handle);
  return commitResult == ESP_OK;
}
#endif

// Preferences key names are capped at 15 characters ("rec_000" is 7).
void makeSlotKeyInternal(char* buffer, std::size_t slot) noexcept {
  std::snprintf(buffer, 8, "rec_%03u",
                static_cast<unsigned>(slot));
}

}  // namespace

bool PreferencesSlotStorage::begin() noexcept {
#if defined(ARDUINO)
  if (started_) {
    return true;
  }
  started_ = preferences().begin(kNamespace, false);
  return started_;
#else
  return false;
#endif
}

std::size_t PreferencesSlotStorage::capacity() const noexcept {
  return application::kHistoryCapacity;
}

void PreferencesSlotStorage::makeSlotKey(char* buffer,
                                         std::size_t slot) const noexcept {
  makeSlotKeyInternal(buffer, slot);
}

application::RecordReadResult PreferencesSlotStorage::readRecord(
    std::size_t slot, std::uint8_t* data, std::size_t capacity,
    std::size_t& sizeOut) const noexcept {
#if defined(ARDUINO)
  if (!started_ || slot >= this->capacity()) {
    return application::RecordReadResult::Error;
  }
  char key[8];
  makeSlotKey(key, slot);
  const auto result = readNvsBlob(key, data, capacity, sizeOut);
  if (result == NvsBlobReadResult::Missing) {
    return application::RecordReadResult::Missing;
  }
  return result == NvsBlobReadResult::Ready
             ? application::RecordReadResult::Ready
             : application::RecordReadResult::Error;
#else
  (void)slot;
  (void)data;
  (void)capacity;
  (void)sizeOut;
  return application::RecordReadResult::Error;
#endif
}

bool PreferencesSlotStorage::writeRecord(std::size_t slot,
                                         const std::uint8_t* data,
                                         std::size_t size) noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  char key[8];
  makeSlotKey(key, slot);
  auto& prefs = preferences();
  return prefs.putBytes(key, data, size) == size;
#else
  (void)slot;
  (void)data;
  (void)size;
  return false;
#endif
}

bool PreferencesSlotStorage::eraseRecord(std::size_t slot) noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  if (slot >= this->capacity()) {
    return false;
  }
  char key[8];
  makeSlotKey(key, slot);
  return eraseNvsKey(key);
#else
  (void)slot;
  return false;
#endif
}

bool PreferencesSlotStorage::readIndex(std::uint8_t* data,
                                       std::size_t capacity,
                                       std::size_t& sizeOut) const noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  auto& prefs = preferences();
  const std::size_t stored = prefs.getBytesLength(kIndexKey);
  if (stored == 0) {
    return false;
  }
  if (stored > capacity) {
    return false;
  }
  const std::size_t read = prefs.getBytes(kIndexKey, data, stored);
  sizeOut = read;
  return read == stored;
#else
  (void)data;
  (void)capacity;
  (void)sizeOut;
  return false;
#endif
}

bool PreferencesSlotStorage::writeIndex(const std::uint8_t* data,
                                        std::size_t size) noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  auto& prefs = preferences();
  return prefs.putBytes(kIndexKey, data, size) == size;
#else
  (void)data;
  (void)size;
  return false;
#endif
}

application::RecoveryJournalReadResult
PreferencesSlotStorage::readRecoveryJournal(std::uint8_t* data,
                                            std::size_t capacity,
                                            std::size_t& sizeOut) const noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return application::RecoveryJournalReadResult::Error;
  }
  const auto result = readNvsBlob(kRecoveryJournalKey, data, capacity,
                                  sizeOut);
  if (result == NvsBlobReadResult::Missing) {
    return application::RecoveryJournalReadResult::Missing;
  }
  return result == NvsBlobReadResult::Ready
             ? application::RecoveryJournalReadResult::Ready
             : application::RecoveryJournalReadResult::Error;
#else
  (void)data;
  (void)capacity;
  (void)sizeOut;
  return application::RecoveryJournalReadResult::Error;
#endif
}

bool PreferencesSlotStorage::writeRecoveryJournal(
    const std::uint8_t* data, std::size_t size) noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  auto& prefs = preferences();
  return prefs.putBytes(kRecoveryJournalKey, data, size) == size;
#else
  (void)data;
  (void)size;
  return false;
#endif
}

bool PreferencesSlotStorage::eraseRecoveryJournal() noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  return eraseNvsKey(kRecoveryJournalKey);
#else
  return false;
#endif
}

}  // namespace jinggua::hardware
