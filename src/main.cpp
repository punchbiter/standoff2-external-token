// https://t.me/secretoccanalo

#include "memory/memory.h"
#include "process/process_manager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr size_t rsa_key_size = 128;
constexpr size_t data_offset = 0x20;
}
class bigint {
private:
  std::vector<uint32_t> data;

  void trim() {
    while (data.size() > 1 && data.back() == 0) {
      data.pop_back();
    }
  }

public:
  bigint() : data(1, 0) {}

  bigint(const uint8_t *bytes, size_t len) {
    data.resize((len + 3) / 4, 0);
    for (size_t i = 0; i < len; i++) {
      size_t idx = (len - 1 - i) / 4;
      size_t shift = ((len - 1 - i) % 4) * 8;
      data[idx] |= uint32_t(bytes[i]) << shift;
    }
    trim();
  }

  bool is_zero() const { return data.size() == 1 && data[0] == 0; }

  bool is_odd() const { return data[0] & 1; }

  void divide_by_2() {
    uint32_t carry = 0;
    for (int i = data.size() - 1; i >= 0; i--) {
      uint64_t temp = (uint64_t(carry) << 32) | data[i];
      data[i] = temp >> 1;
      carry = temp & 1;
    }
    trim();
  }

  bool operator<(const bigint &other) const {
    if (data.size() != other.data.size()) {
      return data.size() < other.data.size();
    }
    for (int i = data.size() - 1; i >= 0; i--) {
      if (data[i] != other.data[i]) {
        return data[i] < other.data[i];
      }
    }
    return false;
  }

  bigint operator-(const bigint &other) const {
    bigint result = *this;
    uint64_t borrow = 0;
    for (size_t i = 0; i < result.data.size(); i++) {
      uint64_t val = result.data[i];
      uint64_t sub = (i < other.data.size() ? other.data[i] : 0) + borrow;
      if (val >= sub) {
        result.data[i] = val - sub;
        borrow = 0;
      } else {
        result.data[i] = (0x100000000ULL + val) - sub;
        borrow = 1;
      }
    }
    result.trim();
    return result;
  }

  bigint add_mod(const bigint &other, const bigint &mod) const {
    bigint result;
    result.data.resize(std::max(data.size(), other.data.size()) + 1, 0);

    uint64_t carry = 0;
    for (size_t i = 0; i < result.data.size(); i++) {
      uint64_t sum = carry;
      if (i < data.size())
        sum += data[i];
      if (i < other.data.size())
        sum += other.data[i];
      result.data[i] = sum & 0xFFFFFFFF;
      carry = sum >> 32;
    }
    result.trim();

    if (!(result < mod)) {
      result = result - mod;
    }
    return result;
  }

  bigint mul_mod(const bigint &b, const bigint &mod) const {
    bigint result;
    bigint temp = *this;
    bigint multiplier = b;

    while (!multiplier.is_zero()) {
      if (multiplier.is_odd()) {
        result = result.add_mod(temp, mod);
      }
      temp = temp.add_mod(temp, mod);
      multiplier.divide_by_2();
    }
    return result;
  }

  bigint pow_mod(const bigint &exp, const bigint &mod) const {
    bigint result;
    result.data[0] = 1;
    bigint base = *this;
    bigint e = exp;

    while (!e.is_zero()) {
      if (e.is_odd()) {
        result = result.mul_mod(base, mod);
      }
      base = base.mul_mod(base, mod);
      e.divide_by_2();
    }
    return result;
  }

  std::vector<uint8_t> to_bytes() const {
    std::vector<uint8_t> result;
    for (int i = data.size() - 1; i >= 0; i--) {
      result.push_back((data[i] >> 24) & 0xFF);
      result.push_back((data[i] >> 16) & 0xFF);
      result.push_back((data[i] >> 8) & 0xFF);
      result.push_back(data[i] & 0xFF);
    }

    while (result.size() > 1 && result[0] == 0) {
      result.erase(result.begin());
    }

    if (result.size() < rsa_key_size) {
      result.insert(result.begin(), rsa_key_size - result.size(), 0);
    }

    return result;
  }
};

std::string rsa_decrypt(uint64_t n_ptr, uint64_t d_ptr, uint64_t enc_ptr) {
  uint8_t n_bytes[rsa_key_size], d_bytes[rsa_key_size], enc_bytes[rsa_key_size];

  for (int i = 0; i < rsa_key_size; i++) {
    n_bytes[i] = memory_utils::read<uint8_t>(n_ptr + data_offset + i);
    d_bytes[i] = memory_utils::read<uint8_t>(d_ptr + data_offset + i);
    enc_bytes[i] = memory_utils::read<uint8_t>(enc_ptr + data_offset + i);
  }

  bigint n(n_bytes, rsa_key_size);
  bigint d(d_bytes, rsa_key_size);
  bigint enc(enc_bytes, rsa_key_size);

  bigint m = enc.pow_mod(d, n);
  std::vector<uint8_t> dec = m.to_bytes();

  size_t idx = 0;
  for (size_t i = 2; i < dec.size(); i++) {
    if (dec[i] == 0x00) {
      idx = i;
      break;
    }
  }

  if (idx == 0 || idx + 1 >= dec.size()) {
    return "";
  }

  return std::string(dec.begin() + idx + 1, dec.end());
}

std::string get_token(uint64_t il2cpp) {
  auto type_info = memory_utils::read<uint64_t>(il2cpp + 153831440);
  if (!type_info)
    return "";

  auto static_fields = memory_utils::read<uint64_t>(type_info + 0x60);
  if (!static_fields)
    return "";

  auto auth_manager = memory_utils::read<uint64_t>(static_fields + 0x0);
  if (!auth_manager)
    return "";

  auto auth_handler = memory_utils::read<uint64_t>(auth_manager + 0xB0);
  if (!auth_handler)
    return "";

  auto n_ptr = memory_utils::read<uint64_t>(auth_handler + 0x28);
  auto d_ptr = memory_utils::read<uint64_t>(auth_handler + 0x58);
  auto enc_ptr = memory_utils::read<uint64_t>(auth_handler + 0x60);

  if (!n_ptr || !d_ptr || !enc_ptr)
    return "";

  return rsa_decrypt(n_ptr, d_ptr, enc_ptr);
}

int main() {
  process_manager proc("com.axlebolt.standoff2");
  if (!proc.initialize())
    return -1;

  memory_utils::initialize(proc.get_pid());

  std::string token = get_token(proc.get_libunity_base());
  if (!token.empty()) {
    printf("%s\n", token.c_str());
  }

  return 0;
}