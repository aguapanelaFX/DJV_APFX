// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/App/TagsTool.h>

#include <djv/App/App.h>
#include <djv/App/TagsModel.h>

#include <ftk/UI/Action.h>
#include <ftk/UI/ColorSwatch.h>
#include <ftk/UI/FileBrowser.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/ComboBox.h>
#include <ftk/UI/Divider.h>
#include <ftk/UI/IntEdit.h>
#include <ftk/UI/Label.h>
#include <ftk/UI/LineEdit.h>
#include <ftk/UI/PushButton.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ToolButton.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Path.h>

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
            std::shared_ptr<ftk::Label> markLabel;
            std::shared_ptr<ftk::PushButton> markStartButton;
            std::shared_ptr<ftk::PushButton> markEndButton;
            std::shared_ptr<ftk::PushButton> clearMarkButton;
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
            std::shared_ptr<ftk::Observer<SegmentMark> > markObserver;
            SegmentMark mark;
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
            hLayout = ftk::HorizontalLayout::create(context, layout);
            hLayout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            p.markStartButton = ftk::PushButton::create(context, "Start  [", hLayout);
            p.markStartButton->setTooltip("Mark the segment start at the current frame ([ key).");
            p.markEndButton = ftk::PushButton::create(context, "End  ]", hLayout);
            p.markEndButton->setTooltip("Mark the segment end at the current frame (] key).");
            p.clearMarkButton = ftk::PushButton::create(context, "Clear", hLayout);
            p.clearMarkButton->setTooltip("Clear the mark and deselect the segment.");
            p.markLabel = ftk::Label::create(context, layout);
            auto help = ftk::Label::create(
                context,
                "Mark with [ and ], then press + on a tag.\n"
                "Select a segment to edit it with [ and ].\n"
                "I and O stay the normal in and out points.",
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
                    auto model = _p->model;
                    _saveDialog(
                        "Export Nuke Script",
                        model->getExportPath("_tags.nk"),
                        ".nk",
                        "Nuke Script",
                        [model](const std::string& path) { return model->exportNuke(path); });
                });
            p.solvfxButton->setClickedCallback(
                [this]
                {
                    auto model = _p->model;
                    _saveDialog(
                        "Export SOLVFX Script",
                        model->getExportPath("_solvfx.py"),
                        ".py",
                        "SOLVFX Python Script",
                        [model](const std::string& path) { return model->exportSOLVFX(path); });
                });

            // Cuts or markers, asked in a menu under the button.
            auto askCutsOrMarkers = [this](
                const std::shared_ptr<ftk::PushButton>& button,
                const std::function<void(bool)>& exportFnc)
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
                    [exportFnc]
                    {
                        exportFnc(false);
                    }));
                p.menu->addAction(ftk::Action::create(
                    "As markers (whole cut, a marker per shot)",
                    [exportFnc]
                    {
                        exportFnc(true);
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
                        [this, model](bool markers)
                        {
                            _saveDialog(
                                "Export EDL",
                                model->getExportPath(markers ? "_markers.edl" : "_cuts.edl"),
                                ".edl",
                                "EDL",
                                [model, markers](const std::string& path) { return model->exportEDL(markers, path); });
                        });
                });
            p.xmlButton->setClickedCallback(
                [this, askCutsOrMarkers]
                {
                    auto model = _p->model;
                    askCutsOrMarkers(
                        _p->xmlButton,
                        [this, model](bool markers)
                        {
                            _saveDialog(
                                "Export XML",
                                model->getExportPath(markers ? "_markers.xml" : "_cuts.xml"),
                                ".xml",
                                "Final Cut Pro XML",
                                [model, markers](const std::string& path) { return model->exportXML(markers, path); });
                        });
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
            p.markObserver = ftk::Observer<SegmentMark>::create(
                p.model->observeMark(),
                [this](const SegmentMark& value)
                {
                    _p->mark = value;
                    _markLabelUpdate();
                });
            p.markStartButton->setClickedCallback(
                [modelWeak]
                {
                    if (auto model = modelWeak.lock())
                        model->markStart();
                });
            p.markEndButton->setClickedCallback(
                [modelWeak]
                {
                    if (auto model = modelWeak.lock())
                        model->markEnd();
                });
            p.clearMarkButton->setClickedCallback(
                [modelWeak]
                {
                    if (auto model = modelWeak.lock())
                    {
                        model->clearMark();
                        model->select(-1);
                    }
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
                _markLabelUpdate();
            }
        }

        void TagsTool::_saveDialog(
            const std::string& title,
            const std::string& defaultPath,
            const std::string& ext,
            const std::string& label,
            const std::function<std::string(const std::string&)>& exportFnc)
        {
            FTK_P();
            auto context = getContext();
            if (!context || defaultPath.empty())
                return;
            // The system's own save dialog (every drive), starting in the
            // media's folder with the suggested name.
            auto fileBrowserSystem = context->getSystem<ftk::FileBrowserSystem>();
            const std::filesystem::path suggested = ftk::toFileSystem(defaultPath);
            ftk::FileBrowserOpenOptions options;
            options.title = title;
            options.path = suggested.parent_path();
            options.mode = ftk::FileBrowserMode::Save;
            options.fileName = ftk::fromFileSystem(suggested.filename());
            options.extensions = { ext };
            options.extensionsLabel = label;
            std::weak_ptr<TagsTool> weak(std::dynamic_pointer_cast<TagsTool>(shared_from_this()));
            fileBrowserSystem->open(
                getWindow(),
                [weak, exportFnc](const ftk::Path& value)
                {
                    const std::string path = value.get();
                    if (path.empty())
                        return;
                    const std::string result = exportFnc(path);
                    if (auto widget = weak.lock())
                    {
                        widget->_p->exportLabel->setText(result);
                    }
                },
                options);
        }

        void TagsTool::_markLabelUpdate()
        {
            FTK_P();
            std::string text;
            if (p.selected >= 0 && p.selected < static_cast<int>(p.segments.size()))
            {
                const Segment& s = p.segments[p.selected];
                text = ftk::Format("Editing {0}: [ and ] move its start and end ({1} - {2})").
                    arg(s.name).
                    arg(std::to_string(s.start)).
                    arg(std::to_string(s.end)).str();
            }
            else if (p.mark.hasStart)
            {
                text = ftk::Format("Marked {0} - {1} ({2} frames): press + on a tag").
                    arg(std::to_string(p.mark.start)).
                    arg(std::to_string(p.mark.end)).
                    arg(std::to_string(p.mark.end - p.mark.start + 1)).str();
            }
            else
            {
                text = "No range marked. Press [ and ].";
            }
            p.markLabel->setText(text);
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
                addButton->setTooltip("Add the marked range to this tag.");
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
                auto box = ftk::VerticalLayout::create(context, p.segmentsLayout);
                box->setSpacingRole(ftk::SizeRole::SpacingTool);

                // Line 1: select, color, name, tag, remove.
                auto row = ftk::HorizontalLayout::create(context, box);
                row->setSpacingRole(ftk::SizeRole::SpacingSmall);

                auto selectButton = ftk::ToolButton::create(context, row);
                selectButton->setCheckable(true);
                selectButton->setChecked(index == p.selected);
                selectButton->setIcon("Next");
                selectButton->setTooltip(
                    "Select: jump to the segment.\n"
                    "While selected, [ and ] set its start and end.");
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

                // Line 2: frames.
                row = ftk::HorizontalLayout::create(context, box);
                row->setSpacingRole(ftk::SizeRole::SpacingSmall);
                row->setMarginRole(ftk::SizeRole::None);
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
                ftk::Label::create(context, "-", row);
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
                auto lengthLabel = ftk::Label::create(
                    context,
                    std::to_string(segment.end - segment.start + 1) + " fr",
                    row);
                lengthLabel->setTextRole(ftk::ColorRole::TextDisabled);

                // Line 3: note.
                auto noteEdit = ftk::LineEdit::create(context, box);
                noteEdit->setHStretch(ftk::Stretch::Expanding);
                noteEdit->setText(segment.note);
                noteEdit->setTooltip("Note, exported to Nuke, SOLVFX, the EDL and the XML.");
                noteEdit->setCallbackOnFocusLost(true);
                noteEdit->setCallback(
                    [modelWeak, index, segment](const std::string& value)
                    {
                        if (auto model = modelWeak.lock())
                        {
                            Segment s = segment;
                            s.note = value;
                            model->setSegment(index, s);
                        }
                    });

                if (i + 1 < p.segments.size())
                {
                    ftk::Divider::create(context, ftk::Orientation::Vertical, p.segmentsLayout);
                }
            }
            p.nukeButton->setEnabled(!p.segments.empty());
            p.solvfxButton->setEnabled(!p.segments.empty());
            p.edlButton->setEnabled(!p.segments.empty());
            p.xmlButton->setEnabled(!p.segments.empty());
        }
    }
}
