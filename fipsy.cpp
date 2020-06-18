#include "fipsy.hpp"
#include "Streaming.h"

uint16_t
Fipsy::FuseTable::computeChecksum() const
{
  uint16_t c = 0x0000;
  for (size_t i = 0; i < size(); ++i) {
    c += test(i) << (i % 8);
  }
  return c;
}

Fipsy::Fipsy(SPIClass& spi)
  : m_spi(spi)
  , m_ss(-1)
{
}

bool
Fipsy::begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1)
{
  m_ss = ss;
  pinMode(ss, OUTPUT);
  m_spi.begin(sck, miso, mosi, ss);

  auto resp = spiTrans<8>({0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  uint32_t deviceId = (resp[4] << 24) | (resp[5] << 16) | (resp[6] << 8) | (resp[7] << 0);
  Serial << "Fipsy device " <<  deviceId << endl;

  return deviceId == 0x012B8043;
}

void
Fipsy::end()
{
  m_spi.end();
}

Fipsy::Status
Fipsy::readStatus()
{
  auto resp = spiTrans<8>({0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  return Status{(static_cast<uint32_t>(resp[4]) << 24) | (resp[5] << 16) | (resp[6] << 8) | (resp[7] << 0)};
}

void
Fipsy::waitIdle()
{
  while (readStatus().busy()) {
    delay(1);
  }
}

bool
Fipsy::enable()
{
  // enable offline configuration
  spiTrans<4>({0xC6, 0x08, 0x00, 0x00});
  waitIdle();

  // erase SRAM
  spiTrans<4>({0x0E, 0x01, 0x00, 0x00});
  waitIdle();

  auto status = readStatus();
  return !status.fail() && status.enabled();
}

void
Fipsy::disable()
{
  // disable configuration
  spiTrans<3>({0x26, 0x00, 0x00});
  spiTrans<4>({0xFF, 0xFF, 0xFF, 0xFF});

  // refresh
  spiTrans<3>({0x79, 0x00, 0x00});
  delay(10);
}

void
Fipsy::readFeatures(uint32_t& featureRow0, uint32_t& featureRow1, uint16_t& feabits)
{
  // read Feature Row
  auto resp = spiTrans<12>({0xE7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  featureRow0 = (resp[4] << 24) | (resp[5] << 16) | (resp[6] << 8) | (resp[7] << 0);
  featureRow1 = (resp[8] << 24) | (resp[9] << 16) | (resp[10] << 8) | (resp[11] << 0);

  // read FEABITS
  auto resp2 = spiTrans<12>({0xFB, 0x00, 0x00, 0x00, 0x00, 0x00});
  feabits = (resp2[4] << 8) | (resp2[5] << 0);
}

bool
Fipsy::program(const FuseTable& fuseTable)
{
  // erase flash
  spiTrans<4>({0x0E, 0x04, 0x00, 0x00});
  waitIdle();
  if (readStatus().fail()) {
    return false;
  }

  // set address to zero
  spiTrans<4>({0x46, 0x00, 0x00, 0x00});
  waitIdle();

  // program pages
  for (int i = 0; i < fuseTable.size(); i += 128) {
    std::array<uint8_t, 20> req;
    req.fill(0x00);
    req[0] = 0x70;
    req[3] = 0x01;
    for (int j = 0; j < 16; ++j) {
      for (int k = 0; k < 8; ++k) {
        req[4 + j] |= fuseTable[i + j * 8 + k] << (7 - k);
      }
    }
    spiTrans<20>(req);
    waitIdle();
  }

  // program DONE bit
  spiTrans<4>({0x5E, 0x00, 0x00, 0x00});
  waitIdle();
  return !readStatus().fail();
}

namespace {

class JedecParser
{
public:
  bool
  findStx()
  {
    return input.find('\x02');
  }

  bool
  skipField()
  {
    return input.find('*');
  }

  char
  readChar()
  {
    char ch;
    do {
      if (input.readBytes(&ch, 1) != 1) {
        return 0;
      }
    } while (ch == ' ' || ch == '\r' || ch == '\n');
    return ch;
  }

public:
  Stream& input;
};

} // anonymous namespace

Fipsy::JedecError
Fipsy::parseJedec(Stream& input, FuseTable& fuseTable)
{
  JedecParser parser{input};

  bool ok = parser.findStx() && parser.skipField();
  if (!ok) {
    return JedecError::NO_STX;
  }

  uint16_t fuseChecksum = 0xFFFF;

  bool etx = false;
  while (!etx) {
    char fieldId = parser.readChar();
    switch (fieldId) {
      case 'Q':
        if (parser.readChar() == 'F') {
          int qf = 0;
          char ch;
          while ((ch = parser.readChar()) != '*') {
            qf = qf * 10 + (ch - '0');
          }
          if (qf != fuseTable.size()) {
            return JedecError::BAD_QF;
          }
        } else {
          parser.skipField();
        }
        break;
      case 'F': {
        switch (parser.readChar()) {
          case '0':
            fuseTable.reset();
            break;
          case '1':
            fuseTable.set();
            break;
          default:
            return JedecError::BAD_F;
        }
        parser.skipField();
        break;
      }
      case 'L': {
        long addr = input.parseInt();
        for (bool stop = false; !stop; ) {
          switch (parser.readChar()) {
            case '0':
              fuseTable.reset(addr++);
              break;
            case '1':
              fuseTable.set(addr++);
              break;
            case '*':
              stop = true;
              break;
            default:
              return JedecError::BAD_L;
          }
        }
        break;
      }
      case 'C': {
        fuseChecksum = 0;
        char ch;
        while ((ch = parser.readChar()) != '*') {
          switch (ch) {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
              fuseChecksum = (fuseChecksum << 4) + (ch - '0');
              break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
              fuseChecksum = (fuseChecksum << 4) + (ch - 'A' + 0x0A);
              break;
            default:
              return JedecError::BAD_C;
          }
        }
      }
      case '\x03':
        etx = true;
        break;
      default:
        if (!parser.skipField()) {
          return JedecError::NO_ETX;
        }
        break;
    }
  }

  if (fuseTable.computeChecksum() != fuseChecksum) {
    return JedecError::WRONG_CHECKSUM;
  }
  return JedecError::OK;
}
