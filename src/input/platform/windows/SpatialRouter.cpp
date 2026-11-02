#include "SpatialRouter.hpp"

void OmniSpatialRouter::SyncConditions(int32_t Margin_)
{
    Margin = Margin_;

    struct MonitorList
    {
        RECT data[8];
        int  count = 0;
    } Monitors;

    EnumDisplayMonitors(
        NULL,
        NULL,
        [](HMONITOR, HDC, LPRECT rc, LPARAM lp) -> BOOL {
            auto* list = reinterpret_cast<MonitorList*>(lp);
            if (list->count < 8)
                list->data[list->count++] = *rc;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&Monitors)
    );

    if (Monitors.count == 0)
        return;

    VirtualBounds = Monitors.data[0];
    for (int i = 1; i < Monitors.count; ++i) {
        VirtualBounds.left   = (std::min)(VirtualBounds.left, Monitors.data[i].left);
        VirtualBounds.top    = (std::min)(VirtualBounds.top, Monitors.data[i].top);
        VirtualBounds.right  = (std::max)(VirtualBounds.right, Monitors.data[i].right);
        VirtualBounds.bottom = (std::max)(VirtualBounds.bottom, Monitors.data[i].bottom);
    }

    InnerBounds = {
        VirtualBounds.left + Margin + 2,
        VirtualBounds.top + Margin + 2,
        VirtualBounds.right - Margin - 2,
        VirtualBounds.bottom - Margin - 2
    };

    int32_t HEdges[16];
    int     HEdgeCount = 0;

    // Getting unique horizontal edges
    for (int MIndex = 0; MIndex < Monitors.count; ++MIndex) {

        bool LState = false, RState = false;

        for (int Edge = 0; Edge < HEdgeCount; ++Edge) {

            if (HEdges[Edge] == Monitors.data[MIndex].left)
                LState = true;

            if (HEdges[Edge] == Monitors.data[MIndex].right)
                RState = true;
        }

        if (!LState && HEdgeCount < 16)
            HEdges[HEdgeCount++] = Monitors.data[MIndex].left;

        if (!RState && HEdgeCount < 16)
            HEdges[HEdgeCount++] = Monitors.data[MIndex].right;
    }

    // Sort..
    for (int Index = 1; Index < HEdgeCount; ++Index) {

        int32_t CurrentEdge = HEdges[Index];
        int     PrevIndex   = Index - 1;

        while (PrevIndex >= 0 && HEdges[PrevIndex] > CurrentEdge) {
            HEdges[PrevIndex + 1] = HEdges[PrevIndex];
            --PrevIndex;
        }

        HEdges[PrevIndex + 1] = CurrentEdge;
    }

    BottomEdges.Clear();
    TopEdges.Clear();

    // Getting top and bottom edge breakpoints
    for (int HIndex = 0; HIndex + 1 < HEdgeCount; ++HIndex) {

        int32_t HEdge1 = HEdges[HIndex], HEdge2 = HEdges[HIndex + 1];

        int32_t MaxBottom = INT32_MIN, MinTop = INT32_MAX;

        bool EdgeHit = false;

        for (int MIndex = 0; MIndex < Monitors.count; ++MIndex) {

            if (Monitors.data[MIndex].left <= HEdge1 && Monitors.data[MIndex].right >= HEdge2) {

                EdgeHit = true;

                MaxBottom =
                    (std::max)(MaxBottom, static_cast<int32_t>(Monitors.data[MIndex].bottom));

                MinTop = (std::min)(MinTop, static_cast<int32_t>(Monitors.data[MIndex].top));
            }
        }

        if (EdgeHit) {

            BottomEdges.Add(HEdge1, HEdge2, MaxBottom - Margin);

            // why -1 ? Windows RECT exclusive right/bottom
            TopEdges.Add(HEdge1, HEdge2, MinTop + (Margin - 1));
        }
    }

    // Same as above but for Vertical Edges

    int32_t VEdges[16];
    int     VEdgeCount = 0;

    // Getting unique vertical edges
    for (int MIndex = 0; MIndex < Monitors.count; ++MIndex) {

        bool TState = false, BState = false;

        for (int VIndex = 0; VIndex < VEdgeCount; ++VIndex) {

            if (VEdges[VIndex] == Monitors.data[MIndex].top)
                TState = true;

            if (VEdges[VIndex] == Monitors.data[MIndex].bottom)
                BState = true;
        }

        if (!TState && VEdgeCount < 16)
            VEdges[VEdgeCount++] = Monitors.data[MIndex].top;

        if (!BState && VEdgeCount < 16)
            VEdges[VEdgeCount++] = Monitors.data[MIndex].bottom;
    }

    // sort.. again..
    for (int VIndex = 1; VIndex < VEdgeCount; ++VIndex) {

        int32_t CurrentEdge = VEdges[VIndex];
        int     PrevIndex   = VIndex - 1;

        while (PrevIndex >= 0 && VEdges[PrevIndex] > CurrentEdge) {
            VEdges[PrevIndex + 1] = VEdges[PrevIndex];
            --PrevIndex;
        }

        VEdges[PrevIndex + 1] = CurrentEdge;
    }

    RightEdges.Clear();
    LeftEdges.Clear();

    // Getting left and right edge breakpoints
    for (int VIndex = 0; VIndex + 1 < VEdgeCount; ++VIndex) {

        int32_t VEdge1 = VEdges[VIndex], VEdge2 = VEdges[VIndex + 1];

        int32_t MaxRight = INT32_MIN, MinLeft = INT32_MAX;

        bool EdgeHit = false;

        for (int MIndex = 0; MIndex < Monitors.count; ++MIndex) {

            if (Monitors.data[MIndex].top <= VEdge1 && Monitors.data[MIndex].bottom >= VEdge2) {

                EdgeHit = true;

                MaxRight = (std::max)(MaxRight, static_cast<int32_t>(Monitors.data[MIndex].right));

                MinLeft = (std::min)(MinLeft, static_cast<int32_t>(Monitors.data[MIndex].left));
            }
        }

        if (EdgeHit) {

            RightEdges.Add(VEdge1, VEdge2, MaxRight - Margin);

            // why -1 ? Windows RECT exclusive right/bottom
            LeftEdges.Add(VEdge1, VEdge2, MinLeft + (Margin - 1));
        }
    }
}

