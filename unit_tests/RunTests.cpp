#include <iostream>

void PlatformIntrinsicsTest();
void OmniUUIDTest();
void BurstQTest();
void ByteStreamTest();
void InstanceRegistryTest();
void WinForgeTest();
void D3D11RendererTest();
void WinCapTest();
void CaptureControllerTest();
void NetSessionTest();
void NvEncDecTest();

int main()
{

    /* PlatformIntrinsicsTest();
    OmniUUIDTest();
    BurstQTest();
    ByteStreamTest();
    InstanceRegistryTest();
    WinForgeTest();
    D3D11RendererTest();
    WinCapTest();
    NetSessionTest(); */
    NvEncDecTest();
    // CaptureControllerTest();

    std::cout << "Press enter to exit\n";
    std::cin.clear();

    if (std::cin.rdbuf()->in_avail() > 0) {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::cin.get();
    return 0;
}
