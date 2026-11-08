// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#pragma once

#include <djv/App/Export.h>

#include <ftk/UI/IContainer.h>

namespace djv
{
    namespace app
    {
        class App;

        //! APFX viewer color bar, like the top of the Nuke viewer: the OCIO
        //! display and view, then exposure, gamma and saturation.
        class DJV_APP_API_TYPE ColorToolBar : public ftk::IContainer
        {
            FTK_NON_COPYABLE(ColorToolBar);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<IWidget>& parent);

            ColorToolBar();

        public:
            DJV_APP_API ~ColorToolBar();

            DJV_APP_API static std::shared_ptr<ColorToolBar> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

        private:
            FTK_PRIVATE();
        };
    }
}
