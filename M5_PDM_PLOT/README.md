# M5Stack CoreS3 PDM Diagnostic Plotter (2026 Edition)

This project implements a high-visibility diagnostic dashboard on the M5Stack CoreS3. It displays a 4x3 grid of 78x78 pixel icons representing various motor and machinery diagnostic states, featuring real-time alarm simulation and high-fidelity color reproduction.

## Features
- **4x3 Dashboard**: Optimized layout for 320x240 display using 78x78 pixel icons.
- **Original Color Fidelity**: Icons are rendered in their original RGB colors (RGB565).
- **Automated Pipeline**: Python script to convert JPG source images into C-style headers.
- **Sequential Alarm Simulation**: A sequential alarm cycles through the 12 icons every 10 seconds, highlighted by a flashing red triangle in the top-right corner.
- **Endianness Correction**: Uses `M5.Display.setSwapBytes(true)` for correct color rendering.

## Project Structure
- `M5_PDM_PLOT.ino`: Main Arduino sketch for the CoreS3.
- `icons_2026.h`: C-style header file containing the RGB565 bitmap data for 12 icons (78x78).
- `PNG2026/`: Directory containing the source JPG images.
- `generate_icons_2026.py`: Python script to convert JPG files from `PNG2026/` into the `icons_2026.h` header.

## Technical Details
- **Grid Layout**: 4 columns x 3 rows.
- **Icon Size**: 78x78 pixels.
- **Total Resolution**: 312x234 (Centered on 320x240 screen).
- **Color Space**: 16-bit RGB565.
- **Background**: Black (0x0000).

## Workflow: Updating Icons
1. Place 12 JPG files in the `PNG2026/` directory (named `*_1.jpg` through `*_12.jpg`).
2. Run the conversion script:
   ```bash
   python generate_icons_2026.py
   ```
3. Compile and upload:
   ```bash
   arduino-cli compile --fqbn m5stack:esp32:m5stack_cores3 M5_PDM_PLOT.ino
   arduino-cli upload -p COM6 --fqbn m5stack:esp32:m5stack_cores3 M5_PDM_PLOT.ino
   ```

## Prerequisites
- **Hardware**: M5Stack CoreS3
- **Software**: `arduino-cli`, Python 3.x (with `Pillow`), M5Unified/M5GFX libraries.
