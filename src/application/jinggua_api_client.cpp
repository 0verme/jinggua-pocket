#include "jinggua/application/jinggua_api_client.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace jinggua::application {
namespace {

class PayloadBuilder final {
 public:
  PayloadBuilder(char* buffer, std::size_t capacity) noexcept
      : buffer_(buffer), capacity_(capacity) {
    if (capacity_ != 0) {
      buffer_[0] = '\0';
    }
  }

  bool append(const char* text) noexcept {
    if (text == nullptr || length_ > capacity_) {
      return false;
    }
    const auto length = std::strlen(text);
    if (length >= capacity_ - length_) {
      return false;
    }
    std::memcpy(buffer_ + length_, text, length);
    length_ += length;
    buffer_[length_] = '\0';
    return true;
  }

  bool appendUnsigned(std::uint32_t value) noexcept {
    if (length_ >= capacity_) {
      return false;
    }
    const int written = std::snprintf(
        buffer_ + length_, capacity_ - length_, "%lu",
        static_cast<unsigned long>(value));
    if (written < 0 || static_cast<std::size_t>(written) >= capacity_ - length_) {
      return false;
    }
    length_ += static_cast<std::size_t>(written);
    return true;
  }

  std::size_t size() const noexcept { return length_; }

 private:
  char* buffer_;
  std::size_t capacity_;
  std::size_t length_{0};
};

std::uint32_t fingerprint(const DivinationSession& session) noexcept {
  // FNV-1a is only used to give a no-history session a stable id for a manual
  // retry. It is not a security primitive and does not participate in
  // randomness.
  std::uint32_t hash = 2166136261U;
  for (const auto& line : session.lines()) {
    hash ^= line.coins.total;
    hash *= 16777619U;
  }
  return hash;
}

bool makeDivinationId(const DivinationSession& session,
                      std::uint32_t localRecordId,
                      char* output, std::size_t capacity) noexcept {
  if (output == nullptr || capacity == 0) {
    return false;
  }
  const auto id = localRecordId == 0 ? fingerprint(session) : localRecordId;
  const int written = std::snprintf(
      output, capacity, "pocket-%08lX", static_cast<unsigned long>(id));
  return written > 0 && static_cast<std::size_t>(written) < capacity;
}

bool appendPayload(const DivinationSession& session,
                   std::uint32_t localRecordId, char* output,
                   std::size_t capacity, std::size_t& length,
                   char* idOutput, std::size_t idCapacity) noexcept {
  const auto& result = session.result();
  if (!session.isComplete() || !result.has_value() ||
      result->original.number < 1 || result->original.number > 64 ||
      result->movingCount > DivinationSession::kLineCount) {
    return false;
  }

  for (std::size_t index = 0; index < DivinationSession::kLineCount; ++index) {
    const auto& line = session.lines()[index];
    if (line.position != index + 1 || line.coins.total < 6 ||
        line.coins.total > 9) {
      return false;
    }
  }

  if (result->hasMovingLines() != result->transformed.has_value()) {
    return false;
  }
  if (result->transformed.has_value() &&
      (result->transformed->number < 1 || result->transformed->number > 64)) {
    return false;
  }

  std::uint8_t previousPosition = 0;
  for (std::uint8_t index = 0; index < result->movingCount; ++index) {
    const auto position = result->movingPositions[index];
    if (position < 1 || position > 6 || position <= previousPosition) {
      return false;
    }
    previousPosition = position;
  }

  if (!makeDivinationId(session, localRecordId, idOutput, idCapacity)) {
    return false;
  }

  PayloadBuilder builder(output, capacity);
  if (!builder.append("{\"api_version\":\"v1\",\"divination_id\":\"")) {
    return false;
  }
  if (!builder.append(idOutput) || !builder.append("\"")) {
    return false;
  }
  if (localRecordId != 0 &&
      (!builder.append(",\"local_record_id\":") ||
       !builder.appendUnsigned(localRecordId))) {
    return false;
  }
  if (!builder.append(",\"line_values\":[")) {
    return false;
  }
  for (std::size_t index = 0; index < DivinationSession::kLineCount; ++index) {
    if ((index != 0 && !builder.append(",")) ||
        !builder.appendUnsigned(session.lines()[index].coins.total)) {
      return false;
    }
  }
  if (!builder.append("],\"hexagram_number\":") ||
      !builder.appendUnsigned(result->original.number) ||
      !builder.append(",\"moving_lines\":[")) {
    return false;
  }
  for (std::uint8_t index = 0; index < result->movingCount; ++index) {
    if ((index != 0 && !builder.append(",")) ||
        !builder.appendUnsigned(result->movingPositions[index])) {
      return false;
    }
  }
  if (!builder.append("],\"changed_hexagram_number\":")) {
    return false;
  }
  if (result->transformed.has_value()) {
    if (!builder.appendUnsigned(result->transformed->number)) {
      return false;
    }
  } else if (!builder.append("null")) {
    return false;
  }
  if (!builder.append(",\"method\":\"coin\"}")) {
    return false;
  }

  length = builder.size();
  return length <= kApiRequestMaxBytes;
}

bool isSpace(char value) noexcept {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool isHexDigit(char value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'a' && value <= 'f') ||
         (value >= 'A' && value <= 'F');
}

struct ResponseFields {
  bool hasApiVersion{false};
  char apiVersion[16]{};
  bool hasOk{false};
  bool ok{false};
  bool hasStatus{false};
  char status[16]{};
  bool hasId{false};
  char id[kApiRecordIdMaxLength + 1]{};
  bool hasRecordId{false};
  char recordId[kApiRecordIdMaxLength + 1]{};
};

class JsonParser final {
 public:
  JsonParser(const char* data, std::size_t size) noexcept
      : data_(data), size_(size) {}

