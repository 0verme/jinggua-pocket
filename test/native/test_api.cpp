#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/jinggua_api_client.h"
#include "jinggua/application/ring_history_store.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/application/wifi_controller.h"

namespace {

using jinggua::application::ApiClient;
using jinggua::application::ApiError;
using jinggua::application::ApiResult;
using jinggua::application::DivinationSession;
using jinggua::application::HttpResponse;
using jinggua::application::HttpTransport;
using jinggua::application::InputEvent;
using jinggua::application::JingGuaApiClient;
using jinggua::application::StateMachine;
using jinggua::application::TransportStatus;
using jinggua::application::AppState;
using jinggua::application::WifiState;

class FakeHttpTransport final : public HttpTransport {
 public:
  bool beginPost(const char* payload, std::size_t payloadLength,
                 const char* idempotencyKey) noexcept override {
    ++callCount_;
    pending_ = true;
    if (payload != nullptr && payloadLength <= payload_.size() - 1U) {
      std::memcpy(payload_.data(), payload, payloadLength);
      payload_[payloadLength] = '\0';
      payloadLength_ = payloadLength;
    }
    if (idempotencyKey != nullptr) {
      const auto length = std::strlen(idempotencyKey);
      const auto copyLength =
          length < idempotencyKey_.size() - 1U ? length
                                               : idempotencyKey_.size() - 1U;
      std::memcpy(idempotencyKey_.data(), idempotencyKey, copyLength);
      idempotencyKey_[copyLength] = '\0';
    }
    return true;
  }

  TransportStatus poll(HttpResponse& response) noexcept override {
    if (!pending_) {
      return TransportStatus::Idle;
    }
    pending_ = false;
    response = response_;
    return response.transportStatus;
  }

  void setResponse(int statusCode, const char* body) noexcept {
    response_ = HttpResponse{};
    response_.transportStatus = TransportStatus::Ok;
    response_.statusCode = statusCode;
    if (body == nullptr) {
      return;
    }
    const auto length = std::strlen(body);
    if (length > jinggua::application::kApiResponseMaxBytes) {
      response_.transportStatus = TransportStatus::ResponseTooLarge;
      return;
    }
    std::memcpy(response_.body.data(), body, length);
    response_.body[length] = '\0';
    response_.bodyLength = length;
  }

  void setTransportStatus(TransportStatus status) noexcept {
    response_ = HttpResponse{};
    response_.transportStatus = status;
  }

  int callCount() const noexcept { return callCount_; }
  std::size_t payloadLength() const noexcept { return payloadLength_; }
  const char* payload() const noexcept { return payload_.data(); }
  const char* idempotencyKey() const noexcept { return idempotencyKey_.data(); }

 private:
  HttpResponse response_{};
  std::array<char, jinggua::application::kApiRequestMaxBytes + 1> payload_{};
  std::array<char, jinggua::application::kApiRecordIdMaxLength + 1>
      idempotencyKey_{};
  std::size_t payloadLength_{0};
  int callCount_{0};
  bool pending_{false};
};

class FakeApiClient final : public ApiClient {
 public:
  bool beginUpload(const DivinationSession& session,
                   std::uint32_t localRecordId) noexcept override {
    ++callCount_;
    pending_ = true;
    observedComplete_ = session.isComplete();
    observedLocalRecordId_ = localRecordId;
    if (observedComplete_) {
      for (std::size_t index = 0; index < session.lines().size(); ++index) {
        observedLineTotals_[index] = session.lines()[index].coins.total;
      }
    }
    return true;
  }

  ApiResult poll() noexcept override {
    if (!pending_) {
      return terminalResult_;
    }
    pending_ = false;
    if (nextResultIndex_ < results_.size()) {
      terminalResult_ = results_[nextResultIndex_++];
    } else {
      terminalResult_ = ApiResult::success("fake-record");
    }
    return terminalResult_;
  }

  void enqueue(ApiResult result) { results_.push_back(std::move(result)); }
  int callCount() const noexcept { return callCount_; }
  bool observedComplete() const noexcept { return observedComplete_; }
  std::uint32_t observedLocalRecordId() const noexcept {
    return observedLocalRecordId_;
  }
  std::uint8_t observedLineTotal(std::size_t index) const noexcept {
    return index < observedLineTotals_.size() ? observedLineTotals_[index] : 0;
  }

