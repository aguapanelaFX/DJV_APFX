// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/App/TagsModel.h>

#include <djv/App/App.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/Path.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace djv
{
    namespace app
    {
        namespace
        {
            struct Snapshot
            {
                std::vector<Tag> tags;
                std::vector<Segment> segments;
            };

            std::string forwardSlashes(std::string value)
            {
                std::replace(value.begin(), value.end(), '\\', '/');
                return value;
            }

            //! Names safe for Nuke and SOLVFX script names.
            std::string scriptName(const std::string& value)
            {
                std::string out;
                for (char c : value)
                {
                    out += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
                }
                if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0])))
                {
                    out = "s_" + out;
                }
                return out;
            }

            std::string quoted(const std::string& value)
            {
                std::string out = "\"";
                for (char c : value)
                {
                    if ('\n' == c)
                    {
                        out += "\\n";
                        continue;
                    }
                    if ('"' == c || '\\' == c)
                        out += '\\';
                    out += c;
                }
                return out + "\"";
            }

            uint8_t to8(float value)
            {
                return static_cast<uint8_t>(std::round(std::clamp(value, 0.F, 1.F) * 255.F));
            }

            std::string hexColor(const ftk::Color4F& c)
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%02x%02x%02x", to8(c.r), to8(c.g), to8(c.b));
                return buf;
            }
        }

        struct TagsModel::Private
        {
            std::weak_ptr<ftk::Context> context;
            std::weak_ptr<App> app;
            std::shared_ptr<tl::Player> player;

            std::shared_ptr<ftk::Observable<std::vector<Tag> > > tags;
            std::shared_ptr<ftk::Observable<std::vector<Segment> > > segments;
            std::shared_ptr<ftk::Observable<int> > selected;
            std::shared_ptr<ftk::Observable<bool> > hasFile;
            std::shared_ptr<ftk::Observable<std::pair<int64_t, int64_t> > > range;
            std::shared_ptr<ftk::Observable<int64_t> > currentFrame;
            std::shared_ptr<ftk::Observable<std::pair<int64_t, int64_t> > > inOut;

            std::vector<Snapshot> undo;
            double rate = 24.0;
            ftk::Path path;
            bool settingInOut = false;

            std::shared_ptr<ftk::Observer<std::shared_ptr<tl::Player> > > playerObserver;
            std::shared_ptr<ftk::Observer<OTIO_NS::RationalTime> > currentTimeObserver;
            std::shared_ptr<ftk::Observer<OTIO_NS::TimeRange> > inOutObserver;
        };

        void TagsModel::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            FTK_P();
            p.context = context;
            p.app = app;

            std::vector<Tag> tags;
            const auto& colors = getPresetColors();
            const std::vector<std::string> names =
            {
                "Explosion", "Muzzle flash", "Cleanup easy", "Cleanup hard", "Beauty"
            };
            for (size_t i = 0; i < names.size(); ++i)
            {
                tags.push_back({ names[i], colors[i % colors.size()] });
            }
            p.tags = ftk::Observable<std::vector<Tag> >::create(tags);
            p.segments = ftk::Observable<std::vector<Segment> >::create();
            p.selected = ftk::Observable<int>::create(-1);
            p.hasFile = ftk::Observable<bool>::create(false);
            p.range = ftk::Observable<std::pair<int64_t, int64_t> >::create(
                std::make_pair<int64_t, int64_t>(0, 0));
            p.currentFrame = ftk::Observable<int64_t>::create(0);
            p.inOut = ftk::Observable<std::pair<int64_t, int64_t> >::create(
                std::make_pair<int64_t, int64_t>(0, 0));

            p.playerObserver = ftk::Observer<std::shared_ptr<tl::Player> >::create(
                app->observePlayer(),
                [this](const std::shared_ptr<tl::Player>& value)
                {
                    _p->player = value;
                    _playerUpdate();
                });
        }

        TagsModel::TagsModel() :
            _p(new Private)
        {}

        TagsModel::~TagsModel()
        {}

        std::shared_ptr<TagsModel> TagsModel::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            auto out = std::shared_ptr<TagsModel>(new TagsModel);
            out->_init(context, app);
            return out;
        }

        const std::vector<ftk::Color4F>& TagsModel::getPresetColors()
        {
            static const std::vector<ftk::Color4F> colors =
            {
                ftk::Color4F(.89F, .29F, .29F),
                ftk::Color4F(.94F, .62F, .15F),
                ftk::Color4F(.50F, .47F, .87F),
                ftk::Color4F(.84F, .33F, .49F),
                ftk::Color4F(.11F, .62F, .46F),
                ftk::Color4F(.22F, .54F, .87F),
                ftk::Color4F(.39F, .60F, .13F),
                ftk::Color4F(.85F, .35F, .19F),
                ftk::Color4F(.53F, .53F, .50F)
            };
            return colors;
        }

        std::shared_ptr<ftk::IObservable<std::vector<Tag> > > TagsModel::observeTags() const
        {
            return _p->tags;
        }

        std::shared_ptr<ftk::IObservable<std::vector<Segment> > > TagsModel::observeSegments() const
        {
            return _p->segments;
        }

        std::shared_ptr<ftk::IObservable<int> > TagsModel::observeSelected() const
        {
            return _p->selected;
        }

        std::shared_ptr<ftk::IObservable<bool> > TagsModel::observeHasFile() const
        {
            return _p->hasFile;
        }

        std::shared_ptr<ftk::IObservable<std::pair<int64_t, int64_t> > > TagsModel::observeRange() const
        {
            return _p->range;
        }

        std::shared_ptr<ftk::IObservable<int64_t> > TagsModel::observeCurrentFrame() const
        {
            return _p->currentFrame;
        }

        std::shared_ptr<ftk::IObservable<std::pair<int64_t, int64_t> > > TagsModel::observeInOut() const
        {
            return _p->inOut;
        }

        void TagsModel::addTag()
        {
            FTK_P();
            _pushUndo();
            auto tags = p.tags->get();
            const auto& colors = getPresetColors();
            Tag tag;
            tag.name = ftk::Format("Tag {0}").arg(static_cast<int>(tags.size() + 1)).str();
            tag.color = colors[tags.size() % colors.size()];
            tags.push_back(tag);
            p.tags->setIfChanged(tags);
            _save();
        }

        void TagsModel::setTag(int index, const Tag& value)
        {
            FTK_P();
            auto tags = p.tags->get();
            if (index < 0 || index >= static_cast<int>(tags.size()) || tags[index] == value)
                return;
            _pushUndo();
            tags[index] = value;
            p.tags->setIfChanged(tags);
            _save();
        }

        void TagsModel::removeTag(int index)
        {
            FTK_P();
            auto tags = p.tags->get();
            if (index < 0 || index >= static_cast<int>(tags.size()))
                return;
            _pushUndo();
            tags.erase(tags.begin() + index);
            std::vector<Segment> segments;
            for (auto segment : p.segments->get())
            {
                if (segment.tag == index)
                    continue;
                if (segment.tag > index)
                    --segment.tag;
                segments.push_back(segment);
            }
            p.selected->setIfChanged(-1);
            p.tags->setIfChanged(tags);
            p.segments->setIfChanged(segments);
            _save();
        }

        void TagsModel::addSegment(int tag)
        {
            FTK_P();
            if (!p.player || tag < 0 || tag >= static_cast<int>(p.tags->get().size()))
                return;
            _pushUndo();
            const auto inOut = p.inOut->get();
            auto segments = p.segments->get();
            // Names go up in tens, like shot numbers, and are not reused.
            int number = 10;
            for (const auto& segment : segments)
            {
                int n = 0;
                if (std::sscanf(segment.name.c_str(), "sh%d", &n) == 1)
                {
                    number = std::max(number, n + 10);
                }
            }
            Segment segment;
            segment.name = ftk::Format("sh{0}").arg(number, 3, '0').str();
            segment.tag = tag;
            segment.start = inOut.first;
            segment.end = inOut.second;
            segments.push_back(segment);
            p.selected->setIfChanged(-1);
            p.segments->setIfChanged(segments);
            _sort();
            _clearInOut();
            _save();
        }

        void TagsModel::setSegment(int index, const Segment& value)
        {
            FTK_P();
            auto segments = p.segments->get();
            if (index < 0 || index >= static_cast<int>(segments.size()) || segments[index] == value)
                return;
            _pushUndo();
            Segment segment = value;
            if (segment.end < segment.start)
            {
                std::swap(segment.start, segment.end);
            }
            segments[index] = segment;
            p.segments->setIfChanged(segments);
            if (index == p.selected->get() && p.player)
            {
                p.settingInOut = true;
                p.player->setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                    OTIO_NS::RationalTime(segment.start, p.rate),
                    OTIO_NS::RationalTime(segment.end, p.rate)));
                p.settingInOut = false;
            }
            _save();
        }

        void TagsModel::removeSegment(int index)
        {
            FTK_P();
            auto segments = p.segments->get();
            if (index < 0 || index >= static_cast<int>(segments.size()))
                return;
            _pushUndo();
            segments.erase(segments.begin() + index);
            if (index == p.selected->get())
            {
                p.selected->setIfChanged(-1);
                _clearInOut();
            }
            else if (index < p.selected->get())
            {
                p.selected->setIfChanged(p.selected->get() - 1);
            }
            p.segments->setIfChanged(segments);
            _save();
        }

        void TagsModel::select(int index)
        {
            FTK_P();
            const auto& segments = p.segments->get();
            if (index >= static_cast<int>(segments.size()))
            {
                index = -1;
            }
            p.selected->setIfChanged(index);
            if (!p.player)
                return;
            if (index >= 0)
            {
                const Segment& segment = segments[index];
                p.settingInOut = true;
                p.player->setInOutRange(OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                    OTIO_NS::RationalTime(segment.start, p.rate),
                    OTIO_NS::RationalTime(segment.end, p.rate)));
                p.settingInOut = false;
                p.player->seek(OTIO_NS::RationalTime(segment.start, p.rate));
            }
            else
            {
                _clearInOut();
            }
        }

        bool TagsModel::canUndo() const
        {
            return !_p->undo.empty();
        }

        void TagsModel::undo()
        {
            FTK_P();
            if (p.undo.empty())
                return;
            const Snapshot snapshot = p.undo.back();
            p.undo.pop_back();
            p.selected->setIfChanged(-1);
            p.tags->setIfChanged(snapshot.tags);
            p.segments->setIfChanged(snapshot.segments);
            _save();
        }

        void TagsModel::_playerUpdate()
        {
            FTK_P();
            p.undo.clear();
            p.selected->setIfChanged(-1);
            if (p.player)
            {
                p.path = p.player->getPath();
                const OTIO_NS::TimeRange& timeRange = p.player->getTimeRange();
                p.rate = timeRange.duration().rate();
                p.range->setIfChanged(std::make_pair(
                    static_cast<int64_t>(timeRange.start_time().value()),
                    static_cast<int64_t>(timeRange.end_time_inclusive().value())));
                p.hasFile->setIfChanged(true);
                _load();

                p.currentTimeObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                    p.player->observeCurrentTime(),
                    [this](const OTIO_NS::RationalTime& value)
                    {
                        _p->currentFrame->setIfChanged(
                            static_cast<int64_t>(std::floor(value.rescaled_to(_p->rate).value())));
                    });
                p.inOutObserver = ftk::Observer<OTIO_NS::TimeRange>::create(
                    p.player->observeInOutRange(),
                    [this](const OTIO_NS::TimeRange& value)
                    {
                        FTK_P();
                        const std::pair<int64_t, int64_t> inOut(
                            static_cast<int64_t>(std::floor(value.start_time().rescaled_to(p.rate).value())),
                            static_cast<int64_t>(std::floor(value.end_time_inclusive().rescaled_to(p.rate).value())));
                        p.inOut->setIfChanged(inOut);
                        // I and O edit the selected segment.
                        const int selected = p.selected->get();
                        auto segments = p.segments->get();
                        if (!p.settingInOut &&
                            selected >= 0 &&
                            selected < static_cast<int>(segments.size()) &&
                            (segments[selected].start != inOut.first ||
                                segments[selected].end != inOut.second))
                        {
                            _pushUndo();
                            segments[selected].start = inOut.first;
                            segments[selected].end = inOut.second;
                            p.segments->setIfChanged(segments);
                            _save();
                        }
                    });
            }
            else
            {
                p.path = ftk::Path();
                p.currentTimeObserver.reset();
                p.inOutObserver.reset();
                p.hasFile->setIfChanged(false);
                p.segments->setIfChanged(std::vector<Segment>());
            }
        }

        namespace
        {
            std::filesystem::path sidecarPath(const ftk::Path& path)
            {
                std::string out;
                if (path.isSeq())
                {
                    out = path.getDir() + path.getBase();
                    if (!out.empty() && (out.back() == '.' || out.back() == '_'))
                    {
                        out.pop_back();
                    }
                    out += path.getExt() + ".tags.json";
                }
                else
                {
                    out = path.get() + ".tags.json";
                }
                return ftk::toFileSystem(out);
            }

            //! The file's name without the frame number or extension, for
            //! naming exports.
            std::string exportBase(const ftk::Path& path)
            {
                std::string base = path.getBase();
                while (!base.empty() && (base.back() == '.' || base.back() == '_'))
                {
                    base.pop_back();
                }
                return path.getDir() + base;
            }
        }

        void TagsModel::_load()
        {
            FTK_P();
            std::vector<Segment> segments;
            try
            {
                std::ifstream file(sidecarPath(p.path));
                if (file.is_open())
                {
                    nlohmann::json json;
                    file >> json;
                    std::vector<Tag> tags;
                    for (const auto& j : json.at("tags"))
                    {
                        Tag tag;
                        tag.name = j.at("name").get<std::string>();
                        const auto c = j.at("color");
                        tag.color = ftk::Color4F(c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>());
                        tags.push_back(tag);
                    }
                    for (const auto& j : json.at("segments"))
                    {
                        Segment segment;
                        segment.name = j.at("name").get<std::string>();
                        segment.tag = j.at("tag").get<int>();
                        segment.start = j.at("start").get<int64_t>();
                        segment.end = j.at("end").get<int64_t>();
                        segments.push_back(segment);
                    }
                    if (!tags.empty())
                    {
                        p.tags->setIfChanged(tags);
                    }
                }
            }
            catch (const std::exception& e)
            {
                if (auto context = p.context.lock())
                {
                    context->getLogSystem()->print(
                        "djv::app::TagsModel",
                        ftk::Format("Cannot read the tags: {0}").arg(e.what()),
                        ftk::LogType::Error);
                }
            }
            // The tags carry over to a file that has none of its own, so
            // they are set up once per job.
            p.segments->setIfChanged(segments);
        }

        void TagsModel::_save()
        {
            FTK_P();
            if (p.path.isEmpty())
                return;
            try
            {
                nlohmann::json json;
                json["tags"] = nlohmann::json::array();
                for (const auto& tag : p.tags->get())
                {
                    json["tags"].push_back({
                        { "name", tag.name },
                        { "color", { tag.color.r, tag.color.g, tag.color.b } } });
                }
                json["segments"] = nlohmann::json::array();
                for (const auto& segment : p.segments->get())
                {
                    json["segments"].push_back({
                        { "name", segment.name },
                        { "tag", segment.tag },
                        { "start", segment.start },
                        { "end", segment.end } });
                }
                std::ofstream file(sidecarPath(p.path));
                file << json.dump(2);
            }
            catch (const std::exception& e)
            {
                if (auto context = p.context.lock())
                {
                    context->getLogSystem()->print(
                        "djv::app::TagsModel",
                        ftk::Format("Cannot save the tags: {0}").arg(e.what()),
                        ftk::LogType::Error);
                }
            }
        }

        void TagsModel::_pushUndo()
        {
            FTK_P();
            p.undo.push_back({ p.tags->get(), p.segments->get() });
            if (p.undo.size() > 100)
            {
                p.undo.erase(p.undo.begin());
            }
        }

        void TagsModel::_sort()
        {
            FTK_P();
            auto segments = p.segments->get();
            std::stable_sort(
                segments.begin(),
                segments.end(),
                [](const Segment& a, const Segment& b)
                {
                    return a.start < b.start;
                });
            p.segments->setIfChanged(segments);
        }

        void TagsModel::_clearInOut()
        {
            FTK_P();
            if (p.player)
            {
                p.settingInOut = true;
                p.player->setInOutRange(p.player->getTimeRange());
                p.settingInOut = false;
            }
        }

        std::string TagsModel::exportNuke()
        {
            FTK_P();
            if (p.path.isEmpty())
                return "Error: no file is open.";
            const auto& tags = p.tags->get();
            const auto& segments = p.segments->get();
            const auto range = p.range->get();

            // Movies start at frame 1 in Nuke; image sequences keep their
            // own frame numbers.
            const bool seq = p.path.isSeq();
            const int64_t offset = seq ? 0 : 1 - range.first;
            std::string file;
            if (seq)
            {
                const int pad = p.path.getPad();
                file = p.path.getDir() + p.path.getBase() +
                    (pad > 0 ? ftk::Format("%0{0}d").arg(pad).str() : std::string("%d")) +
                    p.path.getExt();
            }
            else
            {
                file = p.path.get();
            }
            file = forwardSlashes(file);
            const int64_t first = range.first + offset;
            const int64_t last = range.second + offset;
            const std::string outPath = exportBase(p.path) + "_tags.nk";

            std::stringstream ss;
            ss << "#! Nuke script written by DJV_APFX\n";
            ss << "version 13.0\n";
            ss << "Root {\n inputs 0\n name " << quoted(forwardSlashes(outPath)) << "\n";
            ss << " first_frame " << first << "\n last_frame " << last << "\n";
            ss << " fps " << p.rate << "\n}\n";

            const int step = 300;
            for (size_t i = 0; i < segments.size(); ++i)
            {
                const Segment& s = segments[i];
                const Tag tag = s.tag >= 0 && s.tag < static_cast<int>(tags.size()) ? tags[s.tag] : Tag();
                ss << "BackdropNode {\n inputs 0\n name " << scriptName(s.name + "_bd") << "\n";
                ss << " tile_color 0x" << hexColor(tag.color) << "ff\n";
                ss << " label " << quoted(s.name + "\n" + tag.name + "\n" +
                    std::to_string(s.start + offset) + "-" + std::to_string(s.end + offset)) << "\n";
                ss << " note_font_size 30\n";
                ss << " xpos " << (static_cast<int>(i) * step - 60) << "\n ypos 120\n";
                ss << " bdwidth 240\n bdheight 260\n}\n";
            }
            ss << "Read {\n inputs 0\n file " << quoted(file) << "\n";
            ss << " first " << first << "\n last " << last << "\n";
            ss << " origfirst " << first << "\n origlast " << last << "\n origset true\n";
            ss << " name MainFootage\n xpos 0\n ypos -200\n}\n";
            ss << "Dot {\n name Dot_main\n xpos 34\n ypos -80\n}\n";
            ss << "set N_main [stack 0]\n";
            for (size_t i = 0; i < segments.size(); ++i)
            {
                const Segment& s = segments[i];
                if (i > 0)
                {
                    ss << "push $N_main\n";
                }
                ss << "FrameRange {\n first_frame " << (s.start + offset) << "\n";
                ss << " last_frame " << (s.end + offset) << "\n";
                ss << " time \"\"\n";
                ss << " name " << scriptName(s.name) << "\n";
                ss << " xpos " << (static_cast<int>(i) * step) << "\n ypos 260\n}\n";
            }

            std::ofstream out(ftk::toFileSystem(outPath));
            if (!out.is_open())
                return "Error: cannot write " + outPath;
            out << ss.str();
            return outPath;
        }

        std::string TagsModel::exportSOLVFX()
        {
            FTK_P();
            if (p.path.isEmpty())
                return "Error: no file is open.";
            const auto& tags = p.tags->get();
            const auto& segments = p.segments->get();
            const auto range = p.range->get();

            const bool seq = p.path.isSeq();
            const int64_t offset = seq ? 0 : 1 - range.first;
            std::string file;
            if (seq)
            {
                const int pad = std::max(1, p.path.getPad());
                file = p.path.getDir() + p.path.getBase() + std::string(pad, '#') + p.path.getExt();
            }
            else
            {
                file = p.path.get();
            }
            file = forwardSlashes(file);
            const std::string base = exportBase(p.path);
            const std::string pyPath = base + "_solvfx.py";
            const std::string solvPath = forwardSlashes(base + "_tags.solv");

            std::stringstream ss;
            ss << "# SOLVFX project from DJV_APFX tags.\n";
            ss << "# Background: SOLVFXRenderer.exe -t \"" << forwardSlashes(pyPath) << "\"\n";
            ss << "# Open in the GUI: SOLVFX.exe -l \"" << forwardSlashes(pyPath) << "\"\n";
            ss << "import NatronEngine\n\n";
            ss << "MOV = " << quoted(file) << "\n";
            ss << "OUT = " << quoted(solvPath) << "\n";
            ss << "FIRST = " << (range.first + offset) << "\n";
            ss << "LAST = " << (range.second + offset) << "\n";
            ss << "FPS = " << p.rate << "\n";
            ss << "ANCHOR_LABEL = \"main footage\"\n";
            ss << "STEP = 300\n\n";
            ss << "SHOTS = [  # (script name, label, first, last, (r, g, b))\n";
            for (const auto& s : segments)
            {
                const Tag tag = s.tag >= 0 && s.tag < static_cast<int>(tags.size()) ? tags[s.tag] : Tag();
                ss << "    (" << quoted(scriptName(s.name)) << ", "
                    << quoted(s.name + " - " + tag.name) << ", "
                    << (s.start + offset) << ", " << (s.end + offset) << ", ("
                    << tag.color.r << ", " << tag.color.g << ", " << tag.color.b << ")),\n";
            }
            ss << "]\n\n";
            ss << R"PY(def backdrop_label(text, size=30):
    # SOLVFX parses the font from this exact tag: keep size, color AND face
    return ('<b><font size="%d" color="#e8e8e8" face="Source Sans 3">%s</font></b>'
            % (size, text))

app.getProjectParam("frameRange").setValue(FIRST, 0)
app.getProjectParam("frameRange").setValue(LAST, 1)
app.getProjectParam("frameRate").setValue(FPS)

read = app.createReader(MOV)
read.setScriptName("MainFootage")
read.setPosition(0, 0)

dot = app.createNode("fr.inria.built-in.Dot")
dot.connectInput(0, read)
dot.setPosition(40, 150)

anchor = app.createNode("com.aguapanelafx.solvfx.Anchor")
anchor.setScriptName("main_footage")
anchor.setLabel(ANCHOR_LABEL)
anchor.connectInput(0, dot)
anchor.setPosition(0, 230)

for i, (name, label, first, last, color) in enumerate(SHOTS):
    x = i * STEP
    stamp = app.createNode("com.aguapanelafx.solvfx.Stamp")
    stamp.setScriptName(name + "_stamp")
    stamp.setLabel(ANCHOR_LABEL)
    stamp.connectInput(0, anchor)
    stamp.setPosition(x, 520)

    fr = app.createNode("net.sf.openfx.FrameRange")
    fr.setScriptName(name + "_range")
    fr.connectInput(0, stamp)
    fr.getParam("frameRange").setValue(first, 0)
    fr.getParam("frameRange").setValue(last, 1)
    fr.setPosition(x, 640)

    bd = app.createNode("fr.inria.built-in.BackDrop")
    bd.setScriptName(name + "_bd")
    bd.getParam("Label").setValue(backdrop_label(label))
    bd.setColor(color[0], color[1], color[2])
    bd.setPosition(x - 50, 440)
    bd.setSize(240, 300)

app.saveProjectAs(OUT)
try:
    if NatronEngine.natron.isBackground():
        quit()
except Exception:
    pass
)PY";

            std::ofstream out(ftk::toFileSystem(pyPath));
            if (!out.is_open())
                return "Error: cannot write " + pyPath;
            out << ss.str();
            return pyPath;
        }
    }
}
