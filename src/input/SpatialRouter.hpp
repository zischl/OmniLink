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

    void SyncConditions(int32_t Margin_);

    [[nodiscard]] inline DeviceMap Evaluate(int X, int Y) const
    {
        if (!ActiveMask)
            return DeviceMap::C0;

        if (X > InnerBounds.left && X < InnerBounds.right && Y > InnerBounds.top &&
            Y < InnerBounds.bottom)
            return DeviceMap::C0;

        if (BitState(DeviceMap::LU1) && X <= VirtualBounds.left + Margin &&
            Y <= VirtualBounds.top + Margin)
            return DeviceMap::LU1;
        if (BitState(DeviceMap::RU1) && X >= VirtualBounds.right - Margin &&
            Y <= VirtualBounds.top + Margin)
            return DeviceMap::RU1;
        if (BitState(DeviceMap::LD1) && X <= VirtualBounds.left + Margin &&
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
    };
};

#endif // !SPATIAL_ROUTER
