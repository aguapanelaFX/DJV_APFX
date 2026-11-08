// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/App/TagsTool.h>

#include <djv/App/App.h>
#include <djv/App/TagsModel.h>

#include <ftk/UI/Action.h>
#include <ftk/UI/ColorSwatch.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/ComboBox.h>
#include <ftk/UI/Divider.h>
#include <ftk/UI/IntEdit.h>
#include <ftk/UI/Label.h>
#include <ftk/UI/LineEdit.h>
#include <ftk/UI/PushButton.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ToolButton.h>
#include <ftk/Core/Format.h>

namespace djv
{
    namespace app
    {
        namespace
        {
            void clearLayout(const std::shared_ptr<ftk::IWidget>& layout)
            {
                const auto children = layout->getChildren();
                for (const auto& child : children)
                {
                    child->setParent(nullptr);
                }
            }
        }

        struct TagsTool::Private
        {
            std::shared_ptr<TagsModel> model;
            std::vector<Tag> tags;
            std::vector<Segment> segments;
            int selected = -1;
            std::pair<int64_t, int64_t> range = { 0, 0 };
            bool tagsRebuild = true;
            bool segmentsRebuild = true;

            std::shared_ptr<ftk::VerticalLayout> tagsLayout;
            std::shared_ptr<ftk::PushButton> newTagButton;
            std::shared_ptr<ftk::PushButton> undoButton;
            std::shared_ptr<ftk::Label> inOutLabel;
            std::shared_ptr<ftk::VerticalLayout> segmentsLayout;
            std::shared_ptr<ftk::PushButton> nukeButton;
            std::shared_ptr<ftk::PushButton> solvfxButton;
            std::shared_ptr<ftk::PushButton> edlButton;
            std::shared_ptr<ftk::PushButton> xmlButton;
            std::shared_ptr<ftk::Menu> menu;
            std::shared_ptr<ftk::Label> exportLabel;

            std::shared_ptr<ftk::Observer<std::vector<Tag> > > tagsObserver;
            std::shared_ptr<ftk::Observer<std::vector<Segment> > > segmentsObserver;
            std::shared_ptr<ftk::Observer<int> > selectedObserver;
            std::shared_ptr<ftk::Observer<std::pair<int64_t, int64_t> > > rangeObserver;
            std::shared_ptr<ftk::Observer<std::pair<int64_t, int64_t> > > inOutObserver;
        };

