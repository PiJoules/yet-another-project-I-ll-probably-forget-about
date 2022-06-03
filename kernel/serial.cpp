#include <kernel/io.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

namespace serial {

constexpr uint16_t kCOM1 = 0x3f8;

namespace {

bool Received() { return io::Read8(kCOM1 + 5) & 1; }

bool IsTransmitEmpty() { return io::Read8(kCOM1 + 5) & 0x20; }

}  // namespace

void Initialize() {
  io::Write8(kCOM1 + 1, 0x00);  // Disable all interrupts
  io::Write8(kCOM1 + 3, 0x80);  // Enable DLAB (set baud rate divisor)
  io::Write8(kCOM1 + 0, 0x03);  // Set divisor to 3 (lo byte) 38400 baud
  io::Write8(kCOM1 + 1, 0x00);  //                  (hi byte)
  io::Write8(kCOM1 + 3, 0x03);  // 8 bits, no parity, one stop bit
  io::Write8(kCOM1 + 2,
             0xC7);  // Enable FIFO, clear them, with 14-byte threshold
  io::Write8(kCOM1 + 4, 0x0B);  // IRQs enabled, RTS/DSR set
}

// Be careful not to call this in a while loop since it could hang forever if
// nothing is received.
bool TryRead(char &c) {
  if (Received()) {
    c = static_cast<char>(io::Read8(kCOM1));
    return true;
  }
  return false;
}

bool TryPut(char c) {
  if (IsTransmitEmpty()) {
    io::Write8(kCOM1, static_cast<uint8_t>(c));
    return true;
  }
  return false;
}

char Read() {
  while (!Received()) {}
  return static_cast<char>(io::Read8(kCOM1));
}

void Put(char c) {
  while (!IsTransmitEmpty()) {}
  io::Write8(kCOM1, static_cast<uint8_t>(c));
}

void Write(const char *str) {
  for (size_t size = strlen(str), i = 0; i < size; ++i) Put(str[i]);
}

}  // namespace serial
