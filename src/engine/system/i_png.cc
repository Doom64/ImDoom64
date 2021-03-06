// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007-2012 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    PNG image system
//
//-----------------------------------------------------------------------------

#include <algorithm>
#include <sstream>
#include <cmath>

#include "doomdef.h"
#include "i_swap.h"
#include "z_zone.h"
#include "gl_texture.h"

#include "core/cvar.hh"
#include "image/image.hh"
#include "image/palette_cache.hh"
#include "wad/wad.hh"
#include "console/con_console.h"

extern cvar::FloatVar i_gamma;

namespace {
  static int s_rgb_gamma(int c) {
      float base = c;
      float exp = 1.0 + (0.01f * i_gamma);

      return std::min(std::pow(base, exp), 255.0f);
  }
}

//
// I_TranslatePalette
// Increases the palette RGB based on gamma settings
//

static void I_TranslatePalette(char *data, size_t count, size_t size) {
    if (i_gamma == 0)
        return;

    for(size_t i = 0; i + size - 1 < count * size; i += size) {
        data[i + 0] = s_rgb_gamma(static_cast<uint8>(data[i + 0]));
        data[i + 1] = s_rgb_gamma(static_cast<uint8>(data[i + 1]));
        data[i + 2] = s_rgb_gamma(static_cast<uint8>(data[i + 2]));
    }
}

Image I_ReadImage(int lump, dboolean palette, dboolean nopack, double alpha, int palindex) {
    // get lump data
    auto l = wad::open(lump).value();

    auto image = l.read_image().value();

    if (palindex && image.is_indexed()) {
        auto pal = image.palette();

        char palname[9];
        snprintf(palname, sizeof(palname), "PAL%4.4s%d", l.name().data(), palindex);

        if (wad::exists(palname)) {
            image.set_palette(cache::palette(palname));
        } else {
            log::debug("TODO: safe 16-colour palette swap to #{} in {}", palindex, l.name());
            Palette newpal = {pal.pixel_format(), 16};
            std::copy_n(pal.data_ptr() + palindex * 16 * pal.pixel_info().width, 16 * pal.pixel_info().width, newpal.data_ptr());
            image.set_palette(newpal);
        }
    }

    if (!palette) {
        image.convert(alpha ? PixelFormat::rgba : PixelFormat::rgb);
        if (i_gamma != 0) {
            for (size_t y{}; y < image.height(); ++y) {
                I_TranslatePalette(image[y].data_ptr(), image.width(), image.pixel_info().width);
            }
        }
    }

    return image;
}
