# GROUP-9---MO1---Process-Scheduler-and-CLI

- Pallarca, Cedri
- Cruz, Enrique
- Aguirre, Kyla
- Reyes, Nicos

## Build and run on Windows

1. Install Visual Studio or Build Tools with **Desktop development with C++**, including CMake and Ninja.
2. Open **x64 Native Tools Command Prompt for VS** and go to the project folder:

   ```bat
   cd /d "C:\path\to\GROUP-9---MO1---Process-Scheduler-and-CLI"
   ```

   Replace the path with where you saved the project. This should be the folder containing `CMakeLists.txt` and `config.txt`.

3. Build and run:

   ```bat
   cmake -S . -B build-run -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-run
   .\build-run\csopesy.exe
   ```

   After editing the code, run `cmake --build build-run` again before launching.

`initialize` loads `config.txt` from the project folder. If using Visual Studio's green Run/Debug arrow, the CMake project sets the debugger's working folder to the project root.

## Console commands

At the root prompt, enter:

```text
initialize
screen -ls
screen -s demo
```

Inside a process screen, use:

```text
process-smi
exit
```

Back at the root prompt, these commands are also available:

```text
screen -r demo
scheduler-start
scheduler-stop
report-util
exit
```

- `screen -ls` displays CPU utilization, process status, and runtime status.
- `screen -s <name>` creates a process and opens its screen.
- `screen -r <name>` resumes an unfinished process.
- `process-smi` refreshes the current process screen.
- `scheduler-start` and `scheduler-stop` control automatic process generation.
- `report-util` saves the utilization report to `csopesy-log.txt`.
- `exit` leaves the current process screen, or exits the console from the root prompt.
