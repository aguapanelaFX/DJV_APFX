// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#pragma once

#include <djv/App/Export.h>

#include <ftk/UI/IMouseWidget.h>

namespace djv
{
    namespace app
    {
        class TagsModel;

        //! APFX: a strip over the timeline showing the tagged segments in
        //! their colors, the in and out points, and the current frame.
        //! Clicking a segment selects it; it cannot be dragged.
        class DJV_APP_API_TYPE SegmentsBar : public ftk::IMouseWidget
        {
            FTK_NON_COPYABLE(SegmentsBar);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TagsModel>&,
                const std::shared_ptr<IWidget>& parent);

            SegmentsBar();

        public:
            DJV_APP_API ~SegmentsBar();

            DJV_APP_API static std::shared_ptr<SegmentsBar> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<TagsModel>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            DJV_APP_API ftk::Size2I getSizeHint() const override;
            DJV_APP_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            DJV_APP_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            DJV_APP_API void mousePressEvent(ftk::MouseClickEvent&) override;

        private:
            int _frameToX(int64_t) const;

            FTK_PRIVATE();
        };
    }
}
