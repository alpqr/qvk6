Experiments for a Rendering Hardware Interface abstraction for Qt 6 (QtRhi)
===================================================================

The API and its backends (Vulkan, OpenGL (ES) 2.0, Direct3D 11) are reasonably complete
in the sense that it should be possible to bring up a Qt Quick renderer on top of them
(using Vulkan-style GLSL as the "common" shading language - translation seems to work
pretty well for now, even to HLSL). Next up is a Metal backend and some cleanup.


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
The public API is seen solid already. The plan forward is to add a few enhancements, for instance to allow including DXBC or DXIL as well in the shader packs (by invoking fxc or dxc from qsb).
