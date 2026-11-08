// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#pragma once

#include <djv/App/Export.h>

#include <ftk/Core/Color.h>
#include <ftk/Core/Observable.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ftk
{
    class Context;
}

namespace djv
{
    namespace app
    {
        class App;

        //! APFX tag: a kind of work, like "Explosion".
        struct DJV_APP_API_TYPE Tag
        {
            std::string  name;
            ftk::Color4F color = ftk::Color4F(.5F, .5F, .5F);

            bool operator == (const Tag&) const = default;
        };

        //! APFX segment: a frame range of the loaded file with a tag.
        //! Frames are the frames DJV shows on the timeline.
        struct DJV_APP_API_TYPE Segment
        {
            std::string name;
            int         tag   = 0;
            int64_t     start = 0;
            int64_t     end   = 0;

            bool operator == (const Segment&) const = default;
        };

        //! APFX: the range being marked for a new segment, with its own
        //! keys ([ and ]), separate from the player's in and out points.
        struct DJV_APP_API_TYPE SegmentMark
        {
            bool    hasStart = false;
            bool    hasEnd   = false;
            int64_t start    = 0;
            int64_t end      = 0;

            bool operator == (const SegmentMark&) const = default;
        };

        //! APFX tags model: the tags and segments of the current file,
        //! saved next to it as "<file>.tags.json", and the exports to
        //! Nuke and SOLVFX.
        //!
        //! Marking has its own start and end ([ and ]), apart from the
        //! player's in and out points, which the tags never change. Adding
        //! a segment takes the mark and clears it. While a segment is
        //! selected, [ and ] edit its start and end.
        class DJV_APP_API_TYPE TagsModel : public std::enable_shared_from_this<TagsModel>
        {
            FTK_NON_COPYABLE(TagsModel);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&);

            TagsModel();

        public:
            DJV_APP_API ~TagsModel();

            DJV_APP_API static std::shared_ptr<TagsModel> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&);

            //! The preset tag colors.
            DJV_APP_API static const std::vector<ftk::Color4F>& getPresetColors();

            DJV_APP_API std::shared_ptr<ftk::IObservable<std::vector<Tag> > > observeTags() const;
            DJV_APP_API std::shared_ptr<ftk::IObservable<std::vector<Segment> > > observeSegments() const;
            DJV_APP_API std::shared_ptr<ftk::IObservable<int> > observeSelected() const;
            DJV_APP_API std::shared_ptr<ftk::IObservable<bool> > observeHasFile() const;

            //! The first and last frame of the file, and the current frame,
            //! for drawing.
            DJV_APP_API std::shared_ptr<ftk::IObservable<std::pair<int64_t, int64_t> > > observeRange() const;
            DJV_APP_API std::shared_ptr<ftk::IObservable<int64_t> > observeCurrentFrame() const;
            DJV_APP_API std::shared_ptr<ftk::IObservable<SegmentMark> > observeMark() const;

            //! Mark the start or end at the current frame; with a segment
            //! selected, set its start or end instead.
            DJV_APP_API void markStart();
            DJV_APP_API void markEnd();
            DJV_APP_API void clearMark();

            DJV_APP_API void addTag();
            DJV_APP_API void setTag(int, const Tag&);
            DJV_APP_API void removeTag(int);

            //! Add a segment from the mark, then clear the mark.
            DJV_APP_API void addSegment(int tag);
            DJV_APP_API void setSegment(int, const Segment&);
            DJV_APP_API void removeSegment(int);

            //! Select a segment, or -1 for none.
            DJV_APP_API void select(int);

            DJV_APP_API bool canUndo() const;
            DJV_APP_API void undo();

            //! Export a Nuke script next to the file; returns its path, or
            //! an error starting with "Error:".
            DJV_APP_API std::string exportNuke();

            //! Export a SOLVFX (NatronEngine) Python script next to the
            //! file; returns its path, or an error starting with "Error:".
            DJV_APP_API std::string exportSOLVFX();

            //! Export a CMX 3600 EDL next to the file, as cuts (one event
            //! per segment) or as markers (the whole file with a locator
            //! per segment). Returns its path, or an error starting with
            //! "Error:".
            DJV_APP_API std::string exportEDL(bool markers);

            //! Export a Final Cut Pro 7 XML (Premiere, Resolve) next to the
            //! file, as cuts or as markers. Returns its path, or an error
            //! starting with "Error:".
            DJV_APP_API std::string exportXML(bool markers);

        private:
            void _playerUpdate();
            void _load();
            void _save();
            void _pushUndo();
            void _sort();

            FTK_PRIVATE();
        };
    }
}
