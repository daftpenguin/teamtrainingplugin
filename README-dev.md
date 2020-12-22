# TeamTrainingPlugin Dev Guide

A few people have showed interest in potentially contributed to this plugin, so I've written up some details that may help with setting up your environment.

Please note that I am not very much experienced in modifying games nor am I well versed in programming for Windows. Much of this text is simply just documentation for the actions I have taken to setup my own environment. If these actions don't work, I might be useless with debugging your issues, but you're still welcome to DM me on Discord. I also welcome suggestions for improvements.

## Dependencies

This plugin has dependencies on the boost c++ libraries and OpenSSL (>= v1.1.0) which is required by cpp-httplib.

### Installing Boost

This plugin was built on Boost 1.72.0 which I built from source, which can be downloaded from [here](https://www.boost.org/). After downloading and unpacking the source, open a terminal and cd into the source directory. Then run `bootstrap` followed by `b2 runtime-link=static`.

After building the libraries, you will likely need to update the properties in Visual Studio to point to the location where you installed boost. Update `C/C++ -> Additional Include Directories` to point to the directory where you unzipped the source to, and update `Linker -> Additional Include Directories` to the `stage\lib` directory inside the directory you placed the source (this directory is created by b2, unless you changed the install location).

### Installing OpenSSL

I've used the prebuilt binaries provided by [SLP](https://slproweb.com/products/Win32OpenSSL.html). Install the non-light version wherever you please, but the project is configured to include the lib and include directories from `C:\Program Files\OpenSSL-Win64` and thus you would need to update them if you install them elsewhere.

## Modifying Post-Build Event

You will likely need to modify the post build event (right click solution, properties, build events -> post build event) to point to the location where bakkesmod-patch.exe exists. This is included in the bakkesmodsdk folder inside the bakkesmod installation folder. I should really update this at some point.