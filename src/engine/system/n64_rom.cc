
#include <fstream>
#include <filesystem>
#include "prelude.hh"
#include "platform/app.hh"

#include "n64_rom.hh"

String n64_rom_text = "WADGEN";

namespace {
  template <class CharT>
  struct iequal_to {
      bool operator()(const CharT& a, const CharT& b) const
      { return std::toupper(a) == std::toupper(b); }
  };

  bool iequals(std::string_view a, std::string_view b) {
      if (a.size() != b.size())
          return false;

      iequal_to<char> f {};
      for (size_t i {}; i < a.size(); ++i) {
          if (!f(a[i], b[i])) {
              return false;
          }
      }

      return true;
  }

  /**
   * The way it seems to be laid out
   */

  /* Hardcoded ROM locations */
  constexpr sys::N64Version g_versions[4] = {
      /* EU, P is Portugal? */
      { "EU"_sv, 'P', 0,
        { 0x63f60,  0x5e89a4 }, // IWAD
        { 0x63ac40, 0xb9d8 },   // SN64
        { 0x646620, 0x142f8 },  // SSEQ
        { 0x65a920, 0x1716c4 }  // PCM data
      },

      /* Japan, J is for Japan, which makes sense */
      { "JP"_sv, 'J', 0,
        { 0x64580,  0x5ea120 }, // IWAD
        { 0x63ca00, 0xb9d8 },   // SN64
        { 0x6483e0, 0x142f8 },  // SSEQ
        { 0x65c6e0, 0x1716c4}   // PCM data
      },

      /* US rev. 1, E stands for Eunited States */
      { "US v1"_sv, 'E', 0,
        { 0x63d10,  0x5e38a8 }, // IWAD
        { 0x6355c0, 0xb9d8 },   // SN64
        { 0x640fa0, 0x142f8 },  // SSEQ
        { 0x6552a0, 0x1716c4 }  // PCM data
      },

      /* US rev. 2 */
      { "US v2"_sv, 'E', 1,
        { 0x63dc0,   0x6d301c }, // IWAD
        { 0x63ac40,  0xb9d8},   // SN64
        { 0x646620,  0x142f8 }, // SSEQ
        { 0x65a920 , 0x1716c4 } // PCM DATA
      }
  };

  struct Header {
      uint8 x1;		/* initial PI_BSB_DOM1_LAT_REG value */
      uint8 x2;		/* initial PI_BSB_DOM1_PGS_REG value */
      uint8 x3;		/* initial PI_BSB_DOM1_PWD_REG value */
      uint8 x4;		/* initial PI_BSB_DOM1_RLS_REG value */
      uint32 clock_rate;
      uint32 boot_address;
      uint32 release;
      uint32 crc1;
      uint32 crc2;
      uint32 unknown0;
      uint32 unknown1;
      char name[20];
      uint32 unknown2;
      uint16 unknown3;
      uint8 unknown4;
      uint8 manufacturer;
      uint16 cart_id;

      char country;
      char version;
  };

  static_assert(sizeof(Header) == 64, "N64 ROM header struct must be sizeof 64");
}

std::istringstream sys::N64Rom::m_load(const sys::N64Loc &loc)
{
    assert(m_file.is_open());
    assert(m_rom_version != nullptr);

    std::string buf;
    buf.resize(loc.size);

    m_file.seekg(loc.offset);
    m_file.read(&buf[0], loc.size);

    if (m_swapped) {
        for (size_t i {}; i + 1 < buf.size(); i += 2) {
            std::swap(buf[i], buf[i+1]);
        }
    }

    std::istringstream iss(std::ios_base::binary);
    iss.str(buf);
    iss.exceptions(std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit);
    return iss;
}

bool sys::N64Rom::open(const std::filesystem::path& path)
{
    /* Used to detect endianess. Padded to 20 characters. */
    constexpr auto norm_name  = "Doom64              "_sv;
    constexpr auto norm_name2 = "Doom 64             "_sv;
    constexpr auto swap_name  = "oDmo46              "_sv;
    constexpr auto swap_name2 = "oDmo4 6             "_sv;

    Header header;

    m_file.open(path, std::ios::binary);
    m_file.read(reinterpret_cast<char*>(&header), sizeof(header));

    char country {};
    char version {};
    if (iequals(norm_name, header.name) ||
        iequals(norm_name2, header.name)) {
        country = header.country;
        version = header.version;
        m_swapped = false;
    } else if (iequals(swap_name, header.name) ||
               iequals(swap_name2, header.name)) {
        country = header.version;
        version = header.country;
        m_swapped = true;
    } else {
        m_error = "Could not detect ROM";
        m_file.close();

        return false;
    }

    for (const auto& l : g_versions) {
        if (l.country_id == country && l.version_id == version) {
            m_rom_version = &l;
            assert(l.sn64.size != 0 && l.sn64.offset != 0);
            assert(l.sseq.size != 0 && l.sseq.offset != 0);
            assert(l.iwad.size != 0 && l.iwad.offset != 0);
            assert(l.pcm.size != 0 && l.pcm.offset != 0);
            break;
        }
    }

    // It's likely a hacked region-free ROM, so iterate over the different
    // versions and check if the magic values match
    if (country == '\0') {
        for (const auto& l : g_versions) {
            char magic[4];
            m_file.seekg(l.iwad.offset, std::ios::beg);
            m_file.read(magic, 4);

            if ("IWAD"_sv == magic || "DAWI"_sv == magic) {
                m_rom_version = &l;
                break;
            }
        }
    }

    if (!m_rom_version) {
        m_error = fmt::format("WAD not found in Doom 64 ROM. (Country: {}, Version: {:d})",
                              country, version);
        log::warn("{}", m_error);
        m_file.close();
        return false;
    } else {
        m_version = m_rom_version->name;
        n64_rom_text = fmt::format("{} rom", m_version);
    }

    return true;
}

std::istringstream sys::N64Rom::iwad()
{
    return m_load(m_rom_version->iwad);
}

std::istringstream sys::N64Rom::sn64()
{
    return m_load(m_rom_version->sn64);
}

std::istringstream sys::N64Rom::sseq()
{
    return m_load(m_rom_version->sseq);
}

std::istringstream sys::N64Rom::pcm()
{
    return m_load(m_rom_version->pcm);
}
