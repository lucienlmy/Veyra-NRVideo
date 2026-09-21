#include "veyra/pipeline/EnhanceGraph.h"
#include "veyra/gfx/D3D12DeviceContext.h"
#include "veyra/gfx/CommandSlotRing.h"
#include "CaptureFormatGpuCases.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

using namespace veyra;
using Clock = std::chrono::steady_clock;

static AVFrame* makeFrame(AVPixelFormat format, int width, int height, bool inverted) {
    auto* frame = av_frame_alloc();
    if (!frame) return nullptr;
    frame->format = format; frame->width = width; frame->height = height;
    if (av_frame_get_buffer(frame, 256) < 0) { av_frame_free(&frame); return nullptr; }
    const bool wide = format != AV_PIX_FMT_NV12;
    for (int plane = 0; plane < 2; ++plane) {
        const int rows = plane ? (height + 1) / 2 : height;
        const int samples = plane ? ((width + 1) / 2) * 2 : width;
        if (inverted) {
            frame->data[plane] += ptrdiff_t(rows - 1) * frame->linesize[plane];
            frame->linesize[plane] = -frame->linesize[plane];
        }
        for (int y = 0; y < rows; ++y) {
            auto* row = frame->data[plane] + ptrdiff_t(y) * frame->linesize[plane];
            for (int x = 0; x < samples; ++x) {
                const unsigned value = plane ? 96 + (x * 7 + y * 3) % 64 : 16 + (x * 3 + y * 11) % 220;
                if (wide) {
                    const uint16_t code = uint16_t((value << 8) | (format == AV_PIX_FMT_P010 ? ((x + y) % 4) << 6 : (x + y) % 256));
                    memcpy(row + x * 2, &code, sizeof(code));
                } else row[x] = uint8_t(value);
            }
        }
    }
    return frame;
}

static pipeline::EnhanceGraphDesc graphDesc(AVPixelFormat format, int width, int height) {
    pipeline::EnhanceGraphDesc d;
    d.sourceWidth = d.workWidth = width; d.sourceHeight = d.workHeight = height;
    d.enableNr = d.enableFg = d.enableSr = false; d.noFeatures = true;
    d.captureBitDepth = format == AV_PIX_FMT_P016 ? 16 : format == AV_PIX_FMT_P010 ? 10 : 8;
    return d;
}

static pipeline::ColorDescription colorDesc() {
    pipeline::ColorDescription color;
    color.range = pipeline::ColorRange::Limited;
    color.matrix = pipeline::YuvMatrix::BT709;
    color.primaries = pipeline::ColorPrimaries::BT709;
    color.transfer = pipeline::TransferFunction::BT709;
    return color;
}

int main() {
    gfx::D3D12DeviceContext ctx; gfx::CommandSlotRing ring; Status status;
    if (!ctx.initialize({}, status) || !ring.initialize(ctx.device(), ctx.directQueue(), ctx.fence(), ctx.fenceEvent(), 4, status)) return 2;
    int failures = 0;
    const auto color = colorDesc();
    for (auto format : {AV_PIX_FMT_NV12, AV_PIX_FMT_P010, AV_PIX_FMT_P016}) {
        for (int width : {64, 65, 66}) {
            std::vector<float> reference;
            for (bool inverted : {false, true}) {
                auto* frame = makeFrame(format, width, 35, inverted);
                pipeline::EnhanceGraph graph(ctx, ring);
                pipeline::EnhanceGraph::FrameOutputs out;
                std::vector<float> pixels;
                bool ok = frame && graph.initialize(graphDesc(format, width, 35)) && graph.createViews() &&
                    graph.process(frame, 0, true, out, 1, &color) && captureReadFp16(ctx, ring, graph.diagnosticLinearInput(), pixels);
                if (ok) { if (!inverted) reference = pixels; else ok = reference == pixels; }
                uint64_t hash = 14695981039346656037ull;
                for (size_t i = 0; i < pixels.size() * sizeof(float); ++i) { hash ^= reinterpret_cast<const uint8_t*>(pixels.data())[i]; hash *= 1099511628211ull; }
                std::cout << "UPLOAD_PIXELS format=" << format << " width=" << width << " inverted=" << inverted << " hash=" << hash << " pass=" << ok << std::endl;
                failures += !ok;
                out = {}; ring.drainQueue(); graph.shutdown(); av_frame_free(&frame);
            }
        }
        auto* frame = makeFrame(format, 3840, 2160, false);
        auto desc = graphDesc(format, 3840, 2160);
        Clock::time_point start;
        double uploadMs = 0;
        desc.stageMark = [&](const char* stage) {
            if (strcmp(stage, "upload") == 0) uploadMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        };
        pipeline::EnhanceGraph graph(ctx, ring);
        bool ok = frame && graph.initialize(desc) && graph.createViews();
        std::vector<double> upload, process;
        // Drain outside the timed region to measure CPU upload into the real
        // mapped D3D12 buffers, without conflating previous GPU work or FG.
        for (int i = 0; ok && i < 240; ++i) {
            pipeline::EnhanceGraph::FrameOutputs out;
            start = Clock::now();
            ok = graph.process(frame, i * (1000.0 / 60), i == 0, out, i + 1, &color);
            const double cpuMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (i >= 40) { upload.push_back(uploadMs); process.push_back(cpuMs); }
            out = {}; ok = ring.drainQueue() && ok;
        }
        auto percentile = [](std::vector<double> values, double fraction) {
            if (values.empty()) return -1.0;
            std::sort(values.begin(), values.end()); return values[size_t((values.size() - 1) * fraction)];
        };
        std::cout << "UPLOAD_4K format=" << format << " samples=" << upload.size()
            << " uploadP50=" << percentile(upload, .5) << " uploadP95=" << percentile(upload, .95)
            << " processP50=" << percentile(process, .5) << " processP95=" << percentile(process, .95)
            << " pass=" << ok << std::endl;
        failures += !ok;
        ring.drainQueue(); graph.shutdown(); av_frame_free(&frame);
    }
    return failures ? 1 : 0;
}
