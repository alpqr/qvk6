Note: Further development is now happening at https://git.qt.io/laagocs/qtrhi

Experiments for a Rendering Hardware Interface abstraction for Qt 6.

Relies on https://github.com/alpqr/qtshaderstack17 for shader management.

The API and its backends (Vulkan, OpenGL (ES) 2.0, Direct3D 11) are reasonably complete
in the sense that it should be possible to bring up a Qt Quick renderer on top of them
(using Vulkan-style GLSL as the "common" shading language - translation seems to work
pretty well for now, even to HLSL). Next up is a Metal backend and some cleanup.
