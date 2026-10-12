#include "test_framework.h"
#include "test_support.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "jinggua/application/ota_update.h"
#include "jinggua/application/ring_history_store.h"
#include "jinggua/application/state_machine.h"

namespace {

using jinggua::application::AppState;
using jinggua::application::FirmwareManifestError;
using jinggua::application::FirmwareUpdateClient;
using jinggua::application::FirmwareUpdateError;
using jinggua::application::FirmwareUpdateState;
using jinggua::application::FirmwareUpdateStatus;
using jinggua::application::InputEvent;
using jinggua::application::kFirmwareOtaSlotMaxBytes;
using jinggua::application::kFirmwareSha256HexLength;
using jinggua::application::kFirmwareVersionMaxLength;
using jinggua::application::parseFirmwareUpdateManifest;
using jinggua::application::compareFirmwareVersions;

constexpr char kValidManifest[] =
    "format=1\n"
    "version=1.2.3\n"
    "size=1024\n"
    "sha256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
    "url=https://github.com/0verme/jinggua-pocket/releases/"
    "download/v1.2.3/firmware.bin\n";

class FakeFirmwareUpdateClient final : public FirmwareUpdateClient {
 public:
  bool beginCheck() noexcept override {
    ++checkCount;
    if (!allowCheck) {
      return false;
    }
    current.state = FirmwareUpdateState::Checking;
    current.error = FirmwareUpdateError::None;
    return true;
  }

  bool beginInstall() noexcept override {
    ++installCount;
    if (!allowInstall || current.state != FirmwareUpdateState::Available) {
      return false;
    }
    current.state = FirmwareUpdateState::Downloading;
    current.progressPercent = 0;
    return true;
  }

  void cancel() noexcept override {
    ++cancelCount;
    if (current.state == FirmwareUpdateState::Checking ||
        current.state == FirmwareUpdateState::Downloading) {
      current.state = FirmwareUpdateState::Cancelled;
      current.error = FirmwareUpdateError::None;
    }
  }

  FirmwareUpdateStatus status() const noexcept override { return current; }

  void setState(FirmwareUpdateState state,
                FirmwareUpdateError error = FirmwareUpdateError::None,
                std::uint8_t progress = 0,
                const char* version = nullptr) noexcept {
    current.state = state;
    current.error = error;
    current.progressPercent = progress;
    if (version != nullptr) {
      std::strncpy(current.version, version, kFirmwareVersionMaxLength);
      current.version[kFirmwareVersionMaxLength] = '\0';
    }
  }

  bool allowCheck{true};
  bool allowInstall{true};
  int checkCount{0};
  int installCount{0};
  int cancelCount{0};
  FirmwareUpdateStatus current{};
};

void enterOtaPrompt(jinggua::application::StateMachine& stateMachine,
                    FakeWifiController& wifi) {
  stateMachine.begin();
  stateMachine.handleInput(InputEvent::SecondaryClick);  // Settings
  wifi.setState(jinggua::application::WifiState::Connected);
  stateMachine.handleInput(InputEvent::PrimaryClick);  // Connected
  stateMachine.handleInput(InputEvent::LongPress);     // Explicit OTA prompt
}

void startFakeInstall(jinggua::application::StateMachine& stateMachine,
                      FakeFirmwareUpdateClient& firmware) {
  stateMachine.handleInput(InputEvent::PrimaryClick);  // Check
  firmware.setState(FirmwareUpdateState::Available,
                    FirmwareUpdateError::None, 0, "1.2.3");
  stateMachine.update(1);
  stateMachine.handleInput(InputEvent::PrimaryClick);  // Install
}

void testManifestParsing(TestRunner& runner) {
  jinggua::application::FirmwareUpdateManifest manifest;
  FirmwareManifestError error = FirmwareManifestError::InvalidArgument;
  EXPECT(runner, parseFirmwareUpdateManifest(
                     kValidManifest, sizeof(kValidManifest) - 1, manifest,
                     &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::None);
  EXPECT_EQ(runner, std::string(manifest.version), std::string("1.2.3"));
  EXPECT_EQ(runner, manifest.sizeBytes, static_cast<std::uint32_t>(1024));
  EXPECT_EQ(
      runner, std::string(manifest.firmwareUrl),
      std::string("https://github.com/0verme/jinggua-pocket/releases/"
                  "download/v1.2.3/firmware.bin"));
  EXPECT_EQ(runner, std::strlen(manifest.sha256), kFirmwareSha256HexLength);

  std::vector<char> tooLarge(
      jinggua::application::kFirmwareManifestMaxBytes + 1, 'x');
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     tooLarge.data(), tooLarge.size(), manifest, &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::TooLarge);

  const std::string duplicate = std::string(kValidManifest) + "size=1024\n";
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     duplicate.data(), duplicate.size(), manifest, &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::InvalidSize);