  bool parse(ResponseFields& fields) noexcept {
    if (!parseObject(fields)) {
      return false;
    }
    skipSpace();
    return position_ == size_;
  }

 private:
  void skipSpace() noexcept {
    while (position_ < size_ && isSpace(data_[position_])) {
      ++position_;
    }
  }

  bool consume(char expected) noexcept {
    skipSpace();
    if (position_ >= size_ || data_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  bool parseString(char* output, std::size_t capacity) noexcept {
    if (!consume('"')) {
      return false;
    }

    std::size_t length = 0;
    while (position_ < size_) {
      const char value = data_[position_++];
      if (value == '"') {
        if (output != nullptr) {
          output[length] = '\0';
        }
        return true;
      }
      if (static_cast<unsigned char>(value) < 0x20U) {
        return false;
      }
      char decoded = value;
      if (value == '\\') {
        if (position_ >= size_) {
          return false;
        }
        const char escaped = data_[position_++];
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            decoded = escaped;
            break;
          case 'b':
            decoded = '\b';
            break;
          case 'f':
            decoded = '\f';
            break;
          case 'n':
            decoded = '\n';
            break;
          case 'r':
            decoded = '\r';
            break;
          case 't':
            decoded = '\t';
            break;
          case 'u':
            // IDs, status, and versions are ASCII in this contract. Unknown
            // strings may still contain a Unicode escape, but there is no
            // reason to decode it into a fixed-size buffer here.
            if (output != nullptr || position_ + 4 > size_) {
              return false;
            }
            for (int index = 0; index < 4; ++index) {
              if (!isHexDigit(data_[position_++])) {
                return false;
              }
            }
            continue;
          default:
            return false;
        }
      }
      if (output != nullptr) {
        if (length + 1 >= capacity) {
          return false;
        }
        output[length] = decoded;
      }
      ++length;
    }
    return false;
  }

  bool parseBoolean(bool& output) noexcept {
    skipSpace();
    if (position_ + 4 <= size_ &&
        std::memcmp(data_ + position_, "true", 4) == 0) {
      position_ += 4;
      output = true;
      return true;
    }
    if (position_ + 5 <= size_ &&
        std::memcmp(data_ + position_, "false", 5) == 0) {
      position_ += 5;
      output = false;
      return true;
    }
    return false;
  }

  bool parseNumber() noexcept {
    skipSpace();
    const std::size_t start = position_;
    if (position_ < size_ && data_[position_] == '-') {
      ++position_;
    }
    if (position_ >= size_) {
      return false;
    }
    if (data_[position_] == '0') {
      ++position_;
    } else {
      if (data_[position_] < '1' || data_[position_] > '9') {
        return false;
      }
      while (position_ < size_ && data_[position_] >= '0' &&
             data_[position_] <= '9') {
        ++position_;
      }
    }
    if (position_ < size_ && data_[position_] == '.') {
      ++position_;
      const std::size_t fractionStart = position_;
      while (position_ < size_ && data_[position_] >= '0' &&
             data_[position_] <= '9') {
        ++position_;
      }
      if (position_ == fractionStart) {
        return false;
      }
    }
    if (position_ < size_ && (data_[position_] == 'e' ||
                              data_[position_] == 'E')) {
      ++position_;
      if (position_ < size_ &&
          (data_[position_] == '+' || data_[position_] == '-')) {
        ++position_;
      }
      const std::size_t exponentStart = position_;
      while (position_ < size_ && data_[position_] >= '0' &&
             data_[position_] <= '9') {
        ++position_;
      }
      if (position_ == exponentStart) {
        return false;
      }
    }
    return position_ > start;
  }

  bool parseNull() noexcept {
    skipSpace();
    if (position_ + 4 > size_ ||
        std::memcmp(data_ + position_, "null", 4) != 0) {
      return false;
    }
    position_ += 4;
    return true;
  }

  bool skipValue(unsigned depth = 0) noexcept {
    if (depth > 8U) {
      return false;
    }
    skipSpace();
    if (position_ >= size_) {
      return false;
    }
    if (data_[position_] == '"') {
      return parseString(nullptr, 0);
    }
    if (data_[position_] == '{') {
      ++position_;
      skipSpace();
      if (position_ < size_ && data_[position_] == '}') {
        ++position_;
        return true;
      }
      while (true) {
        if (!parseString(nullptr, 0) || !consume(':') ||
            !skipValue(depth + 1U)) {
          return false;
        }
        skipSpace();
        if (position_ < size_ && data_[position_] == '}') {
          ++position_;
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (data_[position_] == '[') {
      ++position_;
      skipSpace();
      if (position_ < size_ && data_[position_] == ']') {
        ++position_;
        return true;
      }
      while (true) {
        if (!skipValue(depth + 1U)) {
          return false;
        }
        skipSpace();
        if (position_ < size_ && data_[position_] == ']') {
          ++position_;
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (data_[position_] == 't' || data_[position_] == 'f') {
      bool ignored = false;
      return parseBoolean(ignored);
    }
    if (data_[position_] == 'n') {
      return parseNull();
    }
    return parseNumber();
  }

  bool parseObject(ResponseFields& fields) noexcept {
    if (!consume('{')) {
      return false;
    }
    skipSpace();
    if (position_ < size_ && data_[position_] == '}') {
      ++position_;
      return true;
    }

    while (true) {
      char key[32]{};
      if (!parseString(key, sizeof(key)) || !consume(':')) {
        return false;
      }
      if (std::strcmp(key, "api_version") == 0) {
        if (!parseString(fields.apiVersion, sizeof(fields.apiVersion))) {
          return false;
        }
        fields.hasApiVersion = true;
      } else if (std::strcmp(key, "ok") == 0) {
        if (!parseBoolean(fields.ok)) {
          return false;
        }
        fields.hasOk = true;
      } else if (std::strcmp(key, "status") == 0) {
        if (!parseString(fields.status, sizeof(fields.status))) {
          return false;
        }
        fields.hasStatus = true;
      } else if (std::strcmp(key, "id") == 0) {
        if (!parseString(fields.id, sizeof(fields.id))) {
          return false;
        }
        fields.hasId = true;
      } else if (std::strcmp(key, "record_id") == 0) {
        if (!parseString(fields.recordId, sizeof(fields.recordId))) {
          return false;
        }
        fields.hasRecordId = true;
      } else if (!skipValue()) {
        return false;
      }
      skipSpace();
      if (position_ < size_ && data_[position_] == '}') {
        ++position_;
        return true;
      }
      if (!consume(',')) {
        return false;
      }
    }
  }

  const char* data_;
  std::size_t size_;
  std::size_t position_{0};
};

ApiResult parseResponse(const HttpResponse& response) noexcept {
  if (response.bodyLength == 0 ||
      response.bodyLength > kApiResponseMaxBytes) {
    return ApiResult::failure(ApiError::InvalidResponse);
  }

  char body[kApiResponseMaxBytes + 1]{};
  std::memcpy(body, response.body.data(), response.bodyLength);
  body[response.bodyLength] = '\0';

  ResponseFields fields;
  JsonParser parser(body, response.bodyLength);
  if (!parser.parse(fields)) {
    return ApiResult::failure(ApiError::InvalidResponse);
  }
  if (fields.hasApiVersion &&
      std::strcmp(fields.apiVersion, kJingGuaApiVersion) != 0) {
    return ApiResult::failure(ApiError::ApiVersionMismatch);
  }

  if (fields.hasStatus) {
    if (std::strcmp(fields.status, "accepted") != 0 ||
        !fields.hasRecordId) {
      return ApiResult::failure(ApiError::InvalidResponse);
    }
    return ApiResult::success(fields.recordId);
  }

  if (!fields.hasOk || !fields.ok || !fields.hasId) {
    return ApiResult::failure(ApiError::InvalidResponse);
  }
  return ApiResult::success(fields.id);
}

ApiResult mapTransportResponse(const HttpResponse& response) noexcept {
  switch (response.transportStatus) {
    case TransportStatus::Idle:
    case TransportStatus::Pending:
      return ApiResult::failure(ApiError::TransportError);
    case TransportStatus::Timeout:
      return ApiResult::failure(ApiError::Timeout);
    case TransportStatus::NetworkError:
    case TransportStatus::ConfigurationError:
      return ApiResult::failure(ApiError::TransportError);
    case TransportStatus::ResponseTooLarge:
      return ApiResult::failure(ApiError::InvalidResponse);
    case TransportStatus::Ok:
      break;
  }

  if (response.statusCode >= 400 && response.statusCode < 500) {
    return ApiResult::failure(ApiError::Http4xx);
  }
  if (response.statusCode >= 500 && response.statusCode < 600) {
    return ApiResult::failure(ApiError::Http5xx);
  }
  if (response.statusCode < 200 || response.statusCode >= 300) {
    return ApiResult::failure(ApiError::TransportError);
  }
  return parseResponse(response);
}

}  // namespace

const char* apiErrorName(ApiError error) noexcept {
  switch (error) {
    case ApiError::None:
      return "NONE";
    case ApiError::Offline:
      return "OFFLINE";
    case ApiError::WifiUnavailable:
      return "WIFI_UNAVAILABLE";
    case ApiError::Timeout:
      return "TIMEOUT";
    case ApiError::TransportError:
      return "TRANSPORT_ERROR";
    case ApiError::Http4xx:
      return "HTTP_4XX";
    case ApiError::Http5xx:
      return "HTTP_5XX";
    case ApiError::InvalidResponse:
      return "INVALID_RESPONSE";
    case ApiError::ApiVersionMismatch:
      return "API_VERSION_MISMATCH";
  }
  return "UNKNOWN";
}

ApiResult ApiResult::pending() noexcept {
  ApiResult result;
  result.error = ApiError::None;
  result.accepted = false;
  result.complete = false;
  return result;
}

ApiResult ApiResult::success(const char* recordId) noexcept {
  if (recordId == nullptr) {
    return failure(ApiError::InvalidResponse);
  }
  const auto length = std::strlen(recordId);
  if (length == 0 || length > kApiRecordIdMaxLength) {
    return failure(ApiError::InvalidResponse);
  }

  ApiResult result;
  result.error = ApiError::None;
  result.accepted = true;
  result.complete = true;
  std::memcpy(result.recordId.data(), recordId, length);
  result.recordId[length] = '\0';
  return result;
}

ApiResult ApiResult::failure(ApiError error) noexcept {
  ApiResult result;
  result.error = error;
  result.accepted = false;
  result.complete = true;
  return result;
}

JingGuaApiClient::JingGuaApiClient(HttpTransport& transport) noexcept
    : transport_(transport) {}

bool JingGuaApiClient::beginUpload(const DivinationSession& session,
                                   std::uint32_t localRecordId) noexcept {
  if (pending_) {
    return false;
  }

  payloadLength_ = 0;
  if (!appendPayload(session, localRecordId, payload_.data(), payload_.size(),
                     payloadLength_, id_.data(), id_.size())) {
    terminalResult_ = ApiResult::failure(ApiError::InvalidResponse);
    return false;
  }
  if (!transport_.beginPost(payload_.data(), payloadLength_, id_.data())) {
    terminalResult_ = ApiResult::failure(ApiError::TransportError);
    return false;
  }

  pending_ = true;
  terminalResult_ = ApiResult::pending();
  return true;
}

ApiResult JingGuaApiClient::poll() noexcept {
  if (!pending_) {
    return terminalResult_;
  }

  HttpResponse response;
  const auto status = transport_.poll(response);
  if (status == TransportStatus::Pending) {
    return ApiResult::pending();
  }
  if (status == TransportStatus::Idle) {
    pending_ = false;
    terminalResult_ = ApiResult::failure(ApiError::TransportError);
    return terminalResult_;
  }

  response.transportStatus = status;
  pending_ = false;
  terminalResult_ = mapTransportResponse(response);
  return terminalResult_;
}

}  // namespace jinggua::application
