# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EchoType is a multi-component Windows input monitoring system consisting of three independent applications:

1. **EchoKey** (Rust) - System-level keyboard input logger with GUI
2. **FloatingTool** (C#) - Quick feedback popup interface
3. **ClipboardMonitor** (C++) - Clipboard change tracking utility

Each component is self-contained with its own build system and can be developed independently.

## Build Commands

### EchoKey (Rust)
```bash
cd ../echokey
cargo build --release    # Production build (optimized for size)
cargo build              # Debug build (faster compilation)
cargo run                # Run in debug mode
cargo test               # Run tests
```
Binary output: `target/release/echokey.exe` or `target/debug/echokey.exe`

### FloatingTool (C#)
```bash
cd FloatingTool
dotnet build -c Release  # Production build
dotnet build             # Debug build
dotnet run               # Run the application
```
Binary output: `bin/Release/net8.0-windows/FloatingTool.exe`

### ClipboardMonitor (C++)
```bash
cd ClipboardMonitor
mkdir build && cd build
cmake .. && cmake --build . --config Release
```
Alternative: Run `build.bat` for direct compilation with MSVC.

## Architecture

### EchoKey Threading Model

EchoKey uses a multi-threaded architecture with channel-based communication:

```
Main Thread (GUI - egui)
    ↓
Keyboard Thread (Windows hook) → Logic Thread ← Tray Thread
    ↓                                              ↓
  mpsc::channel<KeyboardEvent>    mpsc::channel<TrayEvent>
```

**Key files**:
- `keyboard_win.rs` - Low-level Windows keyboard hook with 30ms deduplication window
- `logger.rs` - Buffered I/O with immediate flushing, daily file rotation, manual segments
- `gui.rs` - egui-based GUI for log viewing and statistics
- `tray.rs` - System tray integration using tray-icon/muda crates

**State management**: Shared state uses `Arc<Mutex<State>>` for thread-safe access across GUI, logic, and tray threads.

**Event flow**:
1. Windows hook captures keyboard input
2. Events sent via `mpsc::channel` to logic thread
3. Logic thread processes and forwards to logger
4. Logger writes immediately with explicit flush for data persistence
5. GUI reads shared state with 50ms timeout on channel operations

### Data Storage Locations

- **EchoKey logs**: `%LOCALAPPDATA%\EchoKey\logs\` - Daily rotation with timestamps
- **ClipboardMonitor**: `%APPDATA%\ClipboardMonitor\clipboard_history.json`
- **EchoKey config**: `%LOCALAPPDATA%\EchoKey\config.toml`

### Important Design Patterns

**Deduplication**: The keyboard hook in `keyboard_win.rs` maintains a deduplication window (30ms) to filter out rapid duplicate events from the Windows input system. This prevents log spam from key repeat or multiple event sources.

**Data Integrity**: The logger uses buffered I/O but explicitly flushes after each write operation. This ensures no data loss if the application crashes while maintaining reasonable performance. The timeout on channel receives (50ms) prevents blocking while keeping UI responsive.

**Binary Size Optimization**: The release profile in `Cargo.toml` uses aggressive optimizations:
- `opt-level = "z"` for size
- LTO enabled
- Single codegen unit
- Strip symbols

## Platform-Specific Details

### Windows API Usage

**EchoKey (Rust)**: Uses `windows-rs` crate for native Windows API access:
- `SetWindowsHookExW` for low-level keyboard hooks
- Registry manipulation via `winreg` crate for autostart
- System tray integration

**FloatingTool (C#)**: Uses P/Invoke for:
- Global hotkey registration (Alt+Q)
- Window positioning at cursor location
- Always-on-top behavior

**ClipboardMonitor (C++)**: Direct Windows API calls:
- Clipboard format iteration
- Window handle tracking for source application detection
- Shell APIs for executable path resolution

## Hotkeys and User Interaction

**EchoKey**:
- `Ctrl+Shift+P` - Pause/resume logging
- `Ctrl+Shift+S` - Save current clipboard content
- `Ctrl+Shift+N` - Start new manual segment
- System tray menu for all operations

**FloatingTool**:
- `Alt+Q` - Show feedback popup at cursor location
- `Enter` - Submit feedback text
- Click buttons for Like/Dislike/Neutral feedback

## Cross-Component Communication

The three components operate independently with no direct inter-process communication. They are designed to work alongside each other as separate utilities:

- **EchoKey** monitors keyboard input continuously
- **FloatingTool** provides on-demand quick feedback capture
- **ClipboardMonitor** tracks all clipboard operations

Each writes to its own log file location and can be run independently.

## Development Notes

### When modifying the keyboard hook (keyboard_win.rs)

The deduplication logic is critical. The 30ms window was chosen empirically to filter duplicates without losing legitimate rapid input. Modifying this requires testing with:
- Rapid key presses
- Key repeat scenarios
- Multiple input devices

### When modifying the logger (logger.rs)

The immediate flush after writes is intentional for data safety. Any buffering changes must maintain this guarantee. The daily file rotation happens at midnight local time - ensure thread-safety when accessing the current file handle.

### When modifying the GUI (gui.rs)

The GUI runs on the main thread with egui's immediate mode paradigm. Long-running operations must not block this thread. Use the existing channel pattern with timeouts to communicate with the logic thread.

## Configuration Management

EchoKey uses `config.rs` for settings management with `serde` serialization. Configuration is loaded at startup and saved when modified through the GUI. Default locations and timeout values are defined here.
