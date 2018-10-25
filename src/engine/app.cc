#include <map>
#include <fstream>
#include <platform/app.hh>
#include <sys/stat.h>
#include <clocale>

#include <cxxabi.h>

#include "SDL.h"
#include "core/args.hh"

#ifdef main
#undef main
#else
#define SDL_main main
#endif

String imp::type_to_string(const std::type_info& type)
{
#ifdef __GNUC__
    int status {};
    auto real_name = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);

    if (status == 0) {
        String name = real_name;
        std::free(real_name);
        return name;
    }

    return type.name();
#else
    return type.name();
#endif
}

[[noreturn]]
void D_DoomMain();

int myargc{};

char **myargv{};

String data_dir { "./" };

namespace {
  auto &_gparams()
  {
      static std::map<StringView, app::Param *> params;
      return params;
  }

  auto &_params = _gparams();

  String _base_dir { "./" };
  String& _data_dir = data_dir;
  StringView _program;
}

[[noreturn]]
void app::main(int argc, char **argv)
{
    myargc = argc;
    myargv = argv;

    _program = argv[0];

    auto base_dir = SDL_GetBasePath();
    if (base_dir) {
        _base_dir = base_dir;
        SDL_free(base_dir);
    }

    // Data files have to be in the cwd on Windows for compatibility reasons.
#ifndef _WIN32
    auto data_dir = SDL_GetPrefPath("", "ImDoom64");
    if (data_dir) {
        _data_dir = data_dir;
        SDL_free(data_dir);
    }
#endif

    args::parse(argc, argv);

    D_DoomMain();
}

bool app::file_exists(StringView path)
{
#ifdef _WIN32
    return std::ifstream(path.to_string()).is_open();
#else
    struct stat st;

    if (stat(path.data(), &st) == -1) {
        return false;
    }

    return S_ISREG(st.st_mode);
#endif
}

Optional<String> app::find_data_file(StringView name, StringView dir_hint)
{
    String path;

    if (!dir_hint.empty()) {
        path = fmt::format("{}{}", dir_hint, name);
        if (app::file_exists(path))
            return path;
    }

    path = _base_dir + name.to_string();
    if (app::file_exists(path))
        return path;

    path = _data_dir + name.to_string();
    if (app::file_exists(path))
        return path;

#if defined(__LINUX__) || defined(__OpenBSD__)
    const char *paths[] = {
        "/usr/local/share/games/imdoom64/",
        "/usr/local/share/imdoom64/",
        "/usr/local/share/doom/",
        "/usr/share/games/imdoom64/",
        "/usr/share/imdoom64/",
        "/usr/share/doom/",
        "/opt/imdoom64/",

        /* flatpak */
        "/app/imdoom64/"
    };

    for (auto p : paths) {
        path = fmt::format("{}{}", p, name);
        if (app::file_exists(path))
            return path;
    }
#endif

    return nullopt;
}

StringView app::program()
{
    return _program;
}

bool app::have_param(StringView name)
{
    return _params.count(name) == 1;
}

const app::Param *app::param(StringView name)
{
    auto it = _params.find(name);
    return it != _params.end() ? it->second : nullptr;
}

app::Param::Param(StringView name, app::Param::Arity arity) :
    arity_(arity)
{
    _gparams()[name] = this;
}

int SDL_main(int argc, char **argv)
{
    std::locale::global(std::locale::classic());
    app::main(argc, argv);
}