        void TagsTool::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<MainWindow>& mainWindow,
            const std::shared_ptr<IWidget>& parent)
        {
            IToolWidget::_init(
                context,
                app,
                mainWindow,
                "Tags",
                "Review",
                "djv::app::TagsTool",
                parent);
            FTK_P();

            p.model = app->getTagsModel();

            auto layout = ftk::VerticalLayout::create(context);
            layout->setMarginRole(ftk::SizeRole::MarginSmall);
            layout->setSpacingRole(ftk::SizeRole::SpacingSmall);

            ftk::Label::create(context, "Tags", layout);
            p.tagsLayout = ftk::VerticalLayout::create(context, layout);
            p.tagsLayout->setSpacingRole(ftk::SizeRole::SpacingTool);
            auto hLayout = ftk::HorizontalLayout::create(context, layout);
            hLayout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            p.newTagButton = ftk::PushButton::create(context, "New tag", hLayout);
            p.undoButton = ftk::PushButton::create(context, "Undo", hLayout);
            p.undoButton->setTooltip("Undo the last change to the tags or segments.");

            ftk::Divider::create(context, ftk::Orientation::Vertical, layout);
            p.inOutLabel = ftk::Label::create(context, layout);
            auto help = ftk::Label::create(
                context,
                "Set the range with I and O, then press + on a tag.\n"
                "Select a segment to edit it with I and O.",
                layout);
            help->setTextRole(ftk::ColorRole::TextDisabled);

            ftk::Divider::create(context, ftk::Orientation::Vertical, layout);
            ftk::Label::create(context, "Segments", layout);
            p.segmentsLayout = ftk::VerticalLayout::create(context, layout);
            p.segmentsLayout->setSpacingRole(ftk::SizeRole::SpacingTool);

            ftk::Divider::create(context, ftk::Orientation::Vertical, layout);
            hLayout = ftk::HorizontalLayout::create(context, layout);
            hLayout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            p.nukeButton = ftk::PushButton::create(context, "Export Nuke", hLayout);
            p.nukeButton->setTooltip("Write <file>_tags.nk next to the file.");
            p.solvfxButton = ftk::PushButton::create(context, "Export SOLVFX", hLayout);
            p.solvfxButton->setTooltip(
                "Write <file>_solvfx.py next to the file.\n"
                "Run it with SOLVFX.exe -l <script> to build the project.");
            hLayout = ftk::HorizontalLayout::create(context, layout);
            hLayout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            p.edlButton = ftk::PushButton::create(context, "Export EDL", hLayout);
            p.edlButton->setTooltip("CMX 3600 EDL for Premiere, Resolve and Avid, as cuts or markers.");
            p.xmlButton = ftk::PushButton::create(context, "Export XML", hLayout);
            p.xmlButton->setTooltip("Final Cut Pro 7 XML for Premiere and Resolve, as cuts or markers.");
            p.exportLabel = ftk::Label::create(context, layout);

            _setWidget(layout);

            std::weak_ptr<TagsModel> modelWeak(p.model);
            p.newTagButton->setClickedCallback(
                [modelWeak]
                {
                    if (auto model = modelWeak.lock())
                        model->addTag();
                });
            p.undoButton->setClickedCallback(
                [modelWeak]
                {
                    if (auto model = modelWeak.lock())
                        model->undo();
                });
            p.nukeButton->setClickedCallback(
                [this]
                {
                    _p->exportLabel->setText(_p->model->exportNuke());
                });
            p.solvfxButton->setClickedCallback(
                [this]
                {
                    _p->exportLabel->setText(_p->model->exportSOLVFX());
                });

            // Cuts or markers, asked in a menu under the button.
            auto askCutsOrMarkers = [this](
                const std::shared_ptr<ftk::PushButton>& button,
                const std::function<std::string(bool)>& exportFnc)
            {
                FTK_P();
                auto context = getContext();
                if (!context)
                    return;
                if (p.menu)
                {
                    p.menu->close();
                    return;
                }
                p.menu = ftk::Menu::create(context);
                p.menu->addAction(ftk::Action::create(
                    "As cuts (only the tagged shots)",
                    [this, exportFnc]
                    {
                        _p->exportLabel->setText(exportFnc(false));
                    }));
                p.menu->addAction(ftk::Action::create(
                    "As markers (whole cut, a marker per shot)",
                    [this, exportFnc]
                    {
                        _p->exportLabel->setText(exportFnc(true));
                    }));
                p.menu->setCloseCallback(
                    [this]
                    {
                        _p->menu.reset();
                    });
                p.menu->open(getWindow(), button->getGeometry());
            };
            p.edlButton->setClickedCallback(
                [this, askCutsOrMarkers]
                {
                    auto model = _p->model;
                    askCutsOrMarkers(
                        _p->edlButton,
                        [model](bool markers) { return model->exportEDL(markers); });
                });
            p.xmlButton->setClickedCallback(
                [this, askCutsOrMarkers]
                {
                    auto model = _p->model;
                    askCutsOrMarkers(
                        _p->xmlButton,
                        [model](bool markers) { return model->exportXML(markers); });
                });

            p.tagsObserver = ftk::Observer<std::vector<Tag> >::create(
                p.model->observeTags(),
                [this](const std::vector<Tag>& value)
                {
                    _p->tags = value;
                    _p->tagsRebuild = true;
                    _p->segmentsRebuild = true;
                });
            p.segmentsObserver = ftk::Observer<std::vector<Segment> >::create(
                p.model->observeSegments(),
                [this](const std::vector<Segment>& value)
                {
                    _p->segments = value;
                    _p->segmentsRebuild = true;
                });
            p.selectedObserver = ftk::Observer<int>::create(
                p.model->observeSelected(),
                [this](int value)
                {
                    _p->selected = value;
                    _p->segmentsRebuild = true;
                });
            p.rangeObserver = ftk::Observer<std::pair<int64_t, int64_t> >::create(
                p.model->observeRange(),
                [this](const std::pair<int64_t, int64_t>& value)
                {
                    _p->range = value;
                    _p->segmentsRebuild = true;
                });
            p.inOutObserver = ftk::Observer<std::pair<int64_t, int64_t> >::create(
                p.model->observeInOut(),
                [this](const std::pair<int64_t, int64_t>& value)
                {
                    _p->inOutLabel->setText(ftk::Format("In: {0}   Out: {1}   ({2} frames)").
                        arg(std::to_string(value.first)).
                        arg(std::to_string(value.second)).
                        arg(std::to_string(value.second - value.first + 1)).str());
                });
        }

