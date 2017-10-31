Thenesis Quake II v1.1

This is a port of id Software's Quake II which focus on embedded devices like the Raspberry Pi, GCW Zero or Creator CI20.

It works also under Windows 7 and PC Linux.

It is based on Yamagi Quake II version 5.26 which is itself based on Icculus Quake II, which is itself based on Quake II 3.21.

How to build
------------

For Linux systems, there is a few dependencies that you need to install before building, if needed:
- The development package for SDL 2:
  sudo apt-get install libsdl2-dev
  They are available by default on the latest OS release
- The development headers and libraries for EGL and OpenGLES 1.
  This is completely platform specific.
  For Windows, you can take these libraries from the SDK of Imagination Technologies.
  For the GCW Zero, these libraries already exist in the Cygwin SDK.
  For the Raspberry Pi and Creator CI20, they are available by default.

Next, using either MSYS, Cygwin or any terminal available, enter in the directory corresponding to your platform:
- For Linux:
  Premake/Build-Linux
- For Windows:
  Premake/Build-Windows
- For GCW Zero:
  Premake/Build-GCWZero
  Under Cygwin, you must override the default GCC executable if you use make.
  I don't know how to do this because I use Codelite.
- For Raspberry Pi:
  Premake/Build-RaspberryPi
- For Creator CI20:
  Premake/Build-CreatorCI20

And type make.

TODO
----

- Add a virtual keyboard for targets without keyboard. This is necessary to enter player name, internet server address, etc
- Add auto-aiming when using a stick. A possibility is to select targets successively by pressing a button, then auto-aim with another button.
- For GLES 2.0 version, use buffers for meshes and to lerp between frames in the shader.
- Remove mesh optimization. This is not necessary with current hardware, just perform a quick indexing.
- Add "modifier" keys/buttons so that we can use the same button for several actions when modifiers are pressed.

History
-------

Version 1.1:
- Added GLES 2 version.
- Added more command bindings in the key menu. This is useful for targets without keyboard and for some not well documented commands.
- Optimizations mainly in rendering.
- Fixed all known bugs in rendering.
- Rendering code refactoring (again).
- A lot of bug fixes everywhere.
