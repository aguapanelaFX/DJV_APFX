// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#pragma once

#include <djv/App/Export.h>
#include <djv/App/IToolWidget.h>

namespace djv
{
    namespace app
    {
        //! APFX tags tool: tags with colors, marking segments with the in
        //! and out points, the segment list, and the Nuke and SOLVFX
        //! exports.
        class DJV_APP_API_TYPE TagsTool : public IToolWidget
        {
            FTK_NON_COPYABLE(TagsTool);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<MainWindow>&,
                const std::shared_ptr<IWidget>& parent);

            TagsTool();

        public:
            DJV_APP_API virtual ~TagsTool();

            DJV_APP_API static std::shared_ptr<TagsTool> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<MainWindow>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            DJV_APP_API void tickEvent(bool, bool, const ftk::TickEvent&) override;

        private:
            void _tagsRebuild();
            void _segmentsRebuild();

            FTK_PRIVATE();
        };
    }
}