        TagsTool::TagsTool() :
            _p(new Private)
        {}

        TagsTool::~TagsTool()
        {}

        std::shared_ptr<TagsTool> TagsTool::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<MainWindow>& mainWindow,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TagsTool>(new TagsTool);
            out->_init(context, app, mainWindow, parent);
            return out;
        }

        void TagsTool::tickEvent(
            bool parentsVisible,
            bool parentsEnabled,
            const ftk::TickEvent& event)
        {
            IToolWidget::tickEvent(parentsVisible, parentsEnabled, event);
            FTK_P();
            // Rebuilt here rather than in the observers: a row's own
            // callback changes the model, and the row must not be destroyed
            // while its callback is running.
            if (p.tagsRebuild)
            {
                p.tagsRebuild = false;
                _tagsRebuild();
            }
            if (p.segmentsRebuild)
            {
                p.segmentsRebuild = false;
                _segmentsRebuild();
            }
        }

        void TagsTool::_tagsRebuild()
        {
            FTK_P();
            auto context = getContext();
            if (!context)
                return;
            clearLayout(p.tagsLayout);
            std::weak_ptr<TagsModel> modelWeak(p.model);
            for (size_t i = 0; i < p.tags.size(); ++i)
            {
                const int index = static_cast<int>(i);
                const Tag tag = p.tags[i];
                auto row = ftk::HorizontalLayout::create(context, p.tagsLayout);
                row->setSpacingRole(ftk::SizeRole::SpacingSmall);

                auto swatch = ftk::ColorSwatch::create(context, row);
                swatch->setColor(tag.color);
                swatch->setEditable(true);
                swatch->setTooltip("Click to change the color.");
                swatch->setCallback(
                    [modelWeak, index, tag](const ftk::Color4F& value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Tag t = tag;
                            t.color = value;
                            model->setTag(index, t);
                        }
                    });

                auto nameEdit = ftk::LineEdit::create(context, row);
                nameEdit->setHStretch(ftk::Stretch::Expanding);
                nameEdit->setText(tag.name);
                nameEdit->setCallbackOnFocusLost(true);
                nameEdit->setCallback(
                    [modelWeak, index, tag](const std::string& value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Tag t = tag;
                            t.name = value;
                            model->setTag(index, t);
                        }
                    });

                auto addButton = ftk::ToolButton::create(context, row);
                addButton->setIcon("AddSmall");
                addButton->setTooltip("Add the in and out range to this tag.");
                addButton->setClickedCallback(
                    [modelWeak, index]
                    {
                        if (auto model = modelWeak.lock())
                            model->addSegment(index);
                    });

                auto removeButton = ftk::ToolButton::create(context, row);
                removeButton->setIcon("CloseSmall");
                removeButton->setTooltip("Remove the tag and its segments.");
                removeButton->setClickedCallback(
                    [modelWeak, index]
                    {
                        if (auto model = modelWeak.lock())
                            model->removeTag(index);
                    });
            }
        }

        void TagsTool::_segmentsRebuild()
        {
            FTK_P();
            auto context = getContext();
            if (!context)
                return;
            clearLayout(p.segmentsLayout);
            std::vector<std::string> tagNames;
            for (const auto& tag : p.tags)
            {
                tagNames.push_back(tag.name);
            }
            std::weak_ptr<TagsModel> modelWeak(p.model);
            for (size_t i = 0; i < p.segments.size(); ++i)
            {
                const int index = static_cast<int>(i);
                const Segment segment = p.segments[i];
                auto row = ftk::HorizontalLayout::create(context, p.segmentsLayout);
                row->setSpacingRole(ftk::SizeRole::SpacingSmall);

                auto selectButton = ftk::ToolButton::create(context, row);
                selectButton->setCheckable(true);
                selectButton->setChecked(index == p.selected);
                selectButton->setIcon("Next");
                selectButton->setTooltip(
                    "Select: jump to the segment and set the in and out points to it.\n"
                    "While selected, I and O edit its start and end.");
                selectButton->setCheckedCallback(
                    [modelWeak, index](bool value)
                    {
                        if (auto model = modelWeak.lock())
                            model->select(value ? index : -1);
                    });

                auto swatch = ftk::ColorSwatch::create(context, row);
                swatch->setColor(segment.tag >= 0 && segment.tag < static_cast<int>(p.tags.size()) ?
                    p.tags[segment.tag].color :
                    ftk::Color4F(.5F, .5F, .5F));

                auto nameEdit = ftk::LineEdit::create(context, row);
                nameEdit->setHStretch(ftk::Stretch::Expanding);
                nameEdit->setText(segment.name);
                nameEdit->setCallbackOnFocusLost(true);
                nameEdit->setCallback(
                    [modelWeak, index, segment](const std::string& value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Segment s = segment;
                            s.name = value;
                            model->setSegment(index, s);
                        }
                    });

                auto startEdit = ftk::IntEdit::create(context, row);
                startEdit->setRange(static_cast<int>(p.range.first), static_cast<int>(p.range.second));
                startEdit->setValue(static_cast<int>(segment.start));
                startEdit->setTooltip("First frame.");
                startEdit->setCallback(
                    [modelWeak, index, segment](int value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Segment s = segment;
                            s.start = value;
                            model->setSegment(index, s);
                        }
                    });

                auto endEdit = ftk::IntEdit::create(context, row);
                endEdit->setRange(static_cast<int>(p.range.first), static_cast<int>(p.range.second));
                endEdit->setValue(static_cast<int>(segment.end));
                endEdit->setTooltip("Last frame.");
                endEdit->setCallback(
                    [modelWeak, index, segment](int value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Segment s = segment;
                            s.end = value;
                            model->setSegment(index, s);
                        }
                    });

                auto tagComboBox = ftk::ComboBox::create(context, tagNames, row);
                tagComboBox->setCurrentIndex(segment.tag);
                tagComboBox->setTooltip("The segment's tag.");
                tagComboBox->setIndexCallback(
                    [modelWeak, index, segment](int value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Segment s = segment;
                            s.tag = value;
                            model->setSegment(index, s);
                        }
                    });

                auto removeButton = ftk::ToolButton::create(context, row);
                removeButton->setIcon("CloseSmall");
                removeButton->setTooltip("Remove the segment.");
                removeButton->setClickedCallback(
                    [modelWeak, index]
                    {
                        if (auto model = modelWeak.lock())
                            model->removeSegment(index);
                    });
            }
            p.nukeButton->setEnabled(!p.segments.empty());
            p.solvfxButton->setEnabled(!p.segments.empty());
            p.edlButton->setEnabled(!p.segments.empty());
            p.xmlButton->setEnabled(!p.segments.empty());
        }
    }
}
