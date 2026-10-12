#include "jinggua/hardware/preferences_slot_storage.h"

#include <cstdio>
#include <cstring>

#include "jinggua/application/history_store.h"  // kHistoryCapacity

#if defined(ARDUINO)
#include <Preferences.h>
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

bool PreferencesSlotStorage::readRecord(std::size_t slot, std::uint8_t* data,
                                        std::size_t capacity,
                                        std::size_t& sizeOut) const noexcept {
#if defined(ARDUINO)
  if (!started_) {
    return false;
  }
  char key[8];
  makeSlotKey(key, slot);
  auto& prefs = preferences();
  const std::size_t stored = prefs.getBytesLength(key);
  if (stored == 0) {
    return false;  // empty slot
  }
  if (stored > capacity) {
    return false;
  }
  const std::size_t read = prefs.getBytes(key, data, stored);
  sizeOut = read;
  return read == stored;
#else
  (void)slot;
  (void)data;
  (void)capacity;
  (void)sizeOut;
  return false;
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
  char key[8];
  makeSlotKey(key, slot);
  auto& prefs = preferences();
  return !prefs.isKey(key) || prefs.remove(key);
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
  auto& prefs = preferences();
  if (!prefs.isKey(kRecoveryJournalKey)) {
    return application::RecoveryJournalReadResult::Missing;
  }
  const std::size_t stored = prefs.getBytesLength(kRecoveryJournalKey);
  if (stored == 0 || stored > capacity) {
    return application::RecoveryJournalReadResult::Error;
  }
  const std::size_t read =
      prefs.getBytes(kRecoveryJournalKey, data, stored);
  sizeOut = read;
  return read == stored ? application::RecoveryJournalReadResult::Ready
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
  auto& prefs = preferences();
  return !prefs.isKey(kRecoveryJournalKey) || prefs.remove(kRecoveryJournalKey);
#else
  return false;
#endif
}

}  // namespace jinggua::hardware
