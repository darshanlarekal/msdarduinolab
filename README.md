# Arduino Color Sorter / Pick-and-Place

Firmware for an Arduino UNO R4 WiFi that:

- reads an LDR to identify white, green, red, and blue strips;
- sweeps a servo (M2) to find a calibrated color;
- drives a second servo (A0) to raise and lower a magnet;
- picks an object from one color and places it on another;
- exposes all controls through the serial monitor and a browser UI.

The detector does not hardcode color positions. It calibrates each strip's
LDR signature, then sweeps the arm and classifies each reading against that
table.

## Features

- Three modes: menu, calibration, and continuous detection.
- Four-color calibration under red, blue, green, and white LED light.
- Automatic color detection with stable-reading filtering.
- 5-degree M2 servo movement, with 3-degree fine lift jogging.
- Measured mechanical limits for both servos.
- Electromagnet pick-and-place with configurable settle times.
- Color-to-color operation: pick from color X, place at color Y.
- Web UI and JSON status endpoint over Wi-Fi.
- Serial control fallback when Wi-Fi is not configured or unavailable.
- Servos are not attached or driven at reset, so flashing the board does not
  make the arm jump to a default position.

## Hardware

| Function | Arduino pin | Notes |
|---|---|---|
| LDR | `A2` | Analog input. The LDR is mounted on the M2 arm. |
| RGB LED - red | `D9` | PWM output. |
| RGB LED - green | `D10` | PWM output. |
| RGB LED - blue | `D11` | PWM output. |
| M2 servo signal | `D6` | Board silk marks the M2 header as `6 / + / -`; DIP switch 8 connects D6 to M2. |
| Lift servo signal | `A0` | A0 is used as a digital servo output. Power the servo from 5 V and GND, not from A0. |
| Electromagnet driver | `D12` | Active high. MOSFET or relay driver; never drive the coil directly from a pin. |

### Power and grounding

- Power the servos from a supply that can handle their stall current.
- Connect the servo supply ground to the Arduino ground.
- Power the electromagnet coil from a suitable external supply through a
  MOSFET/relay, with a flyback diode across the coil.
- Brownouts or resetting servos usually mean the 5 V supply cannot handle
  the combined servo current.

### Measured travel limits

| Mechanism | Safe range | Meaning |
|---|---|---|
| M2 arm | `0..120` degrees | `0` is fully left, `120` is fully right. |
| Lift servo | `24..54` degrees | **`24` is fully up; `54` is fully down.** |

The lift direction is intentionally inverted in physical angle: a smaller
angle raises the arm. The firmware clamps all automatic moves and jog commands
to these limits. If the mechanism is assembled differently, change the
constants in `config.h`.

## Repository layout

| File | Purpose |
|---|---|
| `arduino_maintain.ino` | Arduino `setup()` and main `loop()`. |
| `config.h` | Pins, limits, timings, calibration sizes, and command settings. |
| `color_data.cpp/.h` | Compile-time default color table, live color table, LED helpers, and strip-name mapping. |
| `calibration.cpp/.h` | Four-light strip calibration and result printing. |
| `sensing.cpp/.h` | LDR averaging and classification. |
| `servo_arm.cpp/.h` | M2 servo movement, color sweeps, jogging, limits, and scan report. |
| `pick_place.cpp/.h` | Lift servo, magnet, fixed pick/place, and color-to-color pick/place. |
| `detection.cpp/.h` | Continuous color detection and detection-mode commands. |
| `command_input.cpp/.h` | Serial and queued-command input. |
| `wifi_control.cpp/.h` | Wi-Fi connection, HTTP server, web UI, and `/status` JSON. |
| `menu.cpp/.h`, `modes.h` | Mode state and menu/calibration command handling. |
| `arduino_secrets.example.h` | Template for Wi-Fi credentials. |
| `arduino_secrets.h` | Local Wi-Fi credentials; git-ignored. |

## Build and upload

### Arduino IDE

1. Install the **Arduino UNO R4 Boards** core.
2. Install the **Servo** library from Library Manager.
3. Copy the secrets template:

   ```sh
   cp arduino_secrets.example.h arduino_secrets.h
   ```

4. Edit `arduino_secrets.h` with your 2.4 GHz Wi-Fi SSID and password.
5. Open `arduino_maintain.ino`, select **Arduino UNO R4 WiFi**, select the
   serial port, and upload.
6. Open Serial Monitor at **115200 baud**.

### Arduino CLI

```sh
arduino-cli core install arduino:renesas_uno
arduino-cli lib install Servo

cd ~/Arduino
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi arduino_maintain
arduino-cli upload --fqbn arduino:renesas_uno:unor4wifi \
  -p /dev/ttyACM0 arduino_maintain
```

