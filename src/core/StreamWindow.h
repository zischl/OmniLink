#ifndef STREAMWINDOW_H
#define STREAMWINDOW_H

#pragma once

#if defined(_WIN32)
class WinForge;
using StreamWindow = WinForge;
#elif defined(__linux__)
class LinuxForge;
using StreamWindow = LinuxForge;
#endif

#endif
