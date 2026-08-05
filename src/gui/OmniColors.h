#ifndef OMNICOLORS_H
#define OMNICOLORS_H

#pragma once
#include <imgui.h>

namespace OmniTheme {

// Handshake Modal Theme Configuration
constexpr ImU32 COL_HANDSHAKE_ACCENT = IM_COL32(138, 110, 255, 255);
constexpr ImU32 COL_HANDSHAKE_CARD_BG = IM_COL32(36, 32, 56, 255);
constexpr ImU32 COL_HANDSHAKE_CARD_BRD = IM_COL32(52, 46, 75, 255);
constexpr ImU32 COL_HANDSHAKE_CARD_ICON = IM_COL32(175, 145, 255, 255);
constexpr ImU32 COL_HANDSHAKE_BADGE_BG = IM_COL32(18, 18, 26, 255);
constexpr ImU32 COL_HANDSHAKE_BADGE_BRD = IM_COL32(45, 40, 62, 255);
constexpr ImU32 COL_HANDSHAKE_BADGE_ICON = IM_COL32(175, 145, 255, 255);

constexpr ImVec4 COL4_HANDSHAKE_TITLE = ImVec4(0.58f, 0.48f, 0.90f, 1.0f);
constexpr ImVec4 COL4_HANDSHAKE_HOSTNAME = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
constexpr ImVec4 COL4_HANDSHAKE_SUBTEXT = ImVec4(0.40f, 0.40f, 0.52f, 1.0f);
constexpr ImVec4 COL4_HANDSHAKE_LABEL = ImVec4(0.36f, 0.36f, 0.48f, 1.0f);
constexpr ImVec4 COL4_HANDSHAKE_PROTO_ICONS = ImVec4(0.65f, 0.60f, 0.85f, 1.0f);
constexpr ImVec4 COL4_HANDSHAKE_VALUE = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
constexpr ImVec4 COL4_HANDSHAKE_CODE = ImVec4(0.55f, 0.44f, 0.96f, 1.0f);

// Custom Trust Checkbox Theme
constexpr ImU32 COL_HANDSHAKE_CHECK_BG = IM_COL32(20, 20, 30, 255);
constexpr ImU32 COL_HANDSHAKE_CHECK_BG_HOVER = IM_COL32(32, 32, 48, 255);
constexpr ImU32 COL_HANDSHAKE_CHECK_BRD = IM_COL32(56, 56, 82, 255);
constexpr ImU32 COL_HANDSHAKE_CHECKMARK = IM_COL32(153, 128, 250, 255);
constexpr ImU32 COL_HANDSHAKE_TRUST_LABEL = IM_COL32(115, 117, 143, 255);

// Action Buttons Theme
constexpr ImVec4 COL4_BTN_DECLINE_BG = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
constexpr ImVec4 COL4_BTN_DECLINE_BG_HOVER = ImVec4(0.18f, 0.18f, 0.24f, 1.0f);
constexpr ImVec4 COL4_BTN_DECLINE_BG_ACTIVE = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
constexpr ImVec4 COL4_BTN_DECLINE_BRD = ImVec4(0.20f, 0.20f, 0.28f, 1.0f);
constexpr ImVec4 COL4_BTN_DECLINE_TXT = ImVec4(0.78f, 0.78f, 0.85f, 1.0f);

constexpr ImVec4 COL4_BTN_ACCEPT_BG = ImVec4(0.55f, 0.44f, 0.98f, 1.0f);
constexpr ImVec4 COL4_BTN_ACCEPT_BG_HOVER = ImVec4(0.62f, 0.52f, 1.00f, 1.0f);
constexpr ImVec4 COL4_BTN_ACCEPT_BG_ACTIVE = ImVec4(0.45f, 0.35f, 0.90f, 1.0f);
constexpr ImVec4 COL4_BTN_ACCEPT_TXT = ImVec4(0.92f, 0.92f, 0.96f, 1.0f);

} // namespace OmniTheme

// Global GUI Color Tokens
static constexpr ImU32 COL_BG_CHILD_1 = IM_COL32(19, 21, 31, 255);
static constexpr ImU32 COL_BG_CHILD_2 = IM_COL32(24, 26, 38, 255);
static constexpr ImU32 COL_BORDER = IM_COL32(46, 31, 64, 255);
static constexpr ImU32 COL_TITLE_BG = IM_COL32(8, 9, 14, 255);
static constexpr ImVec4 COL4_TEXT_MUTED = ImVec4(0.600f, 0.580f, 0.700f, 1.0f);
static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);
static constexpr ImVec4 COL4_BTN_HOVER_DARK = ImVec4(0.250f, 0.250f, 0.350f, 0.5f);
static constexpr ImVec4 COL4_BTN_HOVER_RED = ImVec4(0.700f, 0.150f, 0.150f, 0.8f);
static constexpr ImVec4 COL4_TRANSPARENT = ImVec4(0.000f, 0.000f, 0.000f, 0.0f);

// Main Style Setup Colors
static constexpr ImVec4 COL4_STYLE_MODAL_DIM = ImVec4(0.050f, 0.050f, 0.050f, 0.65f);
static constexpr ImVec4 COL4_STYLE_CHILD_BG = ImVec4(0.031f, 0.031f, 0.059f, 1.0f);
static constexpr ImVec4 COL4_STYLE_SEPARATOR = ImVec4(0.120f, 0.130f, 0.160f, 1.0f);

// Dashboard & Metrics Theme Text
static constexpr ImVec4 COL4_DASH_TEXT_MUTED = ImVec4(0.450f, 0.420f, 0.550f, 1.0f);
static constexpr ImVec4 COL4_DASH_TEXT_VALUE = ImVec4(0.700f, 0.450f, 1.000f, 1.0f);

