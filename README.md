# whisperer

A macOS menu-less dictation tool: hold a global hotkey, speak, and the audio is
streamed to Mistral's Voxtral transcription API.

## API key

The Mistral API key is read at runtime, never compiled in. In order of precedence:

1. The `MISTRAL_API_KEY` environment variable (handy when launching from a terminal or IDE).
2. The `mistralApiKey` app setting, which persists across launches (including from Finder):

   ```sh
   defaults write com.sheinhtike.whisperer mistralApiKey "your-key-here"
   ```

## Building

Requires Qt 6, libcurl, FFmpeg, and [QHotkey](https://github.com/Skycoder42/QHotkey).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
