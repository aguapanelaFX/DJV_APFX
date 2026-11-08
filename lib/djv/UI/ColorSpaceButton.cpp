// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/UI/ColorSpaceButton.h>

#include <ftk/UI/Action.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/PushButton.h>
#include <ftk/Core/String.h>

#include <map>

namespace djv
{
    namespace ui
    {
        struct ColorSpaceButton::Private
        {
            std::vector<std::string> items;
            std::vector<std::string> families;
            int currentIndex = -1;
            std::function<void(int)> callback;
            std::shared_ptr<ftk::PushButton> button;
            std::shared_ptr<ftk::Menu> menu;
        };

        void ColorSpaceButton::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            ftk::IContainer::_init(context, "djv::ui::ColorSpaceButton", parent);
            FTK_P();
            setHStretch(ftk::Stretch::Expanding);
            p.button = ftk::PushButton::create(context);
            p.button->setHStretch(ftk::Stretch::Expanding);
            p.button->setIcon("MenuArrow");
            _setWidget(p.button);
            p.button->setClickedCallback(
                [this]
                {
                    _open();
                });
        }

        ColorSpaceButton::ColorSpaceButton() :
            _p(new Private)
        {}

        ColorSpaceButton::~ColorSpaceButton()
        {}

        std::shared_ptr<ColorSpaceButton> ColorSpaceButton::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<ColorSpaceButton>(new ColorSpaceButton);
            out->_init(context, parent);
            return out;
        }

        void ColorSpaceButton::setItems(
            const std::vector<std::string>& items,
            const std::vector<std::string>& families)
        {
            FTK_P();
            p.items = items;
            p.families = families;
            p.families.resize(p.items.size());
            if (p.currentIndex >= static_cast<int>(p.items.size()))
            {
                p.currentIndex = -1;
            }
            _textUpdate();
        }

        int ColorSpaceButton::getCurrentIndex() const
        {
            return _p->currentIndex;
        }

        void ColorSpaceButton::setCurrentIndex(int value)
        {
            FTK_P();
            p.currentIndex = value >= 0 && value < static_cast<int>(p.items.size()) ? value : -1;
            _textUpdate();
        }

        void ColorSpaceButton::setIndexCallback(const std::function<void(int)>& value)
        {
            _p->callback = value;
        }

        void ColorSpaceButton::_textUpdate()
        {
            FTK_P();
            const std::string text = p.currentIndex >= 0 ? p.items[p.currentIndex] : std::string();
            p.button->setText(text);
            p.button->setTooltip(text);
        }

        void ColorSpaceButton::_open()
        {
            FTK_P();
            auto context = getContext();
            if (!context || p.items.empty())
                return;
            if (p.menu)
            {
                p.menu->close();
                return;
            }
            p.menu = ftk::Menu::create(context);

            // One sub menu per family level, in the order the
            // configuration lists them.
            std::map<std::string, std::shared_ptr<ftk::Menu> > subMenus;
            std::weak_ptr<ColorSpaceButton> weak(
                std::dynamic_pointer_cast<ColorSpaceButton>(shared_from_this()));
            for (size_t i = 0; i < p.items.size(); ++i)
            {
                std::shared_ptr<ftk::Menu> menu = p.menu;
                std::string key;
                for (const auto& part : ftk::split(p.families[i], '/'))
                {
                    if (part.empty())
                        continue;
                    key += "/" + part;
                    auto j = subMenus.find(key);
                    if (j == subMenus.end())
                    {
                        j = subMenus.insert(std::make_pair(key, menu->addSubMenu(part))).first;
                    }
                    menu = j->second;
                }
                const int index = static_cast<int>(i);
                auto action = ftk::Action::create(
                    p.items[i],
                    [weak, index](bool)
                    {
                        if (auto widget = weak.lock())
                        {
                            widget->_p->currentIndex = index;
                            widget->_textUpdate();
                            if (widget->_p->callback)
                            {
                                widget->_p->callback(index);
                            }
                        }
                    });
                menu->addAction(action);
                if (index == p.currentIndex)
                {
                    menu->setChecked(action, true);
                }
            }

            p.menu->setCloseCallback(
                [weak]
                {
                    if (auto widget = weak.lock())
                    {
                        widget->_p->menu.reset();
                    }
                });
            p.menu->open(getWindow(), getGeometry());
        }
    }
}
