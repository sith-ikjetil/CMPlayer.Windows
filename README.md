# CMPlayer.Windows
License: **GPL-3.0-or-later**  
This is a console music player for Windows.  

## Application
The applications name is **cmplayer** and the following arguments are supported:  
 - --help                    = shows this help screen  
 - --version                 = show version number  
 - --integrity-check         = does a quick integrity check of cmplayer  
 - --purge                   = remove all stored data  
 - --set-max-log-n max       = sets max log entries [25,1000]  
 - --get-max-log-n           = gets max log entries
 - --set-max-history-n max   = sets max history entries [25,1000]
 - --get-max-history-n       = gets max history entries
 - --get-format              = gets music formats to try and play
 - --add-format <.ext>       = adds a format to music formats to play
 - --remove-format <.ext>    = removes a format from music formats
 - --log-information-on      = turn on information logging
 - --log-information-off     = turn off information logging
 - --log-warning-on          = turn on warning logging
 - --log-warning-off         = turn off warning logging
 - --log-error-on            = turn on error logging
 - --log-error-off           = turn off error logging
 - --log-debug-on            = turn on debug logging
 - --log-debug-off           = turn off debug logging
 - --log-other-on            = turn on other logging
 - --log-other-off           = turn off other logging
 - --get-log-status          = gets log status  
  
## Supported File Formats
The following file formats are default supported:
 - .mp3  
 - .m4a  
  
You can add more file types by using the --add-format/--remove-format  
and --get-format command line arguments as long as Windows Media supports it.  
  
 
## Preprocessor Flags
There are the following preprocessor flags defined in Package.swift:
### cSettings and swiftSettings
 - CMP_TARGET_WIN_V11   (Windows 11)
 - CMP_PLATFORM_AMD64   (AMD64 Platform)
 - CMP_PLATFORM_ARM64   (ARM64 Platform)
  
Uncomment any flag that is not appropriate.  
  
## CMPAudioSupport
CMPAudioSupport is a Windows .dll that handles playback and metadata.
  
## How to build CMPlayer.Linux
CMPlayer.Linux is written in Swift. That means that Swift must be  
installed firstly. See www.swift.org for download and install of Swift.
  
Building **cmplayer** is relativly easy. After you clone the  
project using the following:  
```bash
git clone https://github.com/sith-ikjetil/CMPlayer.Windows.git
```
You first must edit Package.swift in the CMPlayer.Linux directory.  
You must uncomment the .define statements that apply, and comment out  
those that do not apply. Like this:  
```swift
cSettings: [
    .define("CMP_TARGET_WIN_V11"),    
    .define("CMP_PLATFORM_AMD64"),
    //.define("CMP_PLATFORM_ARM64")    
],
swiftSettings: [
    .define("CMP_TARGET_WIN_V11"),    
    .define("CMP_PLATFORM_AMD64"),
    //.define("CMP_PLATFORM_ARM64")        
]
```
You should set the CMP_TARGET_? to the closes match to your system. If none of these  
flags work on you system, you might need to add support for your own system.  
  
You then need to execute the following command as first command in cmd.
```cmd
init.cmd
```
then stay in the CMPlayer.Windows directory and execute the following command:  
```cmd
swift build
```
In order to build in release mode you execute the following command:
```cmd
swift build -c release
```
In order to add icon to the binary, execute the following command for building 
and adding a resource.
```cmd
build-add-icon.cmd -c release
``` 
You build the CMPAudioSupport from Visual Studio 2026 and the project CMPAudioSupport. It is 
using C++ language version C++23.
 
After building you can find the binary in the CMPlayer.Windows/.build/x86??/[debug]|[release] 
directory. NB! You need the CMPAudioSupport.dll in the same directory as the executable. Then it is 
simply a job to copy **cmplayer with dll** to a directory of your choice, and execute the application.  
