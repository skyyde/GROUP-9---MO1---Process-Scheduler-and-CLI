# GROUP-9---MO1---Process-Scheduler-and-CLI


### Build and run on Windows

1. Install Visual Studio or Build Tools with **Desktop development with C++**, including CMake and Ninja.
2. Open **x64 Native Tools Command Prompt for VS** and go to the project folder:

   ```bat
   cd /d "C:\path\to\GROUP-9---MO1---Process-Scheduler-and-CLI"
   ```

   Replace the path with where you saved the project.

3. Build and run from that folder:

   ```bat
   cmake -S . -B build-run -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-run
   .\build-run\csopesy.exe
   ```

   After editing the code, run the build command again before launching.

### Commands to try

```text
initialize
screen -ls
exit
```

`initialize` reads `config.txt`, so run the program from the project folder.
`screen -ls` currently shows the later-phase message. The other screen, scheduler
and report commands are also placeholders. Process creation is available through
the internal Phase 2 API; it is not connected to the console yet.

If `cmake` or the compiler cannot be found, check that you opened the VS developer
prompt and installed the C++ tools. If initialization cannot find `config.txt`,
check your current folder.