`arduino_secrets.h` is intentionally excluded by `.gitignore`. Never commit
real Wi-Fi credentials.

## First power-up and boot behavior

`setup()` initializes the pins, loads the compile-time color defaults, starts
serial, and starts Wi-Fi. It does **not** attach or write either servo.

The servos attach on the first movement command. This is deliberate: the
stock `Servo::attach()` call pulses the servo and can make it jump to the
library default on every reset. Until the first movement command, the servos
receive no control pulses.

The magnet is initialized off at boot.

## Operating modes

The firmware starts in `MENU` mode.

### Menu mode

Serial keys:

| Key | Action |
|---|---|
| `s` | Load defaults and enter calibration mode. |
| `o` | Load the compile-time defaults and start detection. |
| `a` / `d` or `<` / `>` | Jog M2 by 5 degrees. |
| A number | Move M2 directly to that angle (clamped to `0..120`). |
| `k` | Print M2 travel limits and current position. |
| `v` | Scan M2 travel and print the LDR reading/classification at each step. |
| `w`, `g`, `r`, `b` | Sweep for white, green, red, or blue and park M2 on it. Full words and `h`/`home` for white also work. |
| `u` / `l` | Lift fully up (`24`) / fully down (`54`). |
| `+` / `-` | Fine-jog lift 3 degrees up / down. |
| `m` | Toggle the electromagnet. |
| `p` | Pick at the current M2 position; place at `PLACE_ANGLE` (`90`). |
| `pickwr`-style token | Pick from the first color letter, place at the second. |

### Calibration mode

Enter with `s` from menu mode.

1. Place the LDR over a strip.
2. Use `a`/`d`, `<`/`>`, a number, or `v`/`k` to position the arm.
3. Enter the strip label: `w`, `b`, `g`, or `r`.
4. The firmware measures the strip under red, blue, green, and white light and
   updates the live detector table.
5. Repeat for all four strips. After the fourth strip it starts detection
   automatically.

Other keys:

| Key | Action |
|---|---|
| `x` / `results` | Print all calibration rows collected in this run. |
| `t` / `detect` | Start detection with the current table. |
| `u`/`l`, `+`/`-`, `m`, `p` | Lift, magnet, and pick/place controls. |
| M2 movement keys | Jog, jump, limits, scan. |

Color letters in calibration mode mean **calibrate this strip**, not seek.
Use M2 jog/angle commands to position the sensor for calibration.

### Detection mode

Started by `o` from the menu, by `t` from calibration, or automatically after
all four strips are calibrated.

The firmware repeatedly reads the LDR, classifies it against the live table,
and changes the RGB LED only after three consecutive matching scans. It also
prints the reading, candidate color, and displayed color to serial.

| Key | Action |
|---|---|
| `c` / `menu` | Turn off the RGB LED and return to the menu. |
| `x` / `values` | Print the current detector table. |
| M2 movement keys | Jog, jump, limits, scan, or seek a color. |
| Lift/magnet/pick keys | Same as menu mode. |

## Color-to-color pick and place

The web UI has **Pick color → place color** selectors. Serial uses the same
command form:

```text
pickwr
```

This means **pick White, place Red**. The two letters are the source and
destination color:

- `w` = white
- `g` = green
- `r` = red
- `b` = blue

Sequence:

1. Raise the lift to `24` degrees (fully up).
2. Sweep the LDR and find the source color.
3. Sweep the LDR and find the destination color.
4. If either color is missing, abort without picking anything.
5. If both colors round to the same 5-degree M2 position, abort before pickup.
6. Move to the source, lower to `54`, turn the magnet on, and raise to `24`.
7. Move to the destination while the lift remains at `24`.
8. Reassert lift `24`, lower to `54`, turn the magnet off, and raise to `24`.

The destination is located before pickup, so the arm never carries an object
while searching. If a color is not found, check `v`, `x`, and the calibration
table, or recalibrate.

## Serial command input

Commands are case-insensitive and spaces are ignored. A command ends at a line
ending or after 300 ms without another character. The buffer is limited to
11 characters plus the terminator.

Available lift/pick commands:

| Command | Action |
|---|---|
| `u` / `up` | Lift fully up to `24`. |
| `l` / `lower` | Lift fully down to `54`. |
| `+` | Fine lift 3 degrees up (toward `24`). |
| `-` | Fine lift 3 degrees down (toward `54`). |
| `m` / `magnet` | Toggle the electromagnet. |
| `p` / `pick` | Pick at the current M2 position, place at `90`. |
| `pickXY` | Pick from color `X`, place at color `Y`; e.g. `pickwr` (white to red). |

