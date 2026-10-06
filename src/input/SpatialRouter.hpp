#ifndef SPATIAL_ROUTER
#define SPATIAL_ROUTER

#include "OmniEnums.hpp"
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <array>
#include <cstdint>

struct EdgeSegment
{
    int32_t BoundStart;
    int32_t BoundEnd;
    int32_t Threshold;
};

enum ThresholdDirection { LEFT = 0, TOP = 0, RIGHT = 1, BOTTOM = 1 };

struct EdgeSegmentMap
{
  private:
    static constexpr uint8_t              MAX_SEGMENTS = 8;
    std::array<EdgeSegment, MAX_SEGMENTS> Segments{};
    uint8_t                               Count = 0;

  public:
    inline void Add(int32_t EdgeStart, int32_t EdgeEnd, int32_t Threshold)
    {
        if (Count < MAX_SEGMENTS)
            Segments[Count++] = {EdgeStart, EdgeEnd, Threshold};
    }

    inline void Clear() { Count = 0; }

    template <ThresholdDirection Directionality>
    [[nodiscard]] inline bool BoundsCheck(int32_t BoundCoordinate, int32_t EdgeCoordinate) const
    {
        for (uint8_t Index = 0; Index < Count; Index++) {
            if (BoundCoordinate >= Segments[Index].BoundStart &&
                BoundCoordinate < Segments[Index].BoundEnd) {

                if constexpr (Directionality) {
                    return EdgeCoordinate >= Segments[Index].Threshold;
                } else {
                    return EdgeCoordinate <= Segments[Index].Threshold;
                }
            }
        }

        return false;
    }
};

class SpatialEdgeRouter
{
  private:
    uint8_t ActiveMask = 0;

    EdgeSegmentMap TopEdges;
    EdgeSegmentMap BottomEdges;
    EdgeSegmentMap LeftEdges;
    EdgeSegmentMap RightEdges;

    RECT VirtualBounds{};
    RECT InnerBounds{};

    int32_t Margin = 2;

    [[nodiscard]] inline bool BitState(DeviceMap DeviceID) const
    {
        return (ActiveMask & static_cast<uint8_t>(1U << (static_cast<uint8_t>(DeviceID) - 1))) != 0;
    }

  public:
    void AddEdgeCondition(DeviceMap DeviceID)
    {
        if (DeviceID != DeviceMap::C0 && DeviceID != DeviceMap::END) {
            ActiveMask |= 1U << static_cast<uint8_t>(DeviceID - 1);
        }
    }

    void RemoveEdgeCondition(DeviceMap DeviceID)
    {
        if (DeviceID != DeviceMap::C0 && DeviceID != DeviceMap::END) {
            ActiveMask &= ~(1U << static_cast<uint8_t>(DeviceID - 1));
        }
    }

    bool GetState(DeviceMap DeviceID)
    {
        if (DeviceID != DeviceMap::C0 && DeviceID != DeviceMap::END) {
            return (ActiveMask & (1U << static_cast<uint8_t>(DeviceID - 1))) != 0;
        }

        return false;
    }

    uint8_t GetActiveMask() { return ActiveMask; }

    void Reset() { ActiveMask = 0; }

    void SyncConditions(int32_t Margin_ = 2)
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

                    MaxRight =
                        (std::max)(MaxRight, static_cast<int32_t>(Monitors.data[MIndex].right));

                    MinLeft = (std::min)(MinLeft, static_cast<int32_t>(Monitors.data[MIndex].left));
                }
            }

            if (EdgeHit) {

                RightEdges.Add(VEdge1, VEdge2, MaxRight - Margin);

                LeftEdges.Add(VEdge1, VEdge2, MinLeft + Margin);
            }
        }
    }

    [[nodiscard]] inline DeviceMap Evaluate(int X, int Y) const
    {
        if (!ActiveMask)
            return DeviceMap::C0;

        if (X > InnerBounds.left && X < InnerBounds.right && Y > InnerBounds.top &&
            Y < InnerBounds.bottom)
            return DeviceMap::C0;

        if (BitState(DeviceMap::LU1) && X <= VirtualBounds.left + Margin && Y <= VirtualBounds.top)
            return DeviceMap::LU1;
        if (BitState(DeviceMap::RU1) && X >= VirtualBounds.right - Margin && Y <= VirtualBounds.top)
            return DeviceMap::RU1;
        if (BitState(DeviceMap::LD1) && X <= VirtualBounds.left + Margin &&
            Y >= VirtualBounds.bottom)
            return DeviceMap::LD1;
        if (BitState(DeviceMap::RD1) && X >= VirtualBounds.right - Margin &&
            Y >= VirtualBounds.bottom)
            return DeviceMap::RD1;

        if (BitState(DeviceMap::D1) && BottomEdges.BoundsCheck<ThresholdDirection::BOTTOM>(X, Y))
            return DeviceMap::D1;
        if (BitState(DeviceMap::U1) && TopEdges.BoundsCheck<ThresholdDirection::TOP>(X, Y))
            return DeviceMap::U1;
        if (BitState(DeviceMap::R1) && RightEdges.BoundsCheck<ThresholdDirection::RIGHT>(Y, X))
            return DeviceMap::R1;
        if (BitState(DeviceMap::L1) && LeftEdges.BoundsCheck<ThresholdDirection::LEFT>(Y, X))
            return DeviceMap::L1;

        return DeviceMap::C0;
    };
};

#endif // !SPATIAL_ROUTER
