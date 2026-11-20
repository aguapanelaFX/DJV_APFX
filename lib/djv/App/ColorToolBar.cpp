// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the DJV project.

#include <djv/App/ColorToolBar.h>

#include <djv/App/App.h>
#include <djv/Models/ColorModel.h>
#include <djv/Models/OCIOModel.h>
#include <djv/Models/ViewportModel.h>

#include <ftk/UI/Action.h>
#include <ftk/UI/CheckBox.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/PushButton.h>
#include <ftk/UI/ComboBox.h>
#include <ftk/UI/FloatEditSlider.h>
#include <ftk/UI/Label.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ToolButton.h>

#include <cmath>

namespace djv
{
    namespace app
    {
        struct ColorToolBar::Private
        {
            std::shared_ptr<models::OCIOModel> ocioModel;

            std::shared_ptr<ftk::CheckBox> ocioCheckBox;
            std::shared_ptr<ftk::ComboBox> displayComboBox;
            std::shared_ptr<ftk::ComboBox> viewComboBox;
            std::shared_ptr<ftk::FloatEditSlider> exposureSlider;
            std::shared_ptr<ftk::FloatEditSlider> gammaSlider;
            std::shared_ptr<ftk::FloatEditSlider> saturationSlider;
            std::shared_ptr<ftk::ToolButton> resetButton;
            std::shared_ptr<ftk::PushButton> guidesButton;
            std::shared_ptr<ftk::Menu> guidesMenu;
            std::weak_ptr<App> app;
            std::shared_ptr<ftk::HorizontalLayout> layout;

            std::shared_ptr<ftk::Observer<tl::OCIOOptions> > optionsObserver;
            std::shared_ptr<ftk::Observer<tl::OCIOOptions> > optionsObserver2;
            std::shared_ptr<ftk::Observer<models::OCIOModelData> > dataObserver;
            std::shared_ptr<ftk::Observer<tl::DisplayOptions> > displayOptionsObserver;
        };

        void ColorToolBar::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            ftk::IContainer::_init(context, "djv::app::ColorToolBar", parent);
            FTK_P();

            auto colorModel = app->getColorModel();
            auto viewportModel = app->getViewportModel();
            std::weak_ptr<models::ViewportModel> viewportModelWeak(viewportModel);
            p.ocioModel = models::OCIOModel::create(context);

            p.ocioCheckBox = ftk::CheckBox::create(context, "ACES");
            p.ocioCheckBox->setTooltip("Color manage the viewer with OpenColorIO (ACES).");

            p.displayComboBox = ftk::ComboBox::create(context);
            p.displayComboBox->setTooltip("Viewer display.");

            p.viewComboBox = ftk::ComboBox::create(context);
            p.viewComboBox->setTooltip("Viewer transform, like the Nuke viewer process.");

            p.exposureSlider = ftk::FloatEditSlider::create(context);
            p.exposureSlider->setRange(-10.F, 10.F);
            p.exposureSlider->setDefault(0.F);
            p.exposureSlider->getModel()->setRangeSoft(true);
            p.exposureSlider->setTooltip("Viewer exposure in stops (f-stops).");

            p.gammaSlider = ftk::FloatEditSlider::create(context);
            p.gammaSlider->setRange(.1F, 4.F);
            p.gammaSlider->setDefault(1.F);
            p.gammaSlider->getModel()->setRangeSoft(true);
            p.gammaSlider->setTooltip("Viewer gamma.");

            p.saturationSlider = ftk::FloatEditSlider::create(context);
            p.saturationSlider->setRange(0.F, 4.F);
            p.saturationSlider->setDefault(1.F);
            p.saturationSlider->getModel()->setRangeSoft(true);
            p.saturationSlider->setTooltip("Viewer saturation.");

            p.resetButton = ftk::ToolButton::create(context);
            p.resetButton->setIcon("Reset");
            p.resetButton->setTooltip("Reset exposure, gamma and saturation.");

