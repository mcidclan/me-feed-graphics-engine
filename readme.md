## Me/Gu Sample code
A sample code writing Ge list from the Media Engine using Gu and native Ge commands

# Overview
The main idea is to write/update a Ge list from the Media Engine and then signal its execution to the Graphics Engine using a small trigger on the main Cpu.
It uses a shared double list uncached with 0x40000000, ensuring that Ge can read from the range that has been filled, while allowing Me to write to the range that is not currently read.
This allows us to concurrently write drawing processes to be executed on the Graphics Engine, from both Cpu.

# Building
A) Run ./build.sh from a bash
B) Via cmake and ninja: `cmake -Bbuild -G Ninja`, `cmake --build build`
C) Via cmake and make: `cmake -Bbuild`, `cmake --build build`

## Special Thanks
This sample code wouldn't have been possible without the resources from the PSP homebrew community, which served as valuable sources of knowledge.
Thanks to **crazyc** from ps2dev.org, without whom the use of the Media Engine in the community would be far more difficult.
Thanks to all developers and contributors who have kept the scene alive and to those who continue to do so.

# resources:
- [uofw on GitHub](https://github.com/uofw/uofw)
- [psp wiki on PSDevWiki](https://www.psdevwiki.com/psp/)
- [pspdev on GitHub](https://github.com/pspdev)

*m-c/d*
