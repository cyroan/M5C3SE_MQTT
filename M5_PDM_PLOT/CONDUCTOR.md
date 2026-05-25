# Project Conductor: M5_PDM_PLOT

## 🎯 Project Goals
- [x] Implement a high-visibility diagnostic dashboard on M5Stack CoreS3.
- [x] Create a 6x2 grid of 50x50 pixel icons for machinery diagnostics.
- [x] Develop a flashing alarm simulation to highlight critical states.
- [ ] Implement real-time data integration via PDM/Serial (Pending).

## 🚀 Current Status: Active Development
- **Current Phase**: Phase 4 - UI Refinement & Alarm Logic.
- **Latest Update**: Switched alarm indicator from a flashing frame to a flashing red triangle.

## 📝 Task List

### Done (Completed)
- [x] **Setup**: Initial project structure and `arduino-cli` configuration.
- [x] **UI**: 6x2 Icon grid layout on CoreS3.
- [x] **Tooling**: Python script for PNG-to-Header (`icont.h`) conversion.
- [x] **Alarm**: Sequential alarm cycling logic (10s cycle).
- [x] **UI Refinement**: Changed alarm highlight to a red triangle in the top-right corner.
- [x] **Documentation**: Updated `README.md` to reflect the new alarm indicator.
- [x] **Knowledge Management**: Created `m5stack-arduino-dev` skill for cross-project reuse.

### In Progress
- [ ] **Optimization**: Reviewing memory usage for icon bitmaps.

### To Do (Backlog)
- [ ] **Interaction**: Add touch feedback for specific icons to show detailed data.
- [ ] **Connectivity**: Integrate with real PDM sensor data.
- [ ] **Themes**: Add a Dark Mode toggle (Black background, White icons).

## 🕒 History & Significant Changes
- **2026-05-20**:
    - Changed the non-display screen background to a clean, premium Light Slate (RGB: 200, 205, 215) based on user preference.
    - Adjusted the color probe text and swatch outline to BLACK to maintain high contrast and readability on the light background.
    - Refactored `nonDisplayColor` into a global variable to unify setup, dashboard, and color probe backgrounds seamlessly.
- **2026-05-19**: 
    - Replaced flashing thick frame with a flashing red triangle in `drawDashboard()`.
    - Created and installed the `m5stack-arduino-dev` skill.
    - Established `CONDUCTOR.md` for project orchestration.
- **Phase 3**: Finalized PNG-to-Header export pipeline.
- **Phase 2**: Reference-based design from `ChatGPT.png` and `condition.png`.