  const std::string missing =
      "format=1\nversion=1.2.3\nsize=1024\nsha256="
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     missing.data(), missing.size(), manifest, &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::MissingField);

  std::string externalUrl(kValidManifest);
  const auto urlOffset = externalUrl.find("https://github.com/");
  externalUrl.replace(urlOffset, std::string::npos,
                      "https://updates.example.invalid/firmware.bin\n");
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     externalUrl.data(), externalUrl.size(), manifest, &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::InvalidUrl);

  std::string invalidDigest(kValidManifest);
  const auto digestOffset = invalidDigest.find("0123456789abcdef");
  invalidDigest[digestOffset] = 'z';
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     invalidDigest.data(), invalidDigest.size(), manifest,
                     &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::InvalidSha256);

  std::string tooLargeImage(kValidManifest);
  const auto sizeOffset = tooLargeImage.find("size=1024") + 5;
  tooLargeImage.replace(sizeOffset, 4,
                        std::to_string(kFirmwareOtaSlotMaxBytes + 1U));
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     tooLargeImage.data(), tooLargeImage.size(), manifest,
                     &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::InvalidSize);

  const std::string unknown = std::string(kValidManifest) + "token=secret\n";
  EXPECT(runner, !parseFirmwareUpdateManifest(
                     unknown.data(), unknown.size(), manifest, &error));
  EXPECT_EQ(runner, error, FirmwareManifestError::InvalidField);

  int comparison = 0;
  EXPECT(runner, compareFirmwareVersions("v1.10.0", "1.9.9", comparison));
  EXPECT_EQ(runner, comparison, 1);
  EXPECT(runner, compareFirmwareVersions("1.2.3", "v1.2.3", comparison));
  EXPECT_EQ(runner, comparison, 0);
  EXPECT(runner, compareFirmwareVersions("0.2.9", "0.3.0", comparison));
  EXPECT_EQ(runner, comparison, -1);
  EXPECT(runner, !compareFirmwareVersions("01.2.3", "1.2.3", comparison));
  EXPECT(runner, !compareFirmwareVersions("1.2", "1.2.0", comparison));
}

void testExplicitUpdateFlow(TestRunner& runner) {
  std::vector<jinggua::domain::CoinSide> sequence(18,
                                                   jinggua::domain::CoinSide::Back);
  SequenceRandomProvider random(std::move(sequence));
  FakeWifiController wifi;
  FakeFirmwareUpdateClient firmware;
  InMemorySlotStorage historyStorage(4);
  jinggua::application::RingHistoryStore history(historyStorage);
  EXPECT(runner, history.begin());
  jinggua::domain::HistoryRecord savedRecord;
  savedRecord.originalNumber = 1;
  savedRecord.lineTotals = {7, 7, 7, 7, 7, 7};
  EXPECT(runner, history.add(savedRecord));
  jinggua::domain::HistoryRecord historyBefore;
  EXPECT(runner, history.get(0, historyBefore));

  jinggua::application::DivinationSession session(random);
  jinggua::application::StateMachine stateMachine(session, wifi, &history);
  stateMachine.setFirmwareUpdateClient(firmware);
  stateMachine.begin();
  EXPECT_EQ(runner, stateMachine.state(), AppState::Welcome);
  EXPECT_EQ(runner, firmware.checkCount, 0);

  wifi.setState(jinggua::application::WifiState::Connected);
  stateMachine.handleInput(InputEvent::SecondaryClick);
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(), AppState::WifiConnected);
  EXPECT_EQ(runner, firmware.checkCount, 0);
  stateMachine.handleInput(InputEvent::LongPress);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaPrompt);
  EXPECT_EQ(runner, firmware.checkCount, 0);

  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, firmware.checkCount, 1);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaChecking);
  EXPECT(runner, !stateMachine.sleepAllowed());
  firmware.setState(FirmwareUpdateState::Available,
                    FirmwareUpdateError::None, 0, "1.2.3");
  stateMachine.update(1);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaAvailable);
  EXPECT_EQ(runner, firmware.installCount, 0);
  EXPECT_EQ(runner, std::string(stateMachine.lastFirmwareUpdateStatus().version),
            std::string("1.2.3"));

  firmware.allowInstall = false;
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, firmware.installCount, 1);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaAvailable);
  firmware.allowInstall = true;
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, firmware.installCount, 2);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaDownloading);
  firmware.setState(FirmwareUpdateState::Downloading,
                    FirmwareUpdateError::None, 37, "1.2.3");
  stateMachine.update(2);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaDownloading);
  EXPECT_EQ(runner, stateMachine.lastFirmwareUpdateStatus().progressPercent,
            static_cast<std::uint8_t>(37));

  stateMachine.handleInput(InputEvent::SecondaryClick);
  EXPECT_EQ(runner, firmware.cancelCount, 1);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaCancelling);
  firmware.setState(FirmwareUpdateState::Cancelled);
  stateMachine.update(3);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaCancelled);
  EXPECT_EQ(runner, session.lineCount(), static_cast<std::size_t>(0));
  EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(1));
  jinggua::domain::HistoryRecord historyAfter;
  EXPECT(runner, history.get(0, historyAfter));
  EXPECT_EQ(runner, historyAfter.localRecordId, historyBefore.localRecordId);
  EXPECT_EQ(runner, historyAfter.sessionOrder, historyBefore.sessionOrder);
  EXPECT_EQ(runner, historyAfter.originalNumber, historyBefore.originalNumber);
  EXPECT_EQ(runner, historyAfter.lineTotals[0], historyBefore.lineTotals[0]);
}

