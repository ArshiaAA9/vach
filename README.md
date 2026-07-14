# Vach
A low-latency voice streaming project for using simple commands like next, previous, stop, play with spotify.  
The project works by streaming your voice from your android phone to your pc using websockets and using Whisper ai to transcribe it into text  
then it will parse it for commands.
  More commands will be added later.

# Installation
you can either download the [latest release](https://github.com/ArshiaAA9/Vach/releases/latest) or build the project yourself
## Manual Building:
Cloning and building the server side:
```bash
git clone https://github.com/ArshiaAA9/vach.git
cd vach
cmake -B build
cmake --build build --parallel $(nproc)
```
For the android part [Download the latest APK](https://github.com/ArshiaAA9/Vach/releases/latest)

# Usage
Open a server by running: 
```
cd build
./vach -s
```
then open the android program and enter the server's ip
