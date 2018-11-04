#include <fstream>
#include <sstream>
#include <filesystem>
#include "../idevice.hh"
#include "../wad_loaders.hh"
#include "map_lump.hh"

using namespace imp::wad;
Vector<String> iwad_textures;

namespace {
  template <class T>
  void read_into(std::istream& s, T& x)
  {
      s.read(reinterpret_cast<char*>(&x), sizeof(T));
  }

  struct Header {
      char id[4];
      uint32 numlumps;
      uint32 infotableofs;
  };

  struct Directory {
      uint32 filepos;
      uint32 size;
      char name[8];
  };

  static_assert(sizeof(Header) == 12, "WAD Header must have a sizeof of 12 bytes");
  static_assert(sizeof(Directory) == 16, "WAD Directory must have a sizeof of 16 bytes");

  struct Info {
      String name;
      Section section;
      size_t filepos;
      size_t size;
  };

  class DoomDevice;

  class DoomLump : public ILump {
      DoomDevice& device_;
      Info info_;

  public:
      DoomLump(DoomDevice& device, Info info):
          device_(device),
          info_(info) {}

      String name() const override
      { return info_.name; }

      String real_name() const override
      { return info_.name; }

      Section section() const override
      { return info_.section; }

      UniquePtr<std::istream> stream() override;

      IDevice& device() override;
  };

  class DoomDevice : public IDevice {
      std::ifstream stream_;

  public:
      DoomDevice(std::filesystem::path path):
          stream_(path, std::ios::binary)
      {
          stream_.exceptions(stream_.failbit | stream_.badbit);
      }

      Vector<ILumpPtr> read_all() override
      {
          Vector<ILumpPtr> lumps;
          Section section {};
          Header header;
          read_into(stream_, header);

          stream_.seekg(header.infotableofs);
          size_t numlumps = header.numlumps;

          size_t map_step {};
          std::unique_ptr<wad::doom::MapLump> map_lump {};

          for (size_t i = 0; i < numlumps; ++i) {
              Directory dir;
              read_into(stream_, dir);

              std::size_t size {};
              while (size < 8 && dir.name[size]) ++size;
              String name { dir.name, size };

              if (map_step) {
                  DEBUG("Adding {}", name);
                  std::string content;
                  auto current_pos = stream_.tellg();
                  stream_.seekg(dir.filepos);
                  content.resize(dir.size);
                  stream_.read(&content[0], dir.size);
                  stream_.seekg(current_pos);

                  map_lump->add_lump(name, content);
                  --map_step;

                  if (map_step == 0) {
                      DEBUG("Building {}", map_lump->name());
                      map_lump->build();
                      lumps.emplace_back(std::move(map_lump));
                  }
                  continue;
              }

              if (dir.size == 0) {
                  if (name == "T_START" || name == "TT_START") {
                      section = wad::Section::textures;
                  } else if (name == "G_START") {
                      section = wad::Section::graphics;
                  } else if (name == "S_START") {
                      section = wad::Section::sprites;
                  } else if (name == "DS_START") {
                      section = wad::Section::sounds;
                  } else if (name == "T_END" || name == "TT_END") {
                      section = wad::Section::normal;
                  } else if (name == "G_END") {
                      section = wad::Section::normal;
                  } else if (name == "S_END") {
                      section = wad::Section::normal;
                  } else if (name == "DS_END") {
                      section = wad::Section::normal;
                  } else if (name == "ENDOFWAD") {
                      break;
                  } else if (name.substr(0, 3) == "MAP") {
                      map_step = 13;
                      map_lump = std::make_unique<wad::doom::MapLump>(*this, name);
                      map_lump->add_lump(name, "");
                      continue;
                  } else {
                      log::warn("Unknown WAD directory '{}'", name);
                  }
                  continue;
              }

              if (section == Section::textures)
                  iwad_textures.emplace_back(name);

              auto lump_info = Info { name, section, dir.filepos, dir.size };
              auto lump_ptr = std::make_unique<DoomLump>(*this, lump_info);
              lumps.emplace_back(std::move(lump_ptr));
          }

          return lumps;
      }

      std::istream& stream()
      { return stream_; }
  };
}

UniquePtr<std::istream> DoomLump::stream()
{
    auto iss = std::make_unique<std::istringstream>();

    if (info_.size) {
        auto& s = device_.stream();
        s.seekg(info_.filepos);
        String buff(info_.size, 0);
        s.read(&buff[0], info_.size);
        iss->str(buff);
    }

    return iss;
}

IDevice& DoomLump::device()
{ return device_; }

IDevicePtr wad::doom_loader(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    Header header;
    read_into(file, header);
    if (memcmp(header.id, "IWAD", 4) == 0 || memcmp(header.id, "PWAD", 4) == 0) {
        return std::make_unique<DoomDevice>(path);
    } else {
        return nullptr;
    }
}
