#pragma once

#include <core/Types.h>

// The editor's icons: glyphs of the Lucide icon font (ISC license, lucide-static 0.460.0),
// as UTF-8 strings to put in any text. Only the ones listed here are in the font atlas;
// a new icon is added to the design system's Icons card and to this list.
// Names follow Lucide's (https://lucide.dev/icons).

namespace VKE::UI::Icons {

    // Assets
    constexpr const char* Boxes = "\xee\x8b\x90"; // boxes
    constexpr const char* Box = "\xee\x81\xa5"; // box
    constexpr const char* Palette = "\xee\x87\x9d"; // palette
    constexpr const char* Image = "\xee\x83\xb9"; // image
    constexpr const char* Clapperboard = "\xee\x8a\x9b"; // clapperboard
    constexpr const char* Package = "\xee\x84\xad"; // package
    constexpr const char* Folder = "\xee\x83\x9c"; // folder
    constexpr const char* FolderOpen = "\xee\x89\x87"; // folder-open
    constexpr const char* FolderPlus = "\xee\x83\x9e"; // folder-plus
    constexpr const char* File = "\xee\x83\x84"; // file
    constexpr const char* FilePlus = "\xee\x83\x8d"; // file-plus
    constexpr const char* Type = "\xee\x86\x98"; // type

    // Components
    constexpr const char* Move3D = "\xee\x8b\xa5"; // move-3d
    constexpr const char* Sun = "\xee\x85\xbc"; // sun
    constexpr const char* Lightbulb = "\xee\x87\x82"; // lightbulb
    constexpr const char* Flashlight = "\xee\x83\x98"; // flashlight
    constexpr const char* Video = "\xee\x86\xa5"; // video
    constexpr const char* Scan = "\xee\x89\x97"; // scan
    constexpr const char* CircleDashed = "\xee\x92\xb5"; // circle-dashed

    // Tools
    constexpr const char* Move = "\xee\x84\xa5"; // move
    constexpr const char* Rotate3D = "\xee\x8b\xaa"; // rotate-3d
    constexpr const char* Scaling = "\xee\x8b\xac"; // scaling
    constexpr const char* Crosshair = "\xee\x82\xb0"; // crosshair
    constexpr const char* Globe = "\xee\x83\xab"; // globe
    constexpr const char* Magnet = "\xee\x8a\xb5"; // magnet
    constexpr const char* Grid3x3 = "\xee\x83\xac"; // grid-3x3
    constexpr const char* Focus = "\xee\x8a\x9e"; // focus

    // Play
    constexpr const char* Play = "\xee\x85\x80"; // play
    constexpr const char* Pause = "\xee\x84\xb2"; // pause
    constexpr const char* Square = "\xee\x85\xab"; // square
    constexpr const char* StepForward = "\xee\x8f\xae"; // step-forward

    // Panels
    constexpr const char* ListTree = "\xee\x90\x8d"; // list-tree
    constexpr const char* SlidersHorizontal = "\xee\x8a\x9a"; // sliders-horizontal
    constexpr const char* Terminal = "\xee\x86\x85"; // terminal
    constexpr const char* Gamepad2 = "\xee\x83\xa2"; // gamepad-2

    // Log
    constexpr const char* Info = "\xee\x83\xbe"; // info
    constexpr const char* TriangleAlert = "\xee\x86\x93"; // triangle-alert
    constexpr const char* CircleX = "\xee\x82\x88"; // circle-x

    // Actions
    constexpr const char* Plus = "\xee\x85\x81"; // plus
    constexpr const char* Minus = "\xee\x84\xa0"; // minus
    constexpr const char* Trash2 = "\xee\x86\x8e"; // trash-2
    constexpr const char* Copy = "\xee\x82\xa2"; // copy
    constexpr const char* Save = "\xee\x85\x91"; // save
    constexpr const char* Search = "\xee\x85\x95"; // search
    constexpr const char* Eye = "\xee\x82\xbe"; // eye
    constexpr const char* EyeOff = "\xee\x82\xbf"; // eye-off
    constexpr const char* Lock = "\xee\x84\x8f"; // lock
    constexpr const char* LockOpen = "\xee\x84\x90"; // lock-open
    constexpr const char* CircleDot = "\xee\x8d\x89"; // circle-dot
    constexpr const char* EllipsisVertical = "\xee\x82\xbb"; // ellipsis-vertical
    constexpr const char* Ellipsis = "\xee\x82\xba"; // ellipsis
    constexpr const char* ChevronRight = "\xee\x81\xb3"; // chevron-right
    constexpr const char* ChevronDown = "\xee\x81\xb1"; // chevron-down
    constexpr const char* ChevronLeft = "\xee\x81\xb2"; // chevron-left
    constexpr const char* ChevronUp = "\xee\x81\xb4"; // chevron-up
    constexpr const char* X = "\xee\x86\xb2"; // x
    constexpr const char* Check = "\xee\x81\xb0"; // check
    constexpr const char* Layers = "\xee\x84\x84"; // layers
    constexpr const char* LayoutGrid = "\xee\x84\x85"; // layout-grid
    constexpr const char* List = "\xee\x84\x8c"; // list
    constexpr const char* RefreshCw = "\xee\x85\x89"; // refresh-cw
    constexpr const char* Settings = "\xee\x85\x98"; // settings
    constexpr const char* BookOpen = "\xee\x81\xa3"; // book-open
    constexpr const char* ArrowUp = "\xee\x81\x8e"; // arrow-up
    constexpr const char* LogOut = "\xee\x84\x92"; // log-out
    constexpr const char* Ban = "\xee\x81\x95"; // ban
    constexpr const char* Link = "\xee\x84\x88"; // link
    constexpr const char* Unlink = "\xee\x86\x9c"; // unlink
    constexpr const char* Pencil = "\xee\x87\xb9"; // pencil
    constexpr const char* ExternalLink = "\xee\x82\xbd"; // external-link

    // Code points of every icon above, for building the font atlas.
    inline constexpr u32 kCodepoints[] = {
        0xe2d0,
        0xe065,
        0xe1dd,
        0xe0f9,
        0xe29b,
        0xe12d,
        0xe0dc,
        0xe247,
        0xe0de,
        0xe0c4,
        0xe0cd,
        0xe198,
        0xe2e5,
        0xe17c,
        0xe1c2,
        0xe0d8,
        0xe1a5,
        0xe257,
        0xe4b5,
        0xe125,
        0xe2ea,
        0xe2ec,
        0xe0b0,
        0xe0eb,
        0xe2b5,
        0xe0ec,
        0xe29e,
        0xe140,
        0xe132,
        0xe16b,
        0xe3ee,
        0xe40d,
        0xe29a,
        0xe185,
        0xe0e2,
        0xe0fe,
        0xe193,
        0xe088,
        0xe141,
        0xe120,
        0xe18e,
        0xe0a2,
        0xe151,
        0xe155,
        0xe0be,
        0xe0bf,
        0xe10f,
        0xe110,
        0xe349,
        0xe0bb,
        0xe0ba,
        0xe073,
        0xe071,
        0xe072,
        0xe074,
        0xe1b2,
        0xe070,
        0xe104,
        0xe105,
        0xe10c,
        0xe149,
        0xe158,
        0xe063,
        0xe04e,
        0xe112,
        0xe055,
        0xe108,
        0xe19c,
        0xe1f9,
        0xe0bd,
    };

} // namespace VKE::UI::Icons
