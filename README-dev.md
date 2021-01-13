# TeamTrainingPlugin Dev Guide

A few people have showed interest in potentially contributed to this plugin, so I've written up some details that may help with setting up your environment.

Please note that I am not very much experienced in modifying games nor am I well versed in programming for Windows. Much of this text is simply just documentation for the actions I have taken to setup my own environment. If these actions don't work, I might be useless with debugging your issues, but you're still welcome to DM me on Discord. I also welcome suggestions for improvements.

## Dependencies

This plugin has dependencies on the boost c++ libraries and OpenSSL (>= v1.1.0) which is required by cpp-httplib.

### Installing Boost

This plugin was built on Boost 1.72.0 which I built from source, which can be downloaded from [here](https://www.boost.org/). After downloading and unpacking the source, open a terminal and cd into the source directory. Then run `bootstrap` followed by `b2 runtime-link=static`.

After building the libraries, you will likely need to update the properties in Visual Studio to point to the location where you installed boost. Update `C/C++ -> Additional Include Directories` to point to the directory where you unzipped the source to, and update `Linker -> Additional Include Directories` to the `stage\lib` directory inside the directory you placed the source (this directory is created by b2, unless you changed the install location).

### Installing OpenSSL


Some issues were found when statically linking to the precompiled libraries found online which prevented the plugin from being unloaded, causing issues with updating the plugin. To fix this, the libraries needed to be compiled with the no-dso configuration. I have already compiled the 1.1.1i version and shared this in the #programming channel of the BakkesMod discord which you can find pinned there, or download using [this link](https://cdn.discordapp.com/attachments/448093289137307658/796315907768713226/openssl-1.1.1i-static-no-dso.zip).

If you want to build them yourself, I followed the steps documented [here](https://wiki.openssl.org/index.php/Compilation_and_Installation#W64). In the `perl Configure...` step I simply added `no-shared` and `no-dso`.

Note the zip only contains the libraries, and not the includes. The includes can be found [here](https://github.com/openssl/openssl/tree/90cebd1b216e0a160fcfd8e0eddca47dad47c183/include) which you will need in order to compile. You will also need to update the `C/C++ -> Additional Include Directories` and `Linker -> Additional Include Directories` to point to the OpenSSL include directory and the directory you've extracted the compiled libaries to, respectively. I hope to clean this up in the future.

## Modifying Post-Build Event

You will likely need to modify the post build event (right click solution, properties, build events -> post build event) to point to the location where bakkesmod-patch.exe exists. This is included in the bakkesmodsdk folder inside the bakkesmod installation folder. I should really update this at some point.