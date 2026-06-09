#pragma once

class PipeWireCap
{
  public:
    PipeWireCap();
    ~PipeWireCap();
    void Start();
    void Stop();
    inline void AcquireFrame() {}
    inline void ReleaseFrame() {}
};

using PipeWireCapture = PipeWireCap;
