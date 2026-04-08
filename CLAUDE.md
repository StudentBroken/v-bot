# V-Bot Wall Plotter Firmware

ESP32-S3-based firmware for a wall-hanging gondola plotter (V-plotter / wall plotter). The gondola hangs from two strings wound on stepper motors anchored at the top corners. A servo lifts/lowers the pen.

## Hardware

- **MCU**: ESP32-S3 SuperMini (4MB flash, USB CDC)
- **Motors**: 2× stepper motors (A4988 drivers, 16 microsteps) — left (X axis) and right (Y axis)
- **Pen lift**: SG90 servo on GPIO 6
- **LED**: NeoPixel status LED
- **Connectivity**: WiFi AP mode (`V-Bot` / `vbot1234`), WebUI at `http://192.168.4.1`

### Pin assignments (`src/config.h`)
| Signal | GPIO |
|--------|------|
| Left STEP | 1 |
| Left DIR | 2 |
| Right STEP | 4 |
| Right DIR | 5 |
| Stepper ENABLE | 6 (shared, active LOW) |
| Servo PWM | 7 |

## Build & Flash

Uses PlatformIO.

```bash
# Build
pio run

# Flash + monitor
pio run -t upload && pio device monitor

# Upload filesystem (WebUI assets from data/)
pio run -t uploadfs
```

Target: `env:esp32s3` (ESP32-S3 DevKitC-1 compatible).

## Architecture

```
src/
  main.cpp          — setup() / loop() entry point
  config.h          — pin defs, compile-time defaults
  settings.h/.cpp   — runtime settings persisted to LittleFS (/settings.json)
  motion.h/.cpp     — MotionController: AccelStepper wrapper, FreeRTOS task, move queue (32 cmds)
  kinematics.h      — Pure math: Cartesian ↔ cable lengths (forward + inverse)
  gcode.h/.cpp      — G-code parser & executor
  calibration.h/.cpp— Multi-step calibration wizard (retract → release → set width)
  servo_pen.h       — PenServo: up/down angles from settings
  status_led.h      — NeoPixel state indicator (READY / DRAWING / CALIBRATING / PAUSED / MOVING)
  webserver.h/.cpp  — ESPAsyncWebServer + REST API, serves data/ via LittleFS
  log_buffer.h/.cpp — Ring buffer for log streaming to WebUI

data/               — WebUI (served from LittleFS)
  index.html
  css/style.css
  js/{main,api,controls,files,settings,state,terminal,whiteboard}.js
```

## Coordinate System

Origin is the **left anchor point**. X increases rightward, Y increases downward (gravity direction).

```
Left Anchor (0,0) ─────────────── Right Anchor (W,0)
        \                                /
    Ll   \                              /  Lr
          \                            /
           ●  Pen  (x, y)
```

Kinematics in `src/kinematics.h`:
- **Inverse**: `cartesianToLengths(x, y, anchorWidth, gondolaWidth, leftLen, rightLen)`
- **Forward**: `lengthsToCartesian(leftLen, rightLen, anchorWidth, gondolaWidth, x, y)`

## Key Settings (`src/settings.h`)

All persisted to `/settings.json` on LittleFS and adjustable via WebUI:

| Field | Default | Notes |
|-------|---------|-------|
| `anchor_width_mm` | 1300 | Distance between anchor points |
| `gondola_width_mm` | 0 | Distance between string attach points on gondola |
| `steps_per_mm` | 50.93 | (200 steps × 16 microsteps) / (20mm pulley × π) |
| `max_speed_mm_min` | 8000 | |
| `acceleration` | 200 mm/s² | |
| `pen_up_angle` | 60° | Servo angle for pen up |
| `pen_down_angle` | 150° | Servo angle for pen down |
| `calibration_release_mm` | 1000 | String length released during calibration |

## Motion System

- **No FreeRTOS** — runs entirely on the main loop via `motion.update()` in `loop()`
- Uses `MultiStepper` (AccelStepper's built-in coordinator) so both motors arrive simultaneously — this is the correct primitive for a V-plotter where Cartesian lines require coordinated cable changes
- `update()` calls `MultiStepper::run()` (non-blocking, one step per call); when a segment finishes it immediately pops the next
- Segment queue: 64-slot circular buffer of pre-computed cable-length targets
- `getPenX()`/`getPenY()` return the **planning tip** (end of last queued move) so the GCode parser chains moves correctly during pipelining
- `getLivePenX()`/`getLivePenY()` read back from actual stepper steps (for status display)
- Auto-saves position to settings after 10 s idle for crash recovery

## Calibration Flow

1. `CAL_WAIT_RETRACTED` — user manually retracts strings to top
2. `CAL_RELEASING` — firmware releases `calibration_release_mm` of string
3. `CAL_WAIT_WIDTH` — user measures and inputs anchor width via WebUI
4. `CAL_COMPLETE` — position is set; machine is ready

## Serial Commands (115200 baud)

| Command | Action |
|---------|--------|
| `TEST` | List test commands |
| `TEST_LEFT` | Left motor: 500 steps fwd then back |
| `TEST_RIGHT` | Right motor: 500 steps fwd then back |
| `TEST_SERVO` | Toggle pen up/down |
| `TEST_RELEASE` | Release 50mm on both motors |
| `TEST_RETRACT` | Retract 50mm on both motors |
| `STATUS` | Print position, cable lengths, memory |
| Any G-code | Execute directly |

## G-code Support

Standard subset: `G0`/`G1` (rapid/feed move), `G28` (home), `M3`/`M5` (pen down/up), feed rate via `F`.

## FluidNC Reference

`fluidnc/` contains a FluidNC v4.0.1 binary and `config.yaml` — this is an alternative firmware approach using the FluidNC WallPlotter kinematics. The custom firmware in `src/` is the active implementation.
