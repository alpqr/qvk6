![](https://git.qt.io/laagocs/qtrhi/raw/master/rhi2.png)

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

1. qmake && (n)make

2. QT += shadertools

3. Use QSpirvCompiler to compile (Vulkan-flavored) GLSL to SPIR-V. This can also optionally rewrite the input source to make it suitable for the batched pass of Qt Quick's default OpenGL renderer.

4. Use QSpirvShader parse a SPIR-V binary to get reflection data, to strip the binary, and to translate to GLSL suitable for various OpenGL (ES) versions. (or to HLSL/MSL)

5. The reflection data (QShaderDescription) can also be serialized to binary and human-readable JSON, and deserialized from binary JSON.

Alternatively,

3. Run the qsb tool to generate a single file with reflection info and multiple variants (SPIR-V, HLSL, various GLSL versions) of the input shader source.

4. Use QBakedShader to load and access those at run time.

The latter is what the RHI uses, and expects applications to provide QBakedShader packs.

In action
=========

Screenshots from the test application demonstrating basic drawing, pipeline
state (blending, depth), indexed drawing, texturing, and rendering into a
texture. All using the same code and the same two sets of vertex and fragment
shaders, with the only difference being in the QWindow setup.

![](https://git.qt.io/laagocs/qtrhi/raw/master/screenshot_d3d.png)
![](https://git.qt.io/laagocs/qtrhi/raw/master/screenshot_gl.png)
![](https://git.qt.io/laagocs/qtrhi/raw/master/screenshot_vk.png)
![](https://git.qt.io/laagocs/qtrhi/raw/master/screenshot_mtl.png)

Additionally, check
https://git.qt.io/laagocs/qtrhi/raw/master/examples/rhi/hellominimalcrossgfxtriangle/hellominimalcrossgfxtriangle.cpp
for a single-source, cross-API example of drawing a triangle.
