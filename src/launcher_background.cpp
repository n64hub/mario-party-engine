// A picture behind a game's launcher menu, where RecompFrontend draws a plain dark colour, the same in every game: each
// game passes its own picture, built into the program (file_to_c), so the desktop build and the web page need no file
// beside it.
//
// The picture is an image in the launcher's background container, as high as the window and centred, so a 16:9
// picture covers a 16:9 window and is cut at its sides in a narrower one; in a window wider than the picture it is
// stretched to the window's width (min-width), with no band of the dark colour beside it. RecompFrontend's own hook,
// set_launcher_background_svg, takes an SVG as wide as the window, which leaves bands above and below in a narrower one.
//
// The menu's text (the title, the options, the version) is outlined in black, so it reads on a picture where it was
// made for the dark colour: a font effect, which RmlUi's text inherits, set on the launcher once. RecompFrontend's
// Style has no setter for font effects, so a Style of this file's own sets the property, and the launcher enables it.

#include <string>
#include <string_view>
#include <vector>

#include "RmlUi/Core/StyleSheetSpecification.h"
#include "recompui/recompui.h"
#include "elements/ui_image.h"
#include "launcher_background.h"

namespace {
    // A name starting with '?' reaches RecompFrontend's queued images as it is (RmlUi's RenderManager::LoadTexture);
    // any other is first joined to the document's path, found in no queue, and drawn as a transparent pixel.
    const std::string texture_name = "?/mp/launcher_background";
    constexpr std::string_view outline_style_name = "mp_launcher_text_outline";

    struct TextOutline : recompui::Style {
        TextOutline() {
            Rml::PropertyDictionary properties;
            Rml::StyleSheetSpecification::ParsePropertyDeclaration(properties, "font-effect", "outline(2dp #000000c0)");
            for (const auto& [id, property] : properties.GetProperties()) {
                set_property(id, property);
            }
        }
    };
}

void mp_launcher_background::set(recompui::LauncherMenu* menu, std::span<const char> png) {
    using namespace recompui;
    static TextOutline outline;

    queue_image_from_bytes_file(texture_name, std::vector<char>(png.begin(), png.end()));

    ContextId context = get_current_context();
    Element* container = menu->get_background_wrapper();
    container->set_overflow(Overflow::Hidden);
    Image* image = context.create_element<Image>(container, texture_name);
    image->set_position(Position::Absolute);
    image->set_top(50.0f, Unit::Percent);
    image->set_left(50.0f, Unit::Percent);
    image->set_height(100.0f, Unit::Percent);
    image->set_width_auto();
    image->set_min_width(100.0f, Unit::Percent);
    image->set_translate_2D(-50.0f, -50.0f, Unit::Percent);

    menu->add_style(&outline, outline_style_name);
    menu->set_style_enabled(outline_style_name, true);
}
