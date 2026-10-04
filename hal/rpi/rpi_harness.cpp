#include "rpi_harness.h"
#include <algorithm>

namespace toad {

RaspberryPiHAL::RaspberryPiHAL(RpiDisplayMode displayMode)
    : displayMode_(displayMode)
{
    spiBuffer240x240_.assign(DisplayEngine::SCREEN_WIDTH * DisplayEngine::SCREEN_HEIGHT, 0x0821);
    if (displayMode_ != RPI_DISP_SPI_ST7789) {
        hdmiBuffer1080p_.assign(1920 * 1080, 0xFF050808);
    }
}

RaspberryPiHAL::~RaspberryPiHAL() {
    shutdown();
}

bool RaspberryPiHAL::initialize(uint32_t sampleRate, size_t bufferSize) {
    sampleRate_ = sampleRate;
    bufferSize_ = bufferSize;
    configureArmFlushToZero();
    initialized_ = true;
    return true;
}

void RaspberryPiHAL::shutdown() {
    initialized_ = false;
    alsaMidiSink_.clear();
}

void RaspberryPiHAL::configureArmFlushToZero() {
#if defined(__arm__) || defined(__aarch64__)
    #if defined(__aarch64__)
        uint64_t fpcr;
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
        fpcr |= (1 << 24); // FZ bit: Flush-to-zero mode enabled
        __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
    #elif defined(__arm__) && !defined(__SOFTFP__)
        uint32_t fpscr;
        __asm__ __volatile__("vmrs %0, fpscr" : "=r"(fpscr));
        fpscr |= (1 << 24); // FZ bit
        __asm__ __volatile__("vmsr fpscr, %0" : : "r"(fpscr));
    #endif
#endif
}

void RaspberryPiHAL::injectRawInput(const RpiRawInput& raw) {
    lastRawState_ = raw;
}

void RaspberryPiHAL::setDisplayMode(RpiDisplayMode mode) {
    displayMode_ = mode;
    if (displayMode_ != RPI_DISP_SPI_ST7789 && hdmiBuffer1080p_.empty()) {
        hdmiBuffer1080p_.assign(1920 * 1080, 0xFF050808);
    }
}

void RaspberryPiHAL::pollInput(std::function<void(const InputEvent&)> callback) {
    if (!callback) return;

    auto emit = [&](LogicalInput input, bool pressed) {
        InputEvent ev;
        ev.input = input;
        ev.pressed = pressed;
        ev.modifier_view = lastRawState_.trig_l2;
        ev.modifier_sub = lastRawState_.trig_r2;
        callback(ev);
    };

    // Gamepad Buttons
    if (lastRawState_.btn_l1) emit(INPUT_PAGE_PREV, true);
    if (lastRawState_.btn_r1) emit(INPUT_PAGE_NEXT, true);

    if (lastRawState_.dpad_up || lastRawState_.gpio_up) emit(INPUT_UP, true);
    if (lastRawState_.dpad_down || lastRawState_.gpio_down) emit(INPUT_DOWN, true);
    if (lastRawState_.dpad_left || lastRawState_.gpio_left) emit(INPUT_LEFT, true);
    if (lastRawState_.dpad_right || lastRawState_.gpio_right) emit(INPUT_RIGHT, true);

    if (lastRawState_.btn_a || lastRawState_.gpio_a) emit(INPUT_BTN_A, true);
    if (lastRawState_.btn_b || lastRawState_.gpio_b) emit(INPUT_BTN_B, true);
    if (lastRawState_.btn_x || lastRawState_.gpio_opt) emit(INPUT_BTN_OPT, true);
    if (lastRawState_.btn_y || lastRawState_.gpio_edit) emit(INPUT_BTN_EDIT, true);

    if (lastRawState_.btn_start) emit(INPUT_TRANSPORT, true);
    if (lastRawState_.btn_select) emit(INPUT_RECORD, true);
}

void RaspberryPiHAL::renderFrame(const uint16_t* rgb565, size_t width, size_t height) {
    if (!rgb565 || width != DisplayEngine::SCREEN_WIDTH || height != DisplayEngine::SCREEN_HEIGHT) return;

    if (displayMode_ == RPI_DISP_SPI_ST7789) {
        // Direct SPI ST7789 240x240 DMA frame
        std::copy(rgb565, rgb565 + (width * height), spiBuffer240x240_.begin());
    } else {
        // Scaled HDMI Render (3x: 720x720, 4x: 960x960) centered on 1080p (1920x1080)
        size_t scale = (displayMode_ == RPI_DISP_HDMI_4X) ? 4 : 3;
        size_t scaledDim = DisplayEngine::SCREEN_WIDTH * scale;
        size_t offsetX = (1920 - scaledDim) / 2;
        size_t offsetY = (1080 - scaledDim) / 2;

        for (size_t y = 0; y < height; ++y) {
            size_t srcRow = y * width;
            size_t dstYBase = offsetY + y * scale;

            for (size_t sy = 0; sy < scale; ++sy) {
                size_t dstRow = (dstYBase + sy) * 1920 + offsetX;
                for (size_t x = 0; x < width; ++x) {
                    uint16_t c565 = rgb565[srcRow + x];
                    uint8_t r = static_cast<uint8_t>(((c565 >> 11) & 0x1F) * 255 / 31);
                    uint8_t g = static_cast<uint8_t>(((c565 >> 5) & 0x3F) * 255 / 63);
                    uint8_t b = static_cast<uint8_t>((c565 & 0x1F) * 255 / 31);
                    uint32_t argb = 0xFF000000 | (r << 16) | (g << 8) | b;

                    for (size_t sx = 0; sx < scale; ++sx) {
                        hdmiBuffer1080p_[dstRow + x * scale + sx] = argb;
                    }
                }
            }
        }
    }
}

IMidiOutputSink* RaspberryPiHAL::getMidiSink() {
    return &alsaMidiSink_;
}

} // namespace toad