// Notification & Event Handling Theme Colors
static constexpr ImVec4 COL4_EV_TEXT_MUTED = ImVec4(0.450f, 0.470f, 0.570f, 1.0f);
static constexpr ImVec4 COL4_EV_ACCENT = ImVec4(0.530f, 0.440f, 0.960f, 1.0f);
static constexpr ImVec4 COL4_EV_ACCENT_HOVER = ImVec4(0.600f, 0.520f, 0.980f, 1.0f);
static constexpr ImVec4 COL4_EV_ACCENT_ACTIVE = ImVec4(0.450f, 0.360f, 0.880f, 1.0f);
static constexpr ImVec4 COL4_EV_FRAME_BG = ImVec4(0.120f, 0.120f, 0.160f, 1.0f);
static constexpr ImVec4 COL4_EV_FRAME_BG_HOVER = ImVec4(0.180f, 0.180f, 0.240f, 1.0f);
static constexpr ImVec4 COL4_EV_BTN_DECLINE = ImVec4(0.140f, 0.140f, 0.170f, 1.0f);
static constexpr ImVec4 COL4_EV_BTN_DEC_HOVER = ImVec4(0.180f, 0.180f, 0.220f, 1.0f);
static constexpr ImVec4 COL4_EV_BTN_DEC_ACTIVE = ImVec4(0.100f, 0.100f, 0.130f, 1.0f);
static constexpr ImVec4 COL4_EV_BTN_DEC_BORDER = ImVec4(0.250f, 0.250f, 0.320f, 1.0f);

// Alert Handling Colors
static constexpr ImVec4 COL4_ALERT_TITLE = ImVec4(1.000f, 0.820f, 0.000f, 1.0f);

// Device Representation Layouts
static constexpr ImU32 COL_DEV_EMPTY = IM_COL32(41, 25, 63, 255);
static constexpr ImU32 COL_DEV_HOVER = IM_COL32(128, 0, 255, 255);
static constexpr ImU32 COL_DEV_DEFAULT = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 COL_RING_BG = IM_COL32(37, 30, 51, 255);

// Sidebar & Navigation Menus
static constexpr ImU32 COL_MENU_BG_IDLE = IM_COL32(30, 30, 40, 0);
static constexpr ImU32 COL_MENU_ICON_IDLE = IM_COL32(30, 30, 40, 255);
static constexpr ImU32 COL_MENU_BG_ACTIVE = IM_COL32(168, 85, 247, 40);
static constexpr ImU32 COL_MENU_ICON_ACT = IM_COL32(168, 85, 247, 20);
static constexpr ImU32 COL_MENU_STRIP = IM_COL32(168, 85, 247, 255);
static constexpr ImU32 COL_MENU_BG_HOVER = IM_COL32(255, 255, 255, 15);
static constexpr ImU32 COL_MENU_TINT_ACT = IM_COL32(168, 85, 247, 255);
static constexpr ImU32 COL_MENU_TINT_IDLE = IM_COL32(140, 140, 160, 255);
static constexpr ImU32 COL_MENU_TXT_ACT = IM_COL32(168, 85, 247, 255);
static constexpr ImU32 COL_MENU_TXT_IDLE = IM_COL32(100, 105, 125, 255);

// Feature Grid Buttons
static constexpr ImU32 COL_FEAT_BG_ACTIVE = IM_COL32(157, 78, 221, 30);
static constexpr ImU32 COL_FEAT_BG_HOVER = IM_COL32(255, 255, 255, 10);
static constexpr ImU32 COL_FEAT_IC_ACTIVE = IM_COL32(157, 78, 221, 35);
static constexpr ImU32 COL_FEAT_IC_HOVER = IM_COL32(255, 255, 255, 10);
static constexpr ImU32 COL_FEAT_TINT_ACT = IM_COL32(157, 78, 221, 255);
static constexpr ImU32 COL_FEAT_TINT_IDLE = IM_COL32(160, 160, 170, 255);
static constexpr ImU32 COL_FEAT_TXT_ACT = IM_COL32(230, 230, 255, 255);
static constexpr ImU32 COL_FEAT_TXT_IDLE = IM_COL32(150, 150, 160, 255);
static constexpr ImU32 COL_FEAT_STRIP = IM_COL32(157, 78, 221, 255);
static constexpr ImU32 COL_FEAT_GLOW = IM_COL32(157, 78, 221, 45);

// Dashboard Layout Drawing
static constexpr ImU32 COL_DASH_BG = IM_COL32(18, 13, 26, 255);
static constexpr ImU32 COL_DASH_BORDER = IM_COL32(46, 31, 64, 255);

// Modal Hardware Connection Layouts
static constexpr ImU32 COL_MODAL_STRIP = IM_COL32(135, 112, 245, 255);
static constexpr ImU32 COL_MODAL_BOX_BG = IM_COL32(40, 40, 55, 100);
static constexpr ImU32 COL_MODAL_BOX_BRD = IM_COL32(75, 70, 105, 255);
static constexpr ImU32 COL_MODAL_CIRC_BG = IM_COL32(23, 23, 30, 255);
static constexpr ImU32 COL_MODAL_CIRC_BRD = IM_COL32(50, 50, 65, 255);

// Bezier Curves
static constexpr ImU32 COL_GLOW_OPAQUE = IM_COL32(128, 0, 255, 255);
static constexpr ImU32 COL_GLOW_HIGH = IM_COL32(128, 0, 255, 153);
static constexpr ImU32 COL_GLOW_MED = IM_COL32(128, 0, 255, 76);
static constexpr ImU32 COL_GLOW_LOW = IM_COL32(128, 0, 255, 25);

#endif // OMNICOLORS_H
