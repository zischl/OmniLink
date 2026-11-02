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

namespace SpatialConfig {
// Exact physical boundary margin for edge crossing
constexpr int32_t EdgeTriggerMargin = 1;

// Inward padding when cursor arrives at destination monitor
constexpr int32_t EdgeArrivalPadding = 5;

// Recoil push back distance when edge cross or return occurs
constexpr int32_t EdgePushBackPadding = 5;

} // namespace SpatialConfig

class OmniSpatialRouter
{
  private:
    uint8_t ActiveMask = 0;

    EdgeSegmentMap TopEdges;
    EdgeSegmentMap BottomEdges;
    EdgeSegmentMap LeftEdges;
    EdgeSegmentMap RightEdges;

    RECT VirtualBounds{};
    RECT InnerBounds{};

    int32_t Margin = SpatialConfig::EdgeTriggerMargin;

    [[nodiscard]] inline bool BitState(DeviceMap DeviceID) const
    {
        return (ActiveMask & static_cast<uint8_t>(1U << (static_cast<uint8_t>(DeviceID) - 1))) != 0;
    }

  public:
    // Adds a spatial edge detection condition based on directionality map DeviceMap
    void AddEdgeCondition(DeviceMap DeviceID)
    {
        if (DeviceID != DeviceMap::C0 && DeviceID != DeviceMap::END) {
            ActiveMask |= 1U << static_cast<uint8_t>(DeviceID - 1);
        }
    }

    // Removes the spatial edge detection condition added by AddEdgeCondition, obvious much ?
    void RemoveEdgeCondition(DeviceMap DeviceID)
    {
        if (DeviceID != DeviceMap::C0 && DeviceID != DeviceMap::END) {
            ActiveMask &= ~(1U << static_cast<uint8_t>(DeviceID - 1));
        }
    }

    // What state ? state of the spatial router
    // Returns true when the device edge matching the DeviceID is a monitored edge condition
    // else false..
    [[nodiscard]] bool GetState(DeviceMap DeviceID) const
    {
        if (DeviceID != DeviceMap::C0 && DeviceID != DeviceMap::END) {
            return (ActiveMask & (1U << static_cast<uint8_t>(DeviceID - 1))) != 0;
        }

        return false;
    }

    [[nodiscard]] uint8_t GetActiveMask() const { return ActiveMask; }

    void Reset() { ActiveMask = 0; }

    void SyncConditions(int32_t Margin_ = SpatialConfig::EdgeTriggerMargin);

    // X Y Point evaluation for edge conditions.
    [[nodiscard]] inline DeviceMap Evaluate(int X, int Y) const
    {
        if (!ActiveMask)
            return DeviceMap::C0;

        if (X > InnerBounds.left && X < InnerBounds.right && Y > InnerBounds.top &&
            Y < InnerBounds.bottom)
            return DeviceMap::C0;

        if (BitState(DeviceMap::LU1) && X <= VirtualBounds.left + Margin - 1 &&
            Y <= VirtualBounds.top + Margin - 1)
            return DeviceMap::LU1;
        if (BitState(DeviceMap::RU1) && X >= VirtualBounds.right - Margin &&
            Y <= VirtualBounds.top + Margin - 1)
            return DeviceMap::RU1;
        if (BitState(DeviceMap::LD1) && X <= VirtualBounds.left + Margin - 1 &&
            Y >= VirtualBounds.bottom - Margin)
            return DeviceMap::LD1;
        if (BitState(DeviceMap::RD1) && X >= VirtualBounds.right - Margin &&
            Y >= VirtualBounds.bottom - Margin)
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
    }

    // Window boundary evaluation that outputs which edge a window border overlaps
    // Designed to handle 2 edges as the maximum amount of edges a window can overlap
    // Starts with a quick reject if completely inside desktop virtual bounds
    [[nodiscard]] inline DeviceMap EvaluateWindow(const RECT& WindowRect) const
    {
        if (!ActiveMask)
            return DeviceMap::C0;

        if (WindowRect.left >= VirtualBounds.left && WindowRect.right <= VirtualBounds.right &&
            WindowRect.top >= VirtualBounds.top && WindowRect.bottom <= VirtualBounds.bottom) {
            return DeviceMap::C0;
        }

        const bool Left   = (WindowRect.left < VirtualBounds.left) && BitState(DeviceMap::L1);
        const bool Right  = (WindowRect.right > VirtualBounds.right) && BitState(DeviceMap::R1);
        const bool Top    = (WindowRect.top < VirtualBounds.top) && BitState(DeviceMap::U1);
        const bool Bottom = (WindowRect.bottom > VirtualBounds.bottom) && BitState(DeviceMap::D1);

        if (Top && Right && BitState(DeviceMap::RU1))
            return DeviceMap::RU1;
        if (Top && Left && BitState(DeviceMap::LU1))
            return DeviceMap::LU1;
        if (Bottom && Right && BitState(DeviceMap::RD1))
            return DeviceMap::RD1;
        if (Bottom && Left && BitState(DeviceMap::LD1))
            return DeviceMap::LD1;

        if (Right)
            return DeviceMap::R1;
        if (Left)
            return DeviceMap::L1;
        if (Bottom)
            return DeviceMap::D1;
        if (Top)
            return DeviceMap::U1;

        return DeviceMap::C0;
    }

    // Virtual bounds is what u get from multiple monitors in one merged layout
    [[nodiscard]] inline const RECT& GetVirtualBounds() const { return VirtualBounds; }

    [[nodiscard]] inline int32_t GetVirtualWidth() const
    {
        return VirtualBounds.right - VirtualBounds.left;
    }
    [[nodiscard]] inline int32_t GetVirtualHeight() const
    {
        return VirtualBounds.bottom - VirtualBounds.top;
    }

    static void EncodeEdgePosition(
        DeviceMap   Edge,
        const RECT& WindowPosition,
        uint32_t    ResWidth,
        uint32_t    ResHeight,
        int16_t&    RatioX,
        int16_t&    RatioY
    );

    static void DecodeEdgePosition(
        DeviceMap Edge,
        int16_t   RatioX,
        int16_t   RatioY,
        uint32_t  ResWidth,
        uint32_t  ResHeight,
        int&      TargetX,
        int&      TargetY
    );
};

#endif // !SPATIAL_ROUTER
