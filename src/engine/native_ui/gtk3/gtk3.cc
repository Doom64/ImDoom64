#include "native_ui/native_ui.hh"

#include "system/n64_rom.hh"

namespace {
  Optional<String> s_rom_select;
  String s_version;
  bool s_valid_rom {};

  class Gtk3UI : public NativeUI {
  public:
      Gtk3UI();

      void console_show(bool) override;

      void console_add_line(StringView) override;

      Optional<std::filesystem::path> rom_select() override;
  };
}

void G_ExecuteCommand(char *action);

extern "C" {
void nui_gtk_init();
void nui_gtk_rom_dialog_init();
void nui_gtk_console_add_line(const char *line);

/**
 * Gtk3 interop: Open and check (potential) ROM
 */
void nui_gtk_rom_open(char *path)
{
    std::filesystem::path spath { path };

    sys::N64Rom rom { spath };

    if (rom.is_open()) {
        s_version = rom.version();
        s_valid_rom = true;
    } else {
        s_version = "Not a valid ROM";
        s_valid_rom = false;
    }
}

/**
 * Gtk3 interop: Get version of last opened ROM
 */
const char* nui_gtk_rom_version()
{
    char *str = reinterpret_cast<char *>(malloc(s_version.size() + 1));
    std::copy_n(s_version.c_str(), s_version.size() + 1, str);
    return str;
}

/**
 * Gtk3 interop: Check if ROM was valid
 */
bool nui_gtk_rom_valid()
{
    return s_valid_rom;
}

/**
 * Gtk3 interop: Finalise ROM select
 */
void nui_gtk_rom_select(const char *path)
{
    s_rom_select = String { path };
}

/**
 * Gtk3 interop: Evaluate console input
 */
void nui_gtk_console_eval(char *cmd)
{
    G_ExecuteCommand(cmd);
}

}

Gtk3UI::Gtk3UI()
{
    nui_gtk_init();
}

void Gtk3UI::console_show(bool show)
{
}

void Gtk3UI::console_add_line(StringView line)
{
    String str { line };
    nui_gtk_console_add_line(str.c_str());
}

Optional<std::filesystem::path> Gtk3UI::rom_select()
{
    nui_gtk_rom_dialog_init();

    return s_rom_select;
}

//
// imp_init_gtk3_ui
//
[[maybe_unused]]
void imp_init_gtk3_ui()
{
    g_native_ui = std::make_unique<Gtk3UI>();
}
