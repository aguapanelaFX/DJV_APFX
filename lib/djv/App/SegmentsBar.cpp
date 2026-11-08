// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/App/SegmentsBar.h>

#include <djv/App/TagsModel.h>

#include <ftk/UI/DrawUtil.h>

#include <algorithm>

namespace djv
{
    namespace app
    {
        struct SegmentsBar::Private
        {
            std::weak_ptr<TagsModel> model;
            std::vector<Tag> tags;
            std::vector<Segment> segments;
            int selected = -1;
            std::pair<int64_t, int64_t> range = { 0, 0 };
            SegmentMark mark;
            int64_t currentFrame = 0;
            int height = 14;
            int border = 1;

            std::shared_ptr<ftk::Observer<std::vector<Tag> > > tagsObserver;
            std::shared_ptr<ftk::Observer<std::vector<Segment> > > segmentsObserver;
            std::shared_ptr<ftk::Observer<int> > selectedObserver;
            std::shared_ptr<ftk::Observer<std::pair<int64_t, int64_t> > > rangeObserver;
            std::shared_ptr<ftk::Observer<SegmentMark> > markObserver;
            std::shared_ptr<ftk::Observer<int64_t> > currentFrameObserver;
            std::shared_ptr<ftk::Observer<bool> > hasFileObserver;
        };

        void SegmentsBar::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TagsModel>& model,
            const std::shared_ptr<IWidget>& parent)
        {
            IMouseWidget::_init(context, "djv::app::SegmentsBar", parent);
            FTK_P();
            setHStretch(ftk::Stretch::Expanding);
            _setMousePressEnabled(true);
            setTooltip("Tagged segments. Click one to select it.");
            p.model = model;

            p.tagsObserver = ftk::Observer<std::vector<Tag> >::create(
                model->observeTags(),
                [this](const std::vector<Tag>& value)
                {
                    _p->tags = value;
                    setDrawUpdate();
                });
            p.segmentsObserver = ftk::Observer<std::vector<Segment> >::create(
                model->observeSegments(),
                [this](const std::vector<Segment>& value)
                {
                    _p->segments = value;
                    setDrawUpdate();
                });
            p.selectedObserver = ftk::Observer<int>::create(
                model->observeSelected(),
                [this](int value)
                {
                    _p->selected = value;
                    setDrawUpdate();
                });
            p.rangeObserver = ftk::Observer<std::pair<int64_t, int64_t> >::create(
                model->observeRange(),
                [this](const std::pair<int64_t, int64_t>& value)
                {
                    _p->range = value;
                    setDrawUpdate();
                });
            p.markObserver = ftk::Observer<SegmentMark>::create(
                model->observeMark(),
                [this](const SegmentMark& value)
                {
                    _p->mark = value;
                    setDrawUpdate();
                });
            p.currentFrameObserver = ftk::Observer<int64_t>::create(
                model->observeCurrentFrame(),
                [this](int64_t value)
                {
                    _p->currentFrame = value;
                    setDrawUpdate();
                });
            p.hasFileObserver = ftk::Observer<bool>::create(
                model->observeHasFile(),
                [this](bool value)
                {
                    setVisible(value);
                });
        }

        SegmentsBar::SegmentsBar() :
            _p(new Private)
        {}

        SegmentsBar::~SegmentsBar()
        {}

        std::shared_ptr<SegmentsBar> SegmentsBar::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TagsModel>& model,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<SegmentsBar>(new SegmentsBar);
            out->_init(context, model, parent);
            return out;
        }

        ftk::Size2I SegmentsBar::getSizeHint() const
        {
            return ftk::Size2I(100, _p->height);
        }

        void SegmentsBar::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            IMouseWidget::sizeHintEvent(event);
            FTK_P();
            p.height = static_cast<int>(14 * event.displayScale);
            p.border = std::max(1, static_cast<int>(event.displayScale));
        }

        int SegmentsBar::_frameToX(int64_t frame) const
        {
            FTK_P();
            const ftk::Box2I& g = getGeometry();
            const int64_t duration = p.range.second - p.range.first + 1;
            if (duration <= 0)
                return g.min.x;
            return g.min.x + static_cast<int>(
                (frame - p.range.first) * static_cast<double>(g.w()) / duration);
        }

        void SegmentsBar::drawEvent(const ftk::Box2I& drawRect, const ftk::DrawEvent& event)
        {
            IMouseWidget::drawEvent(drawRect, event);
            FTK_P();
            const ftk::Box2I& g = getGeometry();
            event.render->drawRect(g, event.style->getColorRole(ftk::ColorRole::Base));

            const int inset = std::max(1, g.h() / 5);
            for (size_t i = 0; i < p.segments.size(); ++i)
            {
                const Segment& s = p.segments[i];
                const ftk::Color4F color = s.tag >= 0 && s.tag < static_cast<int>(p.tags.size()) ?
                    p.tags[s.tag].color :
                    ftk::Color4F(.5F, .5F, .5F);
                const int x0 = _frameToX(s.start);
                const int x1 = _frameToX(s.end + 1);
                const ftk::Box2I box(x0, g.min.y + inset, std::max(2, x1 - x0), g.h() - inset * 2);
                if (static_cast<int>(i) == p.selected)
                {
                    event.render->drawRect(
                        ftk::Box2I(box.min.x - p.border, g.min.y, box.w() + p.border * 2, g.h()),
                        ftk::Color4F(1.F, 1.F, 1.F));
                }
                event.render->drawRect(box, color);
            }

            // The range being marked, not yet on a tag: gray with stripes.
            if (p.mark.hasStart)
            {
                const int x0 = _frameToX(p.mark.start);
                const int x1 = std::max(x0 + 2, _frameToX((p.mark.hasEnd ? p.mark.end : p.mark.start) + 1));
                const ftk::Box2I box(x0, g.min.y + inset, x1 - x0, g.h() - inset * 2);
                event.render->drawRect(box, ftk::Color4F(.45F, .45F, .45F));
                const int stripe = std::max(2, g.h() / 3);
                for (int x = box.min.x; x < box.max.x; x += stripe * 2)
                {
                    event.render->drawRect(
                        ftk::Box2I(x, box.min.y, std::min(stripe, box.max.x - x + 1), box.h()),
                        ftk::Color4F(.65F, .65F, .65F));
                }
            }

            // Current frame.
            const int x = _frameToX(p.currentFrame);
            event.render->drawRect(
                ftk::Box2I(x, g.min.y, p.border * 2, g.h()),
                event.style->getColorRole(ftk::ColorRole::Text));
        }

        void SegmentsBar::mousePressEvent(ftk::MouseClickEvent& event)
        {
            IMouseWidget::mousePressEvent(event);
            FTK_P();
            event.accept = true;
            int hit = -1;
            for (size_t i = 0; i < p.segments.size(); ++i)
            {
                const int x0 = _frameToX(p.segments[i].start);
                const int x1 = std::max(x0 + 2, _frameToX(p.segments[i].end + 1));
                if (event.pos.x >= x0 && event.pos.x < x1)
                {
                    hit = static_cast<int>(i);
                }
            }
            if (auto model = p.model.lock())
            {
                model->select(hit == p.selected ? -1 : hit);
            }
        }
    }
}
