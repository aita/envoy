#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/buffer/buffer.h"
#include "envoy/common/exception.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MemcacheProxy {

/**
 * All RESP types as defined here: https://memcache.io/topics/protocol
 */
enum class RespType { Null, SimpleString, BulkString, Integer, Error, Array };

/**
 * A variant implementation of a RESP value optimized for performance. A C++11 union is used for
 * the underlying type so that no unnecessary allocations/constructions are needed.
 */
class RespValue {
public:
  RespValue() : type_(RespType::Null) {}
  ~RespValue() { cleanup(); }

  /**
   * Convert a RESP value to a string for debugging purposes.
   */
  std::string toString() const;

  /**
   * The following are getters and setters for the internal value. A RespValue starts as null,
   * and must change type via type() before the following methods can be used.
   */
  std::vector<RespValue>& asArray();
  const std::vector<RespValue>& asArray() const;
  std::string& asString();
  const std::string& asString() const;
  int64_t& asInteger();
  int64_t asInteger() const;

  /**
   * Get/set the type of the RespValue. A RespValue can only be a single type at a time. Each time
   * type() is called the type is changed and then the type specific as* methods can be used.
   */
  RespType type() const { return type_; }
  void type(RespType type);

private:
  union {
    std::vector<RespValue> array_;
    std::string string_;
    int64_t integer_;
  };

  void cleanup();

  RespType type_;
};

typedef std::unique_ptr<RespValue> RespValuePtr;

/**
 * Callbacks that the decoder fires.
 */
class DecoderCallbacks {
public:
  virtual ~DecoderCallbacks() {}

  /**
   * Called when a new top level RESP value has been decoded. This value may include multiple
   * sub-values in the case of arrays or nested arrays.
   * @param value supplies the decoded value that is now owned by the callee.
   */
  virtual void onRespValue(RespValuePtr&& value) PURE;
};

/**
 * A memcache byte decoder for https://memcache.io/topics/protocol
 */
class Decoder {
public:
  virtual ~Decoder() {}

  /**
   * Decode memcache protocol bytes.
   * @param data supplies the data to decode. All bytes will be consumed by the decoder or a
   *        ProtocolError will be thrown.
   */
  virtual void decode(Buffer::Instance& data) PURE;
};

typedef std::unique_ptr<Decoder> DecoderPtr;

/**
 * A factory for a memcache decoder.
 */
class DecoderFactory {
public:
  virtual ~DecoderFactory() {}

  /**
   * Create a decoder given a set of decoder callbacks.
   */
  virtual DecoderPtr create(DecoderCallbacks& callbacks) PURE;
};

/**
 * A memcache byte encoder for https://memcache.io/topics/protocol
 */
class Encoder {
public:
  virtual ~Encoder() {}

  /**
   * Encode a RESP value to a buffer.
   * @param value supplies the value to encode.
   * @param out supplies the buffer to encode to.
   */
  virtual void encode(const RespValue& value, Buffer::Instance& out) PURE;
};

typedef std::unique_ptr<Encoder> EncoderPtr;

/**
 * A memcache protocol error.
 */
class ProtocolError : public EnvoyException {
public:
  ProtocolError(const std::string& error) : EnvoyException(error) {}
};

} // namespace MemcacheProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