void testFinalizationCannotBeCancelled(TestRunner& runner) {
  FakeWifiController wifi;
  FakeFirmwareUpdateClient firmware;
  SequenceRandomProvider random(std::vector<jinggua::domain::CoinSide>(
      18, jinggua::domain::CoinSide::Back));
  jinggua::application::DivinationSession session(random);
  jinggua::application::StateMachine stateMachine(session, wifi);
  stateMachine.setFirmwareUpdateClient(firmware);
  enterOtaPrompt(stateMachine, wifi);
  startFakeInstall(stateMachine, firmware);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaDownloading);

  firmware.setState(FirmwareUpdateState::Finalizing,
                    FirmwareUpdateError::None, 100, "1.2.3");
  // Exercise the UI/input race: the worker has closed cancellation but the
  // state machine has not yet polled the new status.
  stateMachine.handleInput(InputEvent::SecondaryClick);
  EXPECT_EQ(runner, firmware.cancelCount, 1);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaFinalizing);
  EXPECT(runner, !stateMachine.sleepAllowed());
  stateMachine.handleInput(InputEvent::SecondaryClick);
  EXPECT_EQ(runner, firmware.cancelCount, 1);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaFinalizing);

  firmware.setState(FirmwareUpdateState::Rebooting,
                    FirmwareUpdateError::None, 100, "1.2.3");
  stateMachine.update(2);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaRebooting);
  stateMachine.handleInput(InputEvent::SecondaryClick);
  EXPECT_EQ(runner, firmware.cancelCount, 1);
}

void testFinalizationFailureIsReported(TestRunner& runner) {
  FakeWifiController wifi;
  FakeFirmwareUpdateClient firmware;
  SequenceRandomProvider random(std::vector<jinggua::domain::CoinSide>(
      18, jinggua::domain::CoinSide::Back));
  jinggua::application::DivinationSession session(random);
  jinggua::application::StateMachine stateMachine(session, wifi);
  stateMachine.setFirmwareUpdateClient(firmware);
  enterOtaPrompt(stateMachine, wifi);
  startFakeInstall(stateMachine, firmware);

  firmware.setState(FirmwareUpdateState::Finalizing,
                    FirmwareUpdateError::None, 100, "1.2.3");
  stateMachine.update(2);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaFinalizing);
  firmware.setState(FirmwareUpdateState::Failed,
                    FirmwareUpdateError::ActivationFailed);
  stateMachine.update(3);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaFailed);
  EXPECT_EQ(runner, stateMachine.lastFirmwareUpdateStatus().error,
            FirmwareUpdateError::ActivationFailed);
}

void testOfflineAndFailureRecovery(TestRunner& runner) {
  FakeWifiController wifi;
  FakeFirmwareUpdateClient firmware;
  SequenceRandomProvider random(std::vector<jinggua::domain::CoinSide>(
      18, jinggua::domain::CoinSide::Back));
  jinggua::application::DivinationSession session(random);
  jinggua::application::StateMachine stateMachine(session, wifi);
  stateMachine.setFirmwareUpdateClient(firmware);
  enterOtaPrompt(stateMachine, wifi);

  wifi.setState(jinggua::application::WifiState::Off);
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaFailed);
  EXPECT_EQ(runner, stateMachine.lastFirmwareUpdateStatus().error,
            FirmwareUpdateError::Offline);
  EXPECT_EQ(runner, firmware.checkCount, 0);

  wifi.setState(jinggua::application::WifiState::Connected);
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, firmware.checkCount, 1);
  firmware.setState(FirmwareUpdateState::Failed,
                    FirmwareUpdateError::Network);
  stateMachine.update(10);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaFailed);
  EXPECT_EQ(runner, firmware.checkCount, 1);

  // A retry is another explicit button action, never an automatic retry.
  stateMachine.update(20);
  EXPECT_EQ(runner, firmware.checkCount, 1);
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, firmware.checkCount, 2);
  EXPECT_EQ(runner, stateMachine.state(), AppState::OtaChecking);
}

}  // namespace

void runOtaTests(TestRunner& runner) {
  testManifestParsing(runner);
  testExplicitUpdateFlow(runner);
  testFinalizationCannotBeCancelled(runner);
  testFinalizationFailureIsReported(runner);
  testOfflineAndFailureRecovery(runner);
}
