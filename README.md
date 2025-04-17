# MegaHunt

A multiplayer terminal-based hunting game, forked from `hunt`.

## Description

MegaHunt is a classic terminal-based game where players navigate through a maze and hunt each other down. It was forked from `hunt` a long time ago by Alan Klietz (whom you may recognize as quite possibly the progenitor of the MMORPG).

I used to play this a lot in the 1990s on SPARCStations running SunOS 4.1. I'm not sure if it was originally on jacobs.csos.orst.edu, my UMN-adjacent friends' machines, or Kansas State University originally (I was quite the enthusiastic 'borrower' of shell accounts back then), but it is also certain that in 1993 when the Internet provider that I helped to start purchased its first SPARCStation 2, one of the first things we did was play `megahunt` with dumb terminals. Even then, however, I only had a binary, no source code.

It was my great fortune to track down Alan on LinkedIn, and he was happy to provide the source code. Me and Claude AI proceeded to update it from K&R C and termlib, to C99-compliant and ncurses. It now builds on MacOS.

Pull requests and forks are of course always welcome. Note that the `original` branch of the repository has the unmodified source code.

## Installation

```bash
make
```

## Usage

Start the server:
```bash
./hunt.driver
```

Run the client:
```bash
./hunt
```
## Features

- Multiplayer gameplay
- Terminal-based interface
- Maze navigation
- Various weapons and power-ups

## License

This project is licensed under the BSD License - see the [LICENSE](LICENSE) file for details.