void OmniSpatialRouter::EncodeEdgePosition(
    DeviceMap   Edge,
    const RECT& WindowPosition,
    uint32_t    ResWidth,
    uint32_t    ResHeight,
    int16_t&    RatioX,
    int16_t&    RatioY
)
{
    if (ResWidth == 0 || ResHeight == 0) {
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top);
        return;
    }

    switch (Edge) {
    case DeviceMap::R1: {
        RatioX          = static_cast<int16_t>(WindowPosition.left - static_cast<int>(ResWidth));
        int64_t ScaledY = (static_cast<int64_t>(WindowPosition.top) * 65535LL) / ResHeight;
        RatioY          = static_cast<int16_t>(std::clamp<int64_t>(ScaledY, 0, 65535));
        break;
    }
    case DeviceMap::L1: {
        RatioX          = static_cast<int16_t>(WindowPosition.left);
        int64_t ScaledY = (static_cast<int64_t>(WindowPosition.top) * 65535LL) / ResHeight;
        RatioY          = static_cast<int16_t>(std::clamp<int64_t>(ScaledY, 0, 65535));
        break;
    }
    case DeviceMap::D1: {
        int64_t ScaledX = (static_cast<int64_t>(WindowPosition.left) * 65535LL) / ResWidth;
        RatioX          = static_cast<int16_t>(std::clamp<int64_t>(ScaledX, 0, 65535));
        RatioY          = static_cast<int16_t>(WindowPosition.top - static_cast<int>(ResHeight));
        break;
    }
    case DeviceMap::U1: {
        int64_t ScaledX = (static_cast<int64_t>(WindowPosition.left) * 65535LL) / ResWidth;
        RatioX          = static_cast<int16_t>(std::clamp<int64_t>(ScaledX, 0, 65535));
        RatioY          = static_cast<int16_t>(WindowPosition.top);
        break;
    }
    case DeviceMap::RU1: {
        RatioX = static_cast<int16_t>(WindowPosition.left - static_cast<int>(ResWidth));
        RatioY = static_cast<int16_t>(WindowPosition.top);
        break;
    }
    case DeviceMap::LU1: {
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top);
        break;
    }
    case DeviceMap::RD1: {
        RatioX = static_cast<int16_t>(WindowPosition.left - static_cast<int>(ResWidth));
        RatioY = static_cast<int16_t>(WindowPosition.top - static_cast<int>(ResHeight));
        break;
    }
    case DeviceMap::LD1: {
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top - static_cast<int>(ResHeight));
        break;
    }
    default:
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top);
        break;
    }
}

void OmniSpatialRouter::DecodeEdgePosition(
    DeviceMap Edge,
    int16_t   RatioX,
    int16_t   RatioY,
    uint32_t  ResWidth,
    uint32_t  ResHeight,
    int&      TargetX,
    int&      TargetY
)
{
    if (ResWidth == 0 || ResHeight == 0) {
        TargetX = RatioX;
        TargetY = RatioY;
        return;
    }

    switch (Edge) {
    case DeviceMap::R1: {
        TargetX          = RatioX;
        uint16_t URatioY = static_cast<uint16_t>(RatioY);
        TargetY = static_cast<int>((static_cast<uint64_t>(URatioY) * ResHeight) / 65535ULL);
        break;
    }
    case DeviceMap::L1: {
        TargetX          = static_cast<int>(ResWidth) + RatioX;
        uint16_t URatioY = static_cast<uint16_t>(RatioY);
        TargetY = static_cast<int>((static_cast<uint64_t>(URatioY) * ResHeight) / 65535ULL);
        break;
    }
    case DeviceMap::D1: {
        uint16_t URatioX = static_cast<uint16_t>(RatioX);
        TargetX          = static_cast<int>((static_cast<uint64_t>(URatioX) * ResWidth) / 65535ULL);
        TargetY          = RatioY;
        break;
    }
    case DeviceMap::U1: {
        uint16_t URatioX = static_cast<uint16_t>(RatioX);
        TargetX          = static_cast<int>((static_cast<uint64_t>(URatioX) * ResWidth) / 65535ULL);
        TargetY          = static_cast<int>(ResHeight) + RatioY;
        break;
    }
    case DeviceMap::RU1: {
        TargetX = RatioX;
        TargetY = static_cast<int>(ResHeight) + RatioY;
        break;
    }
    case DeviceMap::LU1: {
        TargetX = static_cast<int>(ResWidth) + RatioX;
        TargetY = static_cast<int>(ResHeight) + RatioY;
        break;
    }
    case DeviceMap::RD1: {
        TargetX = RatioX;
        TargetY = RatioY;
        break;
    }
    case DeviceMap::LD1: {
        TargetX = static_cast<int>(ResWidth) + RatioX;
        TargetY = RatioY;
        break;
    }
    default:
        TargetX = RatioX;
        TargetY = RatioY;
        break;
    }
}
