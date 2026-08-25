/*
 * Dusk console widgets for DAF
 * Copyright (C) 2026 Dusk Audio
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with
 * or without fee is hereby granted, provided that the above copyright notice and this
 * permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// Every widget in the Dusk set on one window: what they look like, how they behave, and
// a working example of the two calls the baked knob dome needs around atlas build time.

// ImGui is quite large, build it separately
#define IMGUI_SKIP_IMPLEMENTATION

#include "Application.hpp"
#include "../opengl/DearImGui.cpp"
#include "../opengl/DuskWidgets.hpp"
#include "src/Resources.hpp"

#include <cmath>
#include <cstdio>

START_NAMESPACE_DGL

namespace dw = DuskWidgets;

class DuskWidgetsGallery : public ImGuiStandaloneWindow
{
public:
    DuskWidgetsGallery(Application& app)
        : ImGuiStandaloneWindow(app)
    {
        ImGuiIO& io(ImGui::GetIO());
        io.Fonts->Clear();
        fonts = dw::buildFonts(*io.Fonts, daf_resources::dejavusans_ttf,
                               (int)daf_resources::dejavusans_ttf_size, 1.0f);
        // Reserved before the atlas is packed, rasterised after: the dome is three
        // rectangles inside the font texture, so it costs no draw call of its own.
        knobs.reserve(*io.Fonts, 128);
        io.FontDefault = fonts.band;
        io.Fonts->Build();
        knobs.rasterise(*io.Fonts);
    }

protected:
    void onImGuiDisplay() override
    {
        const float width = getWidth();
        const float height = getHeight();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg,
                              ImGui::ColorConvertU32ToFloat4(theme.background));
        ImGui::Begin("##gallery", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        dw::Context ctx;
        ctx.dl = ImGui::GetWindowDrawList();
        ctx.theme = &theme;
        ctx.fonts = &fonts;
        ctx.knobAtlas = &knobs;
        ctx.drag = &drag;
        ctx.scale = 1.0f;

        char text[32];
        const ImU32 colours[] = { IM_COL32(0xc4, 0x44, 0x44, 0xff),
                                  IM_COL32(0x5f, 0xa5, 0x5f, 0xff),
                                  IM_COL32(0x58, 0x78, 0xb0, 0xff),
                                  IM_COL32(0xd0, 0x90, 0x60, 0xff),
                                  IM_COL32(0xe0, 0xe0, 0xe4, 0xff) };
        const char* const captions[] = { "GAIN", "FREQ", "Q", "DRIVE", "MIX" };

        dw::text(ctx, fonts.title, 13.0f, ImVec2(20, 14), 200, theme.textBright,
                 "Dusk widgets", dw::Align::left);

        for (int i = 0; i < 5; ++i)
        {
            char id[8];
            std::snprintf(id, sizeof id, "##k%d", i);
            std::snprintf(text, sizeof text, "%+.1f", knobValues[i]);

            dw::KnobStyle style;
            style.fill = colours[i];
            style.caption = captions[i];
            style.value = text;

            const auto result = dw::knob(ctx, id, ImVec2(46.0f + i * 66.0f, 74.0f), 16.0f,
                                         knobValues[i], gainRange, 0.0f, style);
            if (result.changed)
                knobValues[i] = result.value;
        }

        const bool engaged = compEngaged;
        const auto pill = dw::modulePill(ctx, "##pill", ImVec2(20, 118), ImVec2(200, 138),
                                         "COMP", engaged, colours[3]);
        if (pill.toggled)
            compEngaged = !engaged;
        if (pill.labelClicked)
            ImGui::OpenPopup("##pillMenu");
        if (ImGui::BeginPopup("##pillMenu"))
        {
            ImGui::MenuItem("Opto");
            ImGui::MenuItem("FET");
            ImGui::EndPopup();
        }

        dw::ButtonStyle button;
        button.onFill = IM_COL32(0xff, 0x45, 0x00, 0xff);
        button.fontSize = 11.0f;
        if (dw::textButton(ctx, "##mute", ImVec2(20, 146), ImVec2(78, 168), "MUTE", muted,
                           button)
                .clicked)
            muted = !muted;

        button.onFill = IM_COL32(0xcc, 0xcc, 0x00, 0xff);
        if (dw::textButton(ctx, "##solo", ImVec2(84, 146), ImVec2(142, 168), "SOLO", soloed,
                           button)
                .clicked)
            soloed = !soloed;

        if (editingName)
        {
            const auto field = dw::textField(ctx, "##name", ImVec2(20, 176), ImVec2(200, 198),
                                             name, sizeof name, takeFocus);
            takeFocus = false;
            if (field.committed || field.cancelled)
                editingName = false;
        }
        else
        {
            if (dw::hitArea(ctx, "##nameLabel", ImVec2(20, 176), ImVec2(200, 198))
                && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                editingName = true;
                takeFocus = true;
            }
            dw::text(ctx, fonts.title, 13.0f, ImVec2(20, 180), 180, theme.textValue, name,
                     dw::Align::left);
        }

        dw::text(ctx, fonts.caption, 8.0f, ImVec2(20, 206), 180, theme.textDim,
                 "double-click the name to rename, the knobs to reset", dw::Align::left);

        // A moving programme signal, so the meter and the gain reduction column show
        // their ballistics rather than a still frame.
        phase += ImGui::GetIO().DeltaTime * 1.7f;
        const float beat = std::fmod(phase, 1.0f);
        const float level = -48.0f + 46.0f * std::exp(-beat * 5.0f);
        meterBallistics.tick(level, 2.0f);

        const float top = 240.0f;
        const float bottom = std::max(top + 80.0f, height - 30.0f);

        dw::FaderStyle faderStyle;
        faderStyle.gutterLeft = 20.0f;
        const auto moved = dw::fader(ctx, "##fader", ImVec2(66, top), ImVec2(106, bottom),
                                     faderDb, faderRange, 0.0f, faderStyle);
        if (moved.changed)
            faderDb = moved.value;

        dw::meter(ctx, ImVec2(112, top + 18.0f), ImVec2(126, bottom - 18.0f),
                  meterBallistics.displayed, meterBallistics.peakHold);

        dw::GainReductionStyle grStyle;
        grStyle.handle = true;
        const auto gr = dw::gainReduction(ctx, "##gr", ImVec2(130, top + 18.0f),
                                          ImVec2(154, bottom - 18.0f),
                                          std::min(0.0f, threshold - meterBallistics.displayed),
                                          threshold, grStyle);
        if (gr.changed)
            threshold = gr.threshold;

        dw::drawDragBubble(ctx);

        std::snprintf(text, sizeof text, "%d widgets   %d verts", ctx.widgets,
                      ctx.dl->VtxBuffer.Size);
        dw::text(ctx, fonts.value, 11.0f, ImVec2(width - 180.0f, 16.0f), 160, theme.textDim,
                 text, dw::Align::right);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

private:
    dw::Theme theme;
    dw::Fonts fonts;
    dw::KnobAtlas knobs;
    dw::DragState drag;
    dw::MeterBallistics meterBallistics;
    dw::Range gainRange { -15.0f, 15.0f, 1.0f };
    dw::Range faderRange = dw::Range::withMidPoint(-90.0f, 6.0f, -12.0f);

    float knobValues[5] = { 2.5f, -1.5f, 0.0f, 6.0f, -3.0f };
    float faderDb = 0.0f;
    float threshold = -14.0f;
    float phase = 0.0f;
    char name[64] = "KICK IN";
    bool compEngaged = true;
    bool muted = false;
    bool soloed = false;
    bool editingName = false;
    bool takeFocus = false;
};

END_NAMESPACE_DGL

int main(int, char**)
{
    USE_NAMESPACE_DGL;

    Application app;
    DuskWidgetsGallery win(app);
    win.setGeometryConstraints(360, 400, false);
    win.setSize(420, 720);
    win.setResizable(true);
    win.setTitle("Dusk widgets");
    win.show();
    app.exec();

    return 0;
}
