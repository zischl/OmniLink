#include "SpatialRouter.hpp"

void SpatialEdgeRouter::SyncConditions(int32_t Margin_)
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

            TopEdges.Add(HEdge1, HEdge2, MinTop + Margin);
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

            LeftEdges.Add(VEdge1, VEdge2, MinLeft + Margin);
        }
    }
}
