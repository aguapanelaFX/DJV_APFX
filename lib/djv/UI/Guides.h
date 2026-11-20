// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#pragma once

namespace djv
{
    namespace ui
    {
        //! APFX viewer guides: an aspect ratio mask and safe areas, drawn
        //! over each picture shown.
        struct Guides
        {
            //! Mask aspect ratio (2.39, 1.85, 1.78...), or 0 for none.
            float maskAspect  = 0.F;

            //! Mask opacity, 0 to 1.
            float maskOpacity = .75F;

            //! A line where the mask meets the picture.
            bool  maskLine    = true;

            //! Action safe (90%) and title safe (80%), inside the mask.
            bool  actionSafe  = false;
            bool  titleSafe   = false;

            bool operator == (const Guides&) const = default;
        };
    }
}