 private:
  std::vector<ApiResult> results_;
  std::size_t nextResultIndex_{0};
  int callCount_{0};
  bool observedComplete_{false};
  std::uint32_t observedLocalRecordId_{0};
  std::array<std::uint8_t, 6> observedLineTotals_{};
  bool pending_{false};
  ApiResult terminalResult_{ApiResult::failure(ApiError::TransportError)};
};

SequenceRandomProvider randomForApiTest() {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(18);
  for (const auto total : std::array<std::uint8_t, 6>{6, 7, 8, 9, 8, 6}) {
    const auto coins = coinsForTotal(total);
    sequence.insert(sequence.end(), coins.coins.begin(), coins.coins.end());
  }
  return SequenceRandomProvider(std::move(sequence));
}

void castComplete(DivinationSession& session) {
  for (std::size_t index = 0; index < 6; ++index) {
    (void)session.castLine();
  }
}

bool sameLineTotals(const DivinationSession& session,
                    const std::array<jinggua::domain::Yao, 6>& before) {
  for (std::size_t index = 0; index < before.size(); ++index) {
    if (session.lines()[index].coins.total != before[index].coins.total ||
        session.lines()[index].position != before[index].position ||
        session.lines()[index].moving != before[index].moving) {
      return false;
    }
  }
  return true;
}

void completeStateMachine(StateMachine& stateMachine) {
  stateMachine.begin();
  stateMachine.handleInput(InputEvent::PrimaryClick);  // Welcome -> Prepare
  stateMachine.handleInput(InputEvent::PrimaryClick);  // Prepare -> Casting
  for (std::size_t index = 0; index < 6; ++index) {
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.finishLineAnimation();
    if (index < 5) {
      stateMachine.handleInput(InputEvent::PrimaryClick);
    }
  }
  stateMachine.handleInput(InputEvent::PrimaryClick);  // LineResult -> result
}

void expectUploadFailure(TestRunner& runner, StateMachine& stateMachine,
                         ApiError expected) {
  EXPECT_EQ(runner, stateMachine.state(), AppState::UploadFailed);
  EXPECT_EQ(runner, stateMachine.lastApiResult().error, expected);
}

ApiResult uploadAndPoll(TestRunner& runner, JingGuaApiClient& client,
                        const DivinationSession& session,
                        std::uint32_t localRecordId) {
  EXPECT(runner, client.beginUpload(session, localRecordId));
  return client.poll();
}

}  // namespace

