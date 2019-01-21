![](https://github.com/alpqr/qvk6/blob/master/rhi2.png)

Experiments for a Rendering Hardware Interface abstraction for future Qt (QtRhi)
========================================================================

The API and its backends (Vulkan, OpenGL (ES) 2.0, Direct3D 11, Metal) are
reasonably complete in the sense that it should be possible to bring up a Qt
Quick renderer on top of them (using Vulkan-style GLSL as the "common" shading
language - translation seems to work pretty well for now, including to HLSL and
MSL). Other than that this is highly experimental with a long todo list, and the
API will change in arbitrary ways. It nonetheless shows what a possible future
direction for the Qt graphics stack could be.

Experiments for more modern graphics shader management in future Qt (QtShaderTools)
===================================================================

Uses https://github.com/KhronosGroup/SPIRV-Cross and https://github.com/KhronosGroup/glslang

QShaderBaker: Compile (Vulkan-flavor) GLSL to SPIR-V. Generate reflection info.
Translate to HLSL, MSL, and various GLSL versions. Optionally rewrite vertex
shaders to make them suitable for Qt Quick scenegraph batching. Pack all this
into conveniently (de)serializable QBakedShader instances. Complemented by a
command-line tool (qsb) to allow doing the expensive work offline. This
optionally allows invoking fxc or metal/metallib to include compiled bytecode
for HLSL and MSL as well.

Documentation
=============

Generated docs are now online at https://alpqr.github.io

In action
=========

Needs Qt 5.12. Tested on Windows 10 with MSVC2015 and 2017, and macOS 10.14 with XCode 10.

Screenshots from the test application demonstrating basic drawing, pipeline
state (blending, depth), indexed drawing, texturing, and rendering into a
texture. All using the same code and the same two sets of vertex and fragment
shaders, with the only difference being in the QWindow setup.

![](https://github.com/alpqr/qvk6/blob/master/screenshot_d3d.png)
![](https://github.com/alpqr/qvk6/blob/master/screenshot_gl.png)
![](https://github.com/alpqr/qvk6/blob/master/screenshot_vk.png)
![](https://github.com/alpqr/qvk6/blob/master/screenshot_mtl.png)

Additionally, check
https://github.com/alpqr/qvk6/blob/master/examples/rhi/hellominimalcrossgfxtriangle/hellominimalcrossgfxtriangle.cpp
for a single-source, cross-API example of drawing a triangle.
