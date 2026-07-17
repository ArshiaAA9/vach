# Vach
Vach streams microphone audio from an Android device to a desktop server over WebSockets. The server uses Whisper to transcribe the audio into text and then parses the transcription to execute supported commands.

More commands and features will be added in future releases.

# Installation
You can either download the [latest release](https://github.com/ArshiaAA9/Vach/releases/latest) or build the project manually yourself
## Manual Building:
Clone the repository and build the server:
```bash
git clone https://github.com/ArshiaAA9/vach.git
cd vach
cmake -B build
cmake --build build --parallel $(nproc)
```
Then download and install the Android APK from the [the latest release](https://github.com/ArshiaAA9/Vach/releases/latest)

# Usage
Open a server by running: 
```
cd build
./vach -s
```
Launch the Android app, enter the server's local IP address, and connect.
