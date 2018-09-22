Experiments for a Rendering Hardware Interface abstraction for Qt 6.

Relies on https://github.com/alpqr/qtshaderstack17 for shader management.

The API and the Vulkan backend are reasonably complete for now in the sense
that it should be possible to bring up a Qt Quick renderer on top of them.
Before that however, some more backends are needed to prove the story.
So next up is OpenGL ES 2.0 + GLSL 100.