void runApiTests(TestRunner& runner) {
  // The adapter uses the existing Web flat model, preserves bottom-to-top
  // line order, and sends no question/device identity data.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    castComplete(session);
    FakeHttpTransport transport;
    transport.setResponse(200, "{\"ok\":true,\"id\":\"web-record-1\"}");
    JingGuaApiClient client(transport);

    const auto result = uploadAndPoll(runner, client, session, 42);
    EXPECT(runner, result.isSuccess());
    EXPECT(runner, std::strcmp(result.recordId.data(), "web-record-1") == 0);
    EXPECT_EQ(runner, transport.callCount(), 1);
    EXPECT(runner, std::strcmp(transport.idempotencyKey(), "pocket-0000002A") == 0);
    const std::string payload(transport.payload(), transport.payloadLength());
    EXPECT(runner, payload.find("\"line_values\":[6,7,8,9,8,6]") !=
                       std::string::npos);
    EXPECT(runner, payload.find("\"hexagram_number\":40") !=
                       std::string::npos);
    EXPECT(runner, payload.find("\"moving_lines\":[1,4,6]") !=
                       std::string::npos);
    EXPECT(runner, payload.find("\"changed_hexagram_number\":41") !=
                       std::string::npos);
    EXPECT(runner, payload.find("\"local_record_id\":42") !=
                       std::string::npos);
    EXPECT(runner, payload.find("question") == std::string::npos);
    EXPECT(runner, payload.find("device") == std::string::npos);
  }

  // An incomplete session cannot reach the transport; this is the randomness
  // boundary and also prevents partial-line uploads.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    for (std::size_t index = 0; index < 5; ++index) {
      EXPECT(runner, session.castLine());
    }
    FakeHttpTransport transport;
    JingGuaApiClient client(transport);
    EXPECT(runner, !client.beginUpload(session, 1));
    EXPECT_EQ(runner, transport.callCount(), 0);
  }

  // The client accepts both the current Web response and the reserved v1
  // response, while rejecting malformed/version-mismatched responses.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    castComplete(session);
    FakeHttpTransport transport;
    JingGuaApiClient client(transport);

    transport.setResponse(
        200, "{\"api_version\":\"v1\",\"record_id\":\"opaque-1\",\"status\":\"accepted\"}");
    EXPECT(runner, uploadAndPoll(runner, client, session, 2).isSuccess());

    transport.setResponse(200, "{\"api_version\":\"v2\",\"record_id\":\"opaque-2\",\"status\":\"accepted\"}");
    EXPECT_EQ(runner, uploadAndPoll(runner, client, session, 2).error,
              ApiError::ApiVersionMismatch);

    transport.setResponse(200, "not-json");
    EXPECT_EQ(runner, uploadAndPoll(runner, client, session, 2).error,
              ApiError::InvalidResponse);
  }

  // HTTP and transport failures are typed and bounded, not leaked as crashes.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    castComplete(session);
    FakeHttpTransport transport;
    JingGuaApiClient client(transport);

    transport.setResponse(500, "server details must not matter");
    EXPECT_EQ(runner, uploadAndPoll(runner, client, session, 3).error,
              ApiError::Http5xx);

    transport.setResponse(429, "slow down");
    EXPECT_EQ(runner, uploadAndPoll(runner, client, session, 3).error,
              ApiError::Http4xx);

    transport.setTransportStatus(TransportStatus::Timeout);
    EXPECT_EQ(runner, uploadAndPoll(runner, client, session, 3).error,
              ApiError::Timeout);

    transport.setTransportStatus(TransportStatus::ResponseTooLarge);
    EXPECT_EQ(runner, uploadAndPoll(runner, client, session, 3).error,
              ApiError::InvalidResponse);
  }

  // A. Wi-Fi Off: a result-page upload request never calls ApiClient.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeApiClient api;
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    EXPECT(runner, session.isComplete());
    stateMachine.handleInput(InputEvent::SecondaryClick);
    expectUploadFailure(runner, stateMachine, ApiError::Offline);
    EXPECT_EQ(runner, api.callCount(), 0);
    EXPECT(runner, session.result().has_value());
  }

  // B. User explicitly connects first, then explicitly uploads successfully.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeApiClient api;
    InMemorySlotStorage storage(8);
    jinggua::application::RingHistoryStore history(storage);
    EXPECT(runner, history.begin());
    StateMachine stateMachine(session, wifi, api, &history);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::SecondaryClick);  // Settings
    stateMachine.handleInput(InputEvent::PrimaryClick);    // Connecting
    wifi.setState(WifiState::Connected);
    stateMachine.update(1000);
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiConnected);
    stateMachine.handleInput(InputEvent::SecondaryClick);  // Settings
    stateMachine.handleInput(InputEvent::SecondaryClick);  // Welcome
    completeStateMachine(stateMachine);
    stateMachine.handleInput(InputEvent::SecondaryClick);
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    EXPECT_EQ(runner, api.callCount(), 0);
    stateMachine.update(1100);  // begin one-shot transport
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    EXPECT_EQ(runner, api.callCount(), 1);
    stateMachine.update(1200);  // poll response
    EXPECT_EQ(runner, stateMachine.state(), AppState::UploadSuccess);
    EXPECT_EQ(runner, api.observedLocalRecordId(), static_cast<std::uint32_t>(1));
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(1));
    EXPECT(runner, session.isComplete());
  }

  // C. Timeout leaves the local result intact.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeApiClient api;
    api.enqueue(ApiResult::failure(ApiError::Timeout));
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    wifi.setState(WifiState::Connected);
    const auto beforeLines = session.lines();
    const auto beforeNumber = session.result()->original.number;
    stateMachine.handleInput(InputEvent::SecondaryClick);
    stateMachine.update(0);  // begin one-shot API request
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(1);  // receive timeout result
    expectUploadFailure(runner, stateMachine, ApiError::Timeout);
    EXPECT_EQ(runner, api.callCount(), 1);
    EXPECT(runner, sameLineTotals(session, beforeLines));
    EXPECT_EQ(runner, session.result()->original.number, beforeNumber);
  }

  // D. HTTP 500 through the real client adapter is also non-fatal locally.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeHttpTransport transport;
    transport.setResponse(500, "internal error");
    JingGuaApiClient api(transport);
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    wifi.setState(WifiState::Connected);
    stateMachine.handleInput(InputEvent::SecondaryClick);
    stateMachine.update(0);
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(1);
    expectUploadFailure(runner, stateMachine, ApiError::Http5xx);
    EXPECT(runner, session.result().has_value());
    EXPECT_EQ(runner, transport.callCount(), 1);
  }

  // E. Invalid JSON is recoverable and does not crash or mutate the session.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeHttpTransport transport;
    transport.setResponse(200, "{\"ok\":true");
    JingGuaApiClient api(transport);
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    wifi.setState(WifiState::Connected);
    stateMachine.handleInput(InputEvent::SecondaryClick);
    stateMachine.update(0);
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(1);
    expectUploadFailure(runner, stateMachine, ApiError::InvalidResponse);
    EXPECT(runner, session.isComplete());
  }

  // F. A response larger than the transport bound fails safely.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeHttpTransport transport;
    std::string oversized(jinggua::application::kApiResponseMaxBytes + 1U,
                          'x');
    transport.setResponse(200, oversized.c_str());
    JingGuaApiClient api(transport);
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    wifi.setState(WifiState::Connected);
    stateMachine.handleInput(InputEvent::SecondaryClick);
    stateMachine.update(0);
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(1);
    expectUploadFailure(runner, stateMachine, ApiError::InvalidResponse);
    EXPECT(runner, session.result().has_value());
  }

  // G. Retry is one more explicit click, never a background retry.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeApiClient api;
    api.enqueue(ApiResult::failure(ApiError::Timeout));
    api.enqueue(ApiResult::success("retry-record"));
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    wifi.setState(WifiState::Connected);

    stateMachine.handleInput(InputEvent::SecondaryClick);
    stateMachine.update(0);  // begin first request
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(1);  // first request fails
    expectUploadFailure(runner, stateMachine, ApiError::Timeout);
    stateMachine.update(2);
    EXPECT_EQ(runner, api.callCount(), 1);

    stateMachine.handleInput(InputEvent::PrimaryClick);
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(3);  // begin retry
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    stateMachine.update(4);
    EXPECT_EQ(runner, stateMachine.state(), AppState::UploadSuccess);
    EXPECT_EQ(runner, api.callCount(), 2);
  }

  // H. The API sees a complete local result and cannot alter its session.
  {
    auto random = randomForApiTest();
    DivinationSession session(random);
    FakeWifiController wifi;
    FakeApiClient api;
    StateMachine stateMachine(session, wifi, api);
    completeStateMachine(stateMachine);
    wifi.setState(WifiState::Connected);
    const auto beforeLines = session.lines();
    const auto beforeResult = session.result()->original.number;
    stateMachine.handleInput(InputEvent::SecondaryClick);
    EXPECT_EQ(runner, api.callCount(), 0);
    stateMachine.update(0);  // begin request after the complete local cast
    EXPECT_EQ(runner, stateMachine.state(), AppState::Uploading);
    EXPECT(runner, api.observedComplete());
    EXPECT_EQ(runner, api.observedLocalRecordId(), static_cast<std::uint32_t>(0));
    EXPECT_EQ(runner, api.observedLineTotal(0), static_cast<std::uint8_t>(6));
    EXPECT_EQ(runner, api.observedLineTotal(5), static_cast<std::uint8_t>(6));
    stateMachine.update(1);
    EXPECT_EQ(runner, stateMachine.state(), AppState::UploadSuccess);
    EXPECT(runner, sameLineTotals(session, beforeLines));
    EXPECT_EQ(runner, session.result()->original.number, beforeResult);
    EXPECT_EQ(runner, random.consumed(), static_cast<std::size_t>(18));
  }
}