            p.layout = ftk::HorizontalLayout::create(context);
            p.layout->setMarginRole(ftk::SizeRole::MarginInside);
            p.layout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            _setWidget(p.layout);
            p.ocioCheckBox->setParent(p.layout);
            ftk::Label::create(context, "Display:", p.layout);
            p.displayComboBox->setParent(p.layout);
            ftk::Label::create(context, "View:", p.layout);
            p.viewComboBox->setParent(p.layout);
            {
                // Click the name to reset, like a Nuke knob label.
                auto label = ftk::ToolButton::create(context, "Exposure:", p.layout);
                label->setTooltip("Click to reset.");
                label->setClickedCallback(
                    [viewportModelWeak]
                    {
                        if (auto viewportModel = viewportModelWeak.lock())
                        {
                            auto options = viewportModel->getDisplayOptions();
                            options.exposure.exposure = 0.F;
                            viewportModel->setDisplayOptions(options);
                        }
                    });
            }
            p.exposureSlider->setParent(p.layout);
            p.exposureSlider->setHStretch(ftk::Stretch::Expanding);
            {
                // Click the name to reset, like a Nuke knob label.
                auto label = ftk::ToolButton::create(context, "Gamma:", p.layout);
                label->setTooltip("Click to reset.");
                label->setClickedCallback(
                    [viewportModelWeak]
                    {
                        if (auto viewportModel = viewportModelWeak.lock())
                        {
                            auto options = viewportModel->getDisplayOptions();
                            options.levels.gamma = 1.F;
                            viewportModel->setDisplayOptions(options);
                        }
                    });
            }
            p.gammaSlider->setParent(p.layout);
            p.gammaSlider->setHStretch(ftk::Stretch::Expanding);
            {
                // Click the name to reset, like a Nuke knob label.
                auto label = ftk::ToolButton::create(context, "Saturation:", p.layout);
                label->setTooltip("Click to reset.");
                label->setClickedCallback(
                    [viewportModelWeak]
                    {
                        if (auto viewportModel = viewportModelWeak.lock())
                        {
                            auto options = viewportModel->getDisplayOptions();
                            options.color.saturation = ftk::V3F(1.F, 1.F, 1.F);
                            viewportModel->setDisplayOptions(options);
                        }
                    });
            }
            p.saturationSlider->setParent(p.layout);
            p.saturationSlider->setHStretch(ftk::Stretch::Expanding);
            p.resetButton->setParent(p.layout);

            // APFX: guides, a menu of masks and safe areas.
            p.app = app;
            p.guidesButton = ftk::PushButton::create(context, "Guides", p.layout);
            p.guidesButton->setTooltip("Aspect ratio masks and safe areas.");
            p.guidesButton->setClickedCallback(
                [this]
                {
                    _guidesMenu();
                });

            // OCIO display and view, kept in step with the Color tool
            // through the color model.
            p.optionsObserver = ftk::Observer<tl::OCIOOptions>::create(
                colorModel->observeOCIOOptions(),
                [this](const tl::OCIOOptions& value)
                {
                    _p->ocioModel->setOptions(value);
                });
            std::weak_ptr<models::ColorModel> colorModelWeak(colorModel);
            p.optionsObserver2 = ftk::Observer<tl::OCIOOptions>::create(
                p.ocioModel->observeOptions(),
                [colorModelWeak](const tl::OCIOOptions& value)
                {
                    if (auto colorModel = colorModelWeak.lock())
                    {
                        colorModel->setOCIOOptions(value);
                    }
                });
            p.dataObserver = ftk::Observer<models::OCIOModelData>::create(
                p.ocioModel->observeData(),
                [this](const models::OCIOModelData& value)
                {
                    FTK_P();
                    p.ocioCheckBox->setChecked(value.enabled);
                    p.displayComboBox->setItems(value.displays);
                    p.displayComboBox->setCurrentIndex(value.displayIndex);
                    p.viewComboBox->setItems(value.views);
                    p.viewComboBox->setCurrentIndex(value.viewIndex);
                });
            p.ocioCheckBox->setCheckedCallback(
                [this](bool value)
                {
                    _p->ocioModel->setEnabled(value);
                });
            p.displayComboBox->setIndexCallback(
                [this](int index)
                {
                    _p->ocioModel->setDisplayIndex(index);
                });
            p.viewComboBox->setIndexCallback(
                [this](int index)
                {
                    _p->ocioModel->setViewIndex(index);
                });

