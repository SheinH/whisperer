# whisperer

A macOS menu-less dictation tool: hold a global hotkey, speak, and the audio is
streamed as WAV to Azure Speech fast transcription using Microsoft's MAI-Transcribe-2 model.

## API key

The Azure Speech key is read at runtime, never compiled in. In order of precedence:

1. The `AZURE_SPEECH_KEY` environment variable (handy when launching from a terminal or IDE).
2. The `azureSpeechKey` app setting, which persists across launches (including from Finder):

   ```sh
   defaults write com.sheinhtike.whisperer azureSpeechKey "your-key-here"
   ```

## Building

Requires Qt 6, libcurl, FFmpeg, and [QHotkey](https://github.com/Skycoder42/QHotkey).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
