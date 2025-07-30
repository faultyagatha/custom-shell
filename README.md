# Custom shell

## Summary

A minimal shell (like bash or zsh) that supports jobs, pipelines, signals (Ctrl+C and Ctrl+Z), and foreground/background process handling.

## Description

This program is a basic Unix-like shell that:

- Reads commands from the user (`fgets`)
- Parses them into commands and arguments
- Handles built-in commands like `jobs`, `fg`, `bg`, `quit`, `help`
- Executes other commands by forking and `execvp`
- Supports pipelines (`|`) for connecting commands
- Tracks running processes as "jobs" with states:
  - Foreground (FG)
  - Background (BG)
  - Stopped (via `Ctrl+Z` → `SIGTSTP`)
  - Default (not active)

- Handles signals:
  - `SIGINT` (`Ctrl+C`) to kill foreground processes
  - `SIGTSTP` (`Ctrl+Z`) to stop foreground processes
  - `SIGCHLD` to track when children exit or stop.

## Job State + Signal Flow Diagram

                  +---------------------+
                  |     New Job (exec)  |
                  +---------------------+
                            |
                            v
                  +---------------------+
                  |   Foreground (FG)   |
                  +---------------------+
                   |        |        |
                   |        |        |
                   |        |        +-----------------------------+
                   |        |                                     |
                   |        |                                   Ctrl+C
                   |        |                               or natural exit
                   |        v                                     |
                   |   +---------------------+                    |
                   |   |      Stopped (T)     |<------------------+
                   |   +---------------------+
                   |        |        |
                Ctrl+Z      |        | SIGCONT + fg
           (SIGTSTP)        |        v
                            |   +---------------------+
                            |   | Background (BG)     |
                            |   +---------------------+
                            |        |        |
                            |        |        |
                            |        |        +------------------+
                            |        |                           |
                            |        |                        Ctrl+C
                            |        |                    or natural exit
                            |        v                           |
                            +-----> Terminated <-----------------+
                                   (exit or signal)
