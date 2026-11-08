// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#pragma once

#include <djv/UI/Export.h>

#include <ftk/UI/IContainer.h>

#include <functional>
#include <string>
#include <vector>

namespace djv
{
    namespace ui
    {
        //! APFX: a color space picker that opens a menu grouped by the
        //! OCIO family, like Nuke (ACES, Input > ARRI, Output, Utility).
        //! Drop-in for a ComboBox: items, an index, an index callback.
        class DJV_UI_API_TYPE ColorSpaceButton : public ftk::IContainer
        {
            FTK_NON_COPYABLE(ColorSpaceButton);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent);

            ColorSpaceButton();

        public:
            DJV_UI_API ~ColorSpaceButton();

            DJV_UI_API static std::shared_ptr<ColorSpaceButton> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Set the items and their families ("Input/ARRI"); an item
            //! with no family goes at the top level of the menu.
            DJV_UI_API void setItems(
                const std::vector<std::string>& items,
                const std::vector<std::string>& families);

            DJV_UI_API int getCurrentIndex() const;
            DJV_UI_API void setCurrentIndex(int);
            DJV_UI_API void setIndexCallback(const std::function<void(int)>&);

        private:
            void _textUpdate();
            void _open();

            FTK_PRIVATE();
        };
    }
}