            // Exposure, gamma and saturation: the same display options as
            // the Color tool's sliders.
            p.displayOptionsObserver = ftk::Observer<tl::DisplayOptions>::create(
                viewportModel->observeDisplayOptions(),
                [this](const tl::DisplayOptions& value)
                {
                    FTK_P();
                    p.exposureSlider->setValue(value.exposure.exposure);
                    p.gammaSlider->setValue(value.levels.gamma);
                    p.saturationSlider->setValue(value.color.saturation.x);
                });
            p.exposureSlider->setCallback(
                [viewportModelWeak](float value)
                {
                    if (auto viewportModel = viewportModelWeak.lock())
                    {
                        auto options = viewportModel->getDisplayOptions();
                        if (value == options.exposure.exposure)
                            return;
                        options.exposure.enabled = true;
                        options.exposure.exposure = value;
                        viewportModel->setDisplayOptions(options);
                    }
                });
            p.gammaSlider->setCallback(
                [viewportModelWeak](float value)
                {
                    if (auto viewportModel = viewportModelWeak.lock())
                    {
                        auto options = viewportModel->getDisplayOptions();
                        if (value == options.levels.gamma)
                            return;
                        options.levels.enabled = true;
                        options.levels.gamma = value;
                        viewportModel->setDisplayOptions(options);
                    }
                });
            p.saturationSlider->setCallback(
                [viewportModelWeak](float value)
                {
                    if (auto viewportModel = viewportModelWeak.lock())
                    {
                        auto options = viewportModel->getDisplayOptions();
                        if (value == options.color.saturation.x)
                            return;
                        options.color.enabled = true;
                        options.color.saturation.x = value;
                        options.color.saturation.y = value;
                        options.color.saturation.z = value;
                        viewportModel->setDisplayOptions(options);
                    }
                });
            p.resetButton->setClickedCallback(
                [viewportModelWeak]
                {
                    if (auto viewportModel = viewportModelWeak.lock())
                    {
                        auto options = viewportModel->getDisplayOptions();
                        options.exposure.exposure = 0.F;
                        options.levels.gamma = 1.F;
                        options.color.saturation.x = 1.F;
                        options.color.saturation.y = 1.F;
                        options.color.saturation.z = 1.F;
                        viewportModel->setDisplayOptions(options);
                    }
                });
        }

        void ColorToolBar::_guidesMenu()
        {
            FTK_P();
            auto context = getContext();
            auto app = p.app.lock();
            if (!context || !app)
                return;
            if (p.guidesMenu)
            {
                p.guidesMenu->close();
                return;
            }
            const ui::Guides guides = app->observeGuides()->get();
            std::weak_ptr<App> appWeak(app);
            p.guidesMenu = ftk::Menu::create(context);

            const std::vector<std::pair<std::string, float> > masks =
            {
                { "No mask", 0.F },
                { "2.39 : 1", 2.39F },
                { "2.35 : 1", 2.35F },
                { "2.00 : 1", 2.F },
                { "1.85 : 1", 1.85F },
                { "16 : 9", 16.F / 9.F },
                { "4 : 3", 4.F / 3.F },
                { "1 : 1", 1.F },
                { "9 : 16", 9.F / 16.F },
                { "4 : 5", .8F }
            };
            for (const auto& mask : masks)
            {
                const float value = mask.second;
                auto action = ftk::Action::create(
                    mask.first,
                    [appWeak, value](bool)
                    {
                        if (auto app = appWeak.lock())
                        {
                            auto g = app->observeGuides()->get();
                            g.maskAspect = value;
                            app->setGuides(g);
                        }
                    });
                p.guidesMenu->addAction(action);
                p.guidesMenu->setChecked(action, std::fabs(guides.maskAspect - value) < .001F);
            }
            p.guidesMenu->addDivider();
            const std::vector<std::pair<std::string, float> > opacities =
            {
                { "Mask 50%", .5F },
                { "Mask 75%", .75F },
                { "Mask 100%", 1.F }
            };
            for (const auto& opacity : opacities)
            {
                const float value = opacity.second;
                auto action = ftk::Action::create(
                    opacity.first,
                    [appWeak, value](bool)
                    {
                        if (auto app = appWeak.lock())
                        {
                            auto g = app->observeGuides()->get();
                            g.maskOpacity = value;
                            app->setGuides(g);
                        }
                    });
                p.guidesMenu->addAction(action);
                p.guidesMenu->setChecked(action, std::fabs(guides.maskOpacity - value) < .001F);
            }
            auto addToggle = [this, appWeak](const std::string& text, bool checked, bool ui::Guides::*member)
            {
                auto action = ftk::Action::create(
                    text,
                    [appWeak, member](bool value)
                    {
                        if (auto app = appWeak.lock())
                        {
                            auto g = app->observeGuides()->get();
                            g.*member = value;
                            app->setGuides(g);
                        }
                    });
                _p->guidesMenu->addAction(action);
                _p->guidesMenu->setChecked(action, checked);
            };
            addToggle("Mask line", guides.maskLine, &ui::Guides::maskLine);
            p.guidesMenu->addDivider();
            addToggle("Action safe (90%)", guides.actionSafe, &ui::Guides::actionSafe);
            addToggle("Title safe (80%)", guides.titleSafe, &ui::Guides::titleSafe);

            p.guidesMenu->setCloseCallback(
                [this]
                {
                    _p->guidesMenu.reset();
                });
            p.guidesMenu->open(getWindow(), p.guidesButton->getGeometry());
        }

        ColorToolBar::ColorToolBar() :
            _p(new Private)
        {}

        ColorToolBar::~ColorToolBar()
        {}

        std::shared_ptr<ColorToolBar> ColorToolBar::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<ColorToolBar>(new ColorToolBar);
            out->_init(context, app, parent);
            return out;
        }
    }
}
