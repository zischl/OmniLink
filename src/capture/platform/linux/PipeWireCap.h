#pragma once
#include "CaptureTypes.h"
#include <cstdint>
#include <functional>

// Stub implementation — will integrate PipeWire mainloop and Vulkan encoder later.
class PipeWireCap
{
  public:
    static constexpr FrameAquisition FrameAqMode = FrameAquisition::EventDriven;
    static constexpr CaptureAPI Type = CaptureAPI::PipeWire;

    // Frame type will become a Vulkan image handle when implemented.
    using FrameCallback = std::function<void(void*)>;

    PipeWireCap();
    ~PipeWireCap();

    // Stubs — implementations will wire up pw_stream callbacks and Vulkan encode.
    void SetupCapturePipeline(uint32_t Width, uint32_t Height, FrameCallback OnFrame);
    void StartSession();
    void CloseSession();
};

using PipeWireCapture = PipeWireCap;
