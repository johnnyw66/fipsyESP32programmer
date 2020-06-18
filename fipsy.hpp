#ifndef FIPSY_HPP
#define FIPSY_HPP

#include <array>
#include <bitset>
#include <Arduino.h>
#include <SPI.h>

/** \brief Fipsy FPGA programmer.
 */
class Fipsy
{
public:
  /** \brief Fuse table of MachXO2-256.
   */
  class FuseTable : public std::bitset<73600>
  {
  public:
    /** \brief Compute fuse checksum.
     */
    uint16_t
    computeChecksum() const;
  };

  /** \brief Status register value.
   */
  class Status
  {
  public:
    bool
    enabled() const
    {
      return v & (1 << 9);
    }

    bool
    busy() const
    {
      return v & (1 << 12);
    }

    bool
    fail() const
    {
      return v & (1 << 13);
    }

  public:
    uint32_t v;
  };

  /** \brief Constructor.
   *  \param spi the SPI bus.
   */
  explicit
  Fipsy(SPIClass& spi);

  /** \brief Detect Fipsy.
   *  \return Whether expected Device ID is found.
   */
  bool
  begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss);

  /** \brief Release SPI bus.
   *  \pre begin()
   */
  void
  end();

  /** \brief Read status register.
   *  \pre begin()
   */
  Status
  readStatus();

  /** \brief Enable offline configuration mode.
   *  \pre begin()
   */
  bool
  enable();

  /** \brief Disable configuration mode.
   *  \pre enable()
   */
  void
  disable();

  /** \brief Read Feature Row and FEABITS.
   *  \pre enable()
   */
  void
  readFeatures(uint32_t& featureRow0, uint32_t& featureRow1, uint16_t& feabits);

  /** \brief Program fuse table.
   *  \pre enable()
   */
  bool
  program(const FuseTable& fuseTable);

  enum class JedecError
  {
    OK,
    NO_STX,
    NO_ETX,
    BAD_QF,
    BAD_F,
    BAD_L,
    BAD_C,
    WRONG_CHECKSUM,
  };

  /** \brief Parse fuse table from JEDEC file.
   */
  static JedecError
  parseJedec(Stream& input, FuseTable& fuseTable);

private:
  template<int N>
  std::array<uint8_t, N>
  spiTrans(const std::array<uint8_t, N>& req);

  void
  waitIdle();

private:
  SPIClass& m_spi;
  int m_ss;
};

template<int N>
std::array<uint8_t, N>
Fipsy::spiTrans(const std::array<uint8_t, N>& req)
{
  std::array<uint8_t, N> resp;
  digitalWrite(m_ss, LOW);
  m_spi.beginTransaction(SPISettings(400000, SPI_MSBFIRST, SPI_MODE0));
  m_spi.transferBytes(const_cast<uint8_t*>(req.data()), resp.data(), req.size());
  digitalWrite(m_ss, HIGH);
  m_spi.endTransaction();
  return resp;
}

#endif // FIPSY_HPP
