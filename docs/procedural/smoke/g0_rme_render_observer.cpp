// Test-only observer of real House GUI transitions and its presented GL buffer.
#include "app/main.h"
#include "ui/gui.h"
#include "ui/main_frame.h"
#include "editor/editor.h"
#include "map/map.h"
#include "util/nanovg_canvas.h"
#include "util/nanovg_table_ctrl.h"
#include <glad/glad.h>
#include <wx/eventfilter.h>
#include <wx/module.h>
#include <wx/timer.h>
#include <array>
#include <filesystem>
#include <fstream>

namespace {
namespace fs = std::filesystem;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

struct PreviousContext {
    HDC dc = wglGetCurrentDC();
    HGLRC context = wglGetCurrentContext();
    ~PreviousContext() { wglMakeCurrent(dc, context); }
};
struct ReadState {
    GLint framebuffer{}, buffer{}, pbo{}, alignment{}, rowLength{}, skipRows{}, skipPixels{};
    explicit ReadState(GLenum target = GL_FRONT) {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
        glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glGetIntegerv(GL_READ_BUFFER, &buffer);
        glReadBuffer(target);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    }
    ~ReadState() {
        glReadBuffer(buffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
        glPixelStorei(GL_PACK_ALIGNMENT, alignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
    }
};

class Observer final : public wxEventFilter {
public:
    Observer() : root(fs::current_path()), report(root / "render-observer.log") {
        report << std::unitbuf;
        check(root.filename() == "run" && fs::is_regular_file(root / ".g0-render-observe"), "Private render guard required");
        wxEvtHandler::AddFilter(this);
    }
    ~Observer() override { wxEvtHandler::RemoveFilter(this); }
    int FilterEvent(wxEvent& event) override {
        if (event.GetEventType() == wxEVT_CLOSE_WINDOW && event.GetEventObject() == g_gui.root) closing = true;
        if (event.GetEventType() != wxEVT_PAINT) return Event_Skip;
        const auto* canvas = dynamic_cast<wxGLCanvas*>(event.GetEventObject());
        if (!canvas) return Event_Skip;
        // Fixed storage: the observer does not allocate in a paint callback.
        for (auto& counter : paints) {
            if (counter.id == canvas->GetId() || counter.id == wxID_NONE) {
                counter.id = canvas->GetId();
                ++counter.count;
                break;
            }
        }
        return Event_Skip;
    }
    void sample() {
        if (closing || failed || stage == expected.size() || !g_gui.root || !wxApp::IsMainLoopRunning() || !g_gui.root->IsEnabled()) return;
        auto* editor = g_gui.GetCurrentEditor();
        auto* canvas = dynamic_cast<NanoVGCanvas*>(wxWindow::FindWindowById(7048, g_gui.root));
        if (!editor || !canvas || !canvas->IsShownOnScreen()) return;
        size_t paintCount = 0;
        for (const auto& counter : paints) if (counter.id == canvas->GetId()) paintCount = counter.count;
        if (++polls % 4 == 0) report << "RENDER_HEARTBEAT\t" << stage << '\t' << paintCount << '\t'
            << (canvas->GetNVGContext() != nullptr) << '\t' << g_gui.IsRenderingEnabled() << '\t' << g_gui.root->IsActive() << '\n';
        if (!canvas->GetNVGContext() || !g_gui.root->IsActive() || editor->map.houses.count() != expected[stage]) return;
        try {
            const auto imagePath = root / ("house-front-" + std::to_string(stage) + ".png");
            PreviousContext restoreContext;
            ScopedGLContext context(canvas);
            check(wglGetCurrentDC() == canvas->GetHDC(), "Current drawable differs from the House canvas; refusing foreign readback");
            const auto previousError = glGetError();
            auto* table = dynamic_cast<NanoVGTableCtrl*>(canvas);
            check(table && table->GetRowCount() == expected[stage], "House table rows disagree with map houses");
            if (expected[stage] == 1) {
                const auto* row = table->GetRow(0);
                check(row && row->id == 1 && row->cells.size() == 5 && row->cells[1] == "1" &&
                    row->cells[2] == "Unnamed House #1" && row->cells[3] == "0" && row->cells[4] == "0", "Unexpected displayed House row data");
            }
            GLint drawFramebuffer{}, readFramebuffer{}, drawBuffer{};
            GLboolean doubleBuffer{}, mask[4]{};
            GLfloat clear[4]{};
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
            glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
            glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffer);
            glGetBooleanv(GL_COLOR_WRITEMASK, mask);
            glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
            PIXELFORMATDESCRIPTOR format{};
            check(DescribePixelFormat(canvas->GetHDC(), GetPixelFormat(canvas->GetHDC()), sizeof(format), &format) != 0,
                "Could not inspect own House pixel format");
            report << "RENDER_GL_STATE\t" << stage << '\t' << drawFramebuffer << '\t' << readFramebuffer << '\t'
                << drawBuffer << '\t' << static_cast<int>(doubleBuffer) << '\t' << format.dwFlags << '\t'
                << static_cast<int>(mask[0]) << static_cast<int>(mask[1]) << static_cast<int>(mask[2]) << static_cast<int>(mask[3]) << '\t'
                << clear[0] << ',' << clear[1] << ',' << clear[2] << ',' << clear[3] << '\t'
                << static_cast<int>(glIsEnabled(GL_SCISSOR_TEST)) << '\t'
                << static_cast<int>(glIsEnabled(GL_RASTERIZER_DISCARD)) << '\t' << table->GetRowCount() << '\n';
            if (stage == 0) report << "RENDER_DRIVER\t" << glGetString(GL_VENDOR) << '\t' << glGetString(GL_RENDERER) << '\t' << glGetString(GL_VERSION) << '\n';
            GLint viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, viewport);
            const int width = viewport[2], height = viewport[3];
            check(width > 0 && height > 0 && width <= 4096 && height <= 4096, "Unexpected House viewport");
            auto saveBuffer = [&](GLenum buffer, const fs::path& destinationPath) {
                std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4);
                {
                    ReadState restoreReadState(buffer);
                    check(glGetError() == GL_NO_ERROR, "Could not configure buffer readback");
                    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                    check(glGetError() == GL_NO_ERROR, "Buffer readback failed");
                }
                wxImage image(width, height);
                auto* pixels = image.GetData();
                for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                    const auto source = (static_cast<size_t>(height - 1 - y) * width + x) * 4;
                    const auto destination = (static_cast<size_t>(y) * width + x) * 3;
                    for (size_t component = 0; component < 3; ++component) pixels[destination + component] = rgba[source + component];
                }
                check(image.SaveFile(wxString(destinationPath.wstring()), wxBITMAP_TYPE_PNG), "Could not save private readback PNG");
            };
            saveBuffer(GL_FRONT, imagePath);
            // Back-buffer contents after SwapBuffers are diagnostic only and
            // are never substituted for the presented-image visual oracle.
            if (doubleBuffer) saveBuffer(GL_BACK, root / ("house-back-" + std::to_string(stage) + ".png"));
            if (stage == 1 && fs::is_regular_file(root / ".g0-diagnostic-extra-swap")) {
                // Deliberately separate presentation experiment, never normal-GUI proof.
                // It neither changes map data nor replaces the pre-experiment images.
                for (wxWindow* window = canvas; window; window = window->GetParent()) {
                    const auto handle = static_cast<HWND>(window->GetHandle());
                    report << "RENDER_WINDOW_STYLE\t" << window->GetId() << '\t'
                        << GetWindowLongPtrW(handle, GWL_STYLE) << '\t'
                        << GetWindowLongPtrW(handle, GWL_EXSTYLE) << '\n';
                }
                glFinish();
                SetLastError(ERROR_SUCCESS);
                const auto swapped = ::SwapBuffers(canvas->GetHDC());
                const auto swapError = GetLastError();
                glFinish();
                report << "RENDER_EXTRA_SWAP\t" << swapped << '\t' << swapError << '\t' << glGetError() << '\n';
                saveBuffer(GL_FRONT, root / "house-front-extra-swap.png");
                saveBuffer(GL_BACK, root / "house-back-extra-swap.png");
            }
            report << "RENDER_CAPTURE\t" << stage << '\t' << expected[stage] << '\t' << paintCount << '\t'
                << g_gui.IsRenderingEnabled() << '\t' << previousError << '\t' << width << '\t' << height << '\t' << imagePath.string() << '\n';
            ++stage;
            if (stage == expected.size()) report << "RENDER_SEQUENCE_PASS\n";
        } catch (const std::exception& error) {
            failed = true;
            report << "FAILED\t" << error.what() << '\n';
        }
    }
private:
    struct Counter { wxWindowID id = wxID_NONE; size_t count = 0; };
    const std::array<size_t, 5> expected{0, 1, 0, 1, 0};
    std::array<Counter, 32> paints{};
    fs::path root;
    std::ofstream report;
    size_t stage = 0, polls = 0;
    bool closing = false, failed = false;
};
std::unique_ptr<Observer> observer;
class ObserverTimer final : public wxTimer {
    void Notify() override { if (observer) observer->sample(); }
};
}
class G0RenderObserverModule final : public wxModule {
public:
    bool OnInit() override {
        observer = std::make_unique<Observer>();
        timer = std::make_unique<ObserverTimer>();
        timer->Start(500);
        return true;
    }
    void OnExit() override {
        timer->Stop();
        timer.reset();
        observer.reset();
    }
private:
    std::unique_ptr<ObserverTimer> timer;
    wxDECLARE_DYNAMIC_CLASS(G0RenderObserverModule);
};
wxIMPLEMENT_DYNAMIC_CLASS(G0RenderObserverModule, wxModule);