Fine jog prints the new angle. At a limit it prints `Lift at limit: ...`.

## Web control

The board connects to the configured Wi-Fi network as a station. When it gets
an IP address, the serial monitor prints:

```text
WiFi: open http://192.168.x.x
```

Open that address in a browser.

The page provides:

- mode switching and calibration/detection shortcuts;
- M2 jog, angle jumps, limits, and scan;
- lift up/down, fine jog, magnet toggle, and fixed pick/place;
- color-to-color pick and place dropdowns;
- live status: mode, M2 angle, lift angle, calibrated strip count, LED color,
  candidate color, LDR reading, and magnet state.

Some buttons are mode-dependent. For example, the calibration buttons work in
calibration mode, and the detection seek buttons work in menu or detection
mode. Use the **Menu** button first when switching between workflows.

### HTTP endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `/` or `/index.html` | `GET` | Web control page. |
| `/status` | `GET` | JSON status object. |
| `/cmd?c=<command>` | `GET` | Queue one command. Percent-escape special characters, e.g. `+` is `%2B`. |
| `/favicon.ico` | `GET` | Returns `204 No Content`. |

`/status` fields:

```json
{
  "mode": "MENU",
  "m2": 90,
  "strips": 0,
  "displayed": "WHITE",
  "candidate": "WHITE",
  "ldr": 442.82,
  "lift": 24,
  "magnet": false
}
```

Only one remote command is queued at a time. A second command while the first
is still pending returns HTTP `409 Conflict`. Invalid or malformed commands
return HTTP `400 Bad Request`.

If Wi-Fi is unavailable, serial control still works.

## Default color calibration

`color_data.cpp` contains the compile-time defaults loaded at boot:

```cpp
{"WHITE", 532.54, 547.10, 490.02, 442.82},
{"GREEN", 835.46, 834.04, 756.88, 725.46},
{"RED",   754.56, 806.42, 784.76, 713.64},
{"BLUE",  822.54, 821.76, 809.80, 775.26}
```

Column order is **red light, blue light, green light, white light**. The
`WHITE` row is also the initial displayed color and the home row for detection.

Runtime calibration updates the live `colors[]` table in RAM only. It is not
saved to EEPROM or flash. After a reset, the compile-time defaults are loaded
again. To make a calibration permanent, update `defaultColors` in
`color_data.cpp` and re-upload.

`o` from the menu restores these defaults and starts detection.

## Detection details

- Calibration averages 50 LDR readings per light after a 200 ms settle.
- Detection averages 20 readings per scan.
- A detected color must match for 3 consecutive scans before the LED changes.
- A reading farther than `SEEK_MAX_MATCH_DISTANCE` from every row during a
  white-light color sweep is treated as "nothing" rather than the closest
  color.
- During a color sweep, the arm steps in 5-degree increments with a 150 ms
  settle and requires 2 consecutive hits of the target color.

These values are in `config.h` if the environment needs different timing.

## Troubleshooting

| Symptom | Check |
|---|---|
| Servos do not move immediately after reset | Expected. They attach on the first movement command to avoid a reset jump. |
| Lift moves the wrong way | Confirm `24` is up and `54` is down in `config.h`; swap `LIFT_UP_ANGLE` and `LIFT_DOWN_ANGLE` if the mechanics are reversed. |
| Color is not found during a sweep | Run `v` and `x`, check the strip is under the LDR, and recalibrate. The default table may not match a different rig. |
| Web page does not load | Check `SECRET_SSID`/`SECRET_PASS`, use a 2.4 GHz network, and watch the serial output for the IP address. Serial control works without Wi-Fi. |
| Web command returns `409 Conflict` | Another command is already waiting in the queue. Wait for it to be consumed, then send the next one. |
| Servos twitch or the board resets during motion | Use a stronger separate 5 V servo supply and a common ground. |
| Magnet does not actuate | Check the MOSFET/relay, external coil supply, flyback diode, and `MAGNET_ACTIVE_HIGH`. |
| Calibration disappears after reset | Expected: runtime calibration is RAM-only. Update `defaultColors` for a permanent default. |
| Color pick/place aborts | The source or destination color was not found, or both resolved to the same 5-degree M2 position. Check `x`, `v`, and the calibration table. |

## Configuration quick reference

Most behavior is controlled from `config.h`:

- pin assignments;
- M2 travel and lift travel limits;
- servo step size and delay;
- color sweep samples and settle times;
- detection sample counts and stability threshold;
- magnet timings;
- command buffer size and idle timeout.

Color values live in `color_data.cpp`. Calibration and detection logic live in
`sensing.cpp`, `calibration.cpp`, and `detection.cpp`.
