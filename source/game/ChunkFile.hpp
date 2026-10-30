// game/ChunkFile.hpp -- OWNER: WP5b.  Port of ChunkFile.cs (BinaryReader over the .map TextAsset bytes). Little endian (Mono on ARM), absolute seeks.
// Reads past the end return 0 (the C# would throw EndOfStreamException; the shipped files never do that, the guard only keeps a damaged file from crashing).
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "miniunity.hpp"
using namespace miniunity;

class ChunkFile {
public:
  ChunkFile(const uint8_t* data, size_t size) : m_data(data), m_len(size) {}
  void Reset() { m_id = 0; m_fpos = 0; m_size = 0; m_pos = 0; }
  uint32_t getID() const { return m_id; }
  std::string getIDString() const { std::string s; for (int i = 0; i < 4; i++) s += (char)((m_id >> (8 * i)) & 0xFF); return s; }
  uint32_t getSize() const { return m_size; }
  void skip(int size) { if (size > 0) m_pos += (size_t)size; }
  long Position() const { return (long)m_pos; }
  bool Eof() const { return m_pos >= m_len; }

  uint8_t ReadByte() { return m_pos < m_len ? m_data[m_pos++] : (m_pos++, (uint8_t)0); }
  int16_t ReadInt16() { uint16_t v = (uint16_t)(ReadByte()); v |= (uint16_t)(ReadByte() << 8); return (int16_t)v; }
  int32_t ReadInt32() { uint32_t v = ReadUInt32(); return (int32_t)v; }
  uint32_t ReadUInt32() { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)ReadByte() << (8 * i); return v; }
  float ReadSingle() { uint32_t u = ReadUInt32(); float f; std::memcpy(&f, &u, 4); return f; }

  Vector3 readVector3() { Vector3 r; r.x = ReadSingle(); r.y = ReadSingle(); r.z = ReadSingle(); return r; }
  Vector3 readPosition() { Vector3 r; r.x = ReadSingle(); float y = ReadSingle(); r.y = 0.f - y; r.z = ReadSingle(); return r; }   // Y is negated (DS -> Unity)
  Vector3 readRotation() { return readVector3(); }
  Color readColor() { uint32_t n = ReadUInt32(); return Color((float)(n & 0xFF) / 255.f, (float)((n >> 8) & 0xFF) / 255.f, (float)((n >> 16) & 0xFF) / 255.f, (float)((n >> 24) & 0xFF) / 255.f); }
  Color readColorf() { Color c; c.r = ReadSingle(); c.g = ReadSingle(); c.b = ReadSingle(); c.a = ReadSingle(); return c; }
  float readFX32() { int n = ReadInt32(); return (float)n / 4096.f; }

  void fseek(long fpos) { m_pos = fpos < 0 ? 0 : (size_t)fpos; }
  // NUL-terminated string at fpos (or at the current position when fpos == -1); the read position is restored afterwards (so callers skip() the field themselves).
  std::string getString(long fpos) {
    size_t save = m_pos; if (fpos != -1) m_pos = (size_t)fpos;
    std::string s;
    while (m_pos < m_len) { uint8_t b = m_data[m_pos++]; if (b == 0) break; s += (char)b; }
    m_pos = save; return s;
  }
  bool nextChunk() {
    m_pos = m_fpos + m_size;
    if (m_pos + 4 > m_len) return false;
    m_id = ReadUInt32();
    if (m_id == 0) return false;
    m_size = ReadUInt32();
    m_fpos = m_pos;
    return true;
  }
private:
  const uint8_t* m_data; size_t m_len; size_t m_pos = 0;
  uint32_t m_id = 0, m_size = 0; size_t m_fpos = 0;
};
