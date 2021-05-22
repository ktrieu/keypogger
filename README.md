# keypogger
Have people told you your keyboard is too loud on video calls? Show them what loud really means.

Keypogger uses `SetWindowsHookEx` to capture all keyboard input, and then plays a `.wav` file of your choice on every keypress.

This also serves as a quick XAudio2 demo, if you want to see the minimal code it takes to play a `.wav` file with that library.

## Usage:
```
keypogger [audio_file]
```
`audio_file` must be a `.wav` file. No other formats are supported. Use `ffmpeg` or something.
