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
- White-patch RGB calibration with manual pot adjustment or automatic LED balancing.
- Four-color calibration under red, blue, green, and white LED light.
- Automatic color detection with stable-reading filtering.
- 16x2 LCD status display (parallel or I2C).
- Optional object-sensor-assisted lift (disabled in `config.h` until fitted) and a five-second electromagnet pickup.
- Color-to-color assignment flow: prompt for calibration, sweep the base, prompt for pickup/drop colors, then run one cycle.
- 5-degree M2 servo movement, with 3-degree fine lift jogging.
- Measured mechanical limits for both servos.
- Electromagnet pick-and-place with configurable settle times.
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
| Object sensor | `D5` | Optional IR/object sensor input. Disabled by default with `OBJECT_SENSOR_ENABLED`; set it to `true` when fitted, and adjust `OBJECT_SENSOR_ACTIVE_LOW`. |
| LCD RS/E/D4-D7 | `D2`, `D3`, `D4`, `D7`, `D8`, `D13` | Default parallel 16x2 LCD wiring. Change `LCD_*_PIN` or set `LCD_USE_I2C` to `1` for an I2C backpack. |
| LCD I2C SDA/SCL | `A4`/`A5` | Used only when `LCD_USE_I2C` is `1`. Set `LCD_I2C_ADDRESS` for the backpack. |

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
| `pick_place.cpp/.h` | Lift servo, magnet, object sensor, fixed pick/place, and color-to-color pick/place. |
| `rgb_calibration.cpp/.h` | White-patch manual/automatic RGB calibration, classification, sweep, and color search. |
| `assignment_flow.cpp/.h` | Calibration choice, pickup/drop color prompts, and the assignment pick/place cycle. |
| `lcd_display.cpp/.h` | Parallel/I2C 16x2 LCD output. |
| `detection.cpp/.h` | Continuous color detection and detection-mode commands. |
| `command_input.cpp/.h` | Serial and queued-command input. |
| `wifi_control.cpp/.h` | Wi-Fi connection, HTTP server, web UI, and `/status` JSON. |
| `menu.cpp/.h`, `modes.h` | Mode state and menu/calibration command handling. |
| `arduino_secrets.example.h` | Template for Wi-Fi credentials. |
| `arduino_secrets.h` | Local Wi-Fi credentials; git-ignored. |

## Build and upload

### Arduino IDE

1. Install the **Arduino UNO R4 Boards** core.
2. Install the **Servo** and **LiquidCrystal** libraries from Library Manager.
   The LiquidCrystal library is only needed for the default parallel LCD;
   the I2C backpack mode uses the built-in `Wire` library.
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
arduino-cli lib install Servo LiquidCrystal

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
| `man` | Manual white-patch calibration: adjust the RGB pots, then type `1`. |
| `auto` | Autocalibrate the white patch by dimming the brighter LED channels. |
| `none` | Use the current/default RGB calibration, sweep the base, and start the pick/drop flow. |
| `s` | Legacy: load defaults and enter the four-strip calibration mode. |
| `o` | Legacy: load the compile-time defaults and start detection. |
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

### White-patch RGB calibration and assignment flow

At the menu, choose `man`, `auto`, or `none`.

Important: auto calibration does **not** know where red, green, or blue are
before it starts. It only balances the white patch. The color scan happens
afterward; the arm classifies each angle from the reflected R/G/B readings.
Place the LDR over the white patch at `WHITE_PATCH_ANGLE` (default `85`) before
pressing `auto`.

1. The lift rises to `24` degrees.
2. `man` moves M2 to `WHITE_PATCH_ANGLE` (default `85`) and repeatedly shows
   raw R/G/B readings plus the maximum. Turn the two brighter-channel
   potentiometers until the three readings match, then type `1`.
3. `auto` moves M2 to the white patch, measures all three channels at full
   LED, and dims the brighter channels until their LDR readings match the
   dimmest channel. This step does not store color positions.
4. `none` keeps the current values (or the compile-time defaults).
5. The firmware sweeps M2 across the base in 5-degree steps and displays
   R/G/B plus the detected color on the LCD and serial output. The arm is
   scanning here; it is not calibrating anymore. Wait for
   `Base sweep done` and the pickup-color prompt. A color is classified by
   which channel reflects most strongly: its LDR reading is the lowest. A red
   object reflects red best, so its red reading is lowest; green and blue work
   the same way. Roughly equal readings are white; very dark readings are
   `NONE`.
6. Enter the pickup color and then the drop color (`w`, `g`, `r`, `b`).
   On the web page, use the **Pickup W/G/R/B** buttons first, then the
   **Drop W/G/R/B** buttons.
7. The arm finds the pickup color and lowers. With the object sensor enabled,
   lowering stops when the sensor triggers; while it is disabled, the arm goes
   to the down limit. It then turns the magnet on for `MAGNET_PICK_MS`
   (5000 ms), and retracts.
8. It then searches for the drop color, lowers, releases the magnet, retracts,
   and returns to the calibration prompt.

The object sensor is **disabled by default** (`OBJECT_SENSOR_ENABLED = false`)
because it is not required for the current setup. The lowering/checking code
remains in place; set `OBJECT_SENSOR_ENABLED = true` when the module is fitted.
It is on `D5` and active low by default; if it never triggers, the arm
continues to the down limit so the cycle still completes; set
`OBJECT_SENSOR_ACTIVE_LOW` to match the module.

### Legacy four-strip calibration mode

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

## Assignment flow commands

The `man`, `auto`, and `none` menu commands run the white-patch calibration and
pickup/drop prompt flow described above. This is separate from the legacy
`pickXY` token below, which pre-searches the destination before pickup.

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
=== WEB CONTROL READY ===
WiFi IP: 192.168.x.x
URL: http://192.168.x.x
========================
```

The same IP appears on the LCD as `WEB UI` / `192.168.x.x` when the board
connects at boot. Open the URL in a browser.

The page provides:

- **White-patch RGB / assignment** controls:
  - **Manual RGB** (`man`) starts pot adjustment;
  - **Finish manual** (`1`) saves the final reading;
  - **Auto RGB** (`auto`) balances the channels automatically;
  - **Reuse RGB / start** (`none`) skips calibration and starts the flow;
  - **Pickup W/G/R/B** and **Drop W/G/R/B** answer the two color prompts;
  - **Cancel prompt** (`c`) exits the color prompt.
- legacy four-strip calibration/detection shortcuts;
- M2 jog, angle jumps, limits, and scan;
- lift up/down, fine jog, magnet toggle, and fixed pick/place;
- legacy color-to-color pre-search pick/place dropdowns;
- live status: mode, current assignment prompt, M2 angle, lift angle,
  calibrated strip count, LED color, candidate color, LDR reading, and magnet
  state.

For the RGB assignment flow, use the buttons in order: start calibration,
finish manual (if manual), wait for the 5-degree color sweep to finish, then
send the pickup color followed by the drop color. The status **Prompt** line
shows phases such as `AUTO CAL`, `BASE SWEEP`, `PICK-UP COLOR`, `DROP COLOR`,
`FIND PICKUP`, and `FIND DROP`. The firmware prints the RGB readings and detected color on the LCD and
serial monitor while it sweeps. The legacy **Legacy four-strip calibration**
buttons still use the old strip table; use **Menu** first when switching
workflows.

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

Runtime calibration updates the live `colors[]` table in RAM only. The
white-patch RGB levels and references are also RAM-only. After a reset, the
compile-time defaults are loaded again. To make a calibration permanent,
update `defaultColors` in `color_data.cpp` and re-upload.

`o` from the menu restores these defaults and starts detection.

## Detection details

- Calibration averages 50 LDR readings per light after a 200 ms settle.
- Detection averages 20 readings per scan.
- A detected color must match for 3 consecutive scans before the LED changes.
- The RGB white/color classifier normalizes each reading to its white-patch
  reference and uses a 3% normalized spread limit, with a dark cutoff capped
  at 950 ADC counts.
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
| Calibration disappears after reset | Expected: both the four-strip table and white-patch RGB calibration are RAM-only. Update `defaultColors` or `RGB_DEFAULT_WHITE` for a permanent default. |
| LCD stays blank | Check the `LCD_USE_I2C` setting, the `LCD_*_PIN` values, and the I2C address. Parallel mode needs the `LiquidCrystal` library. |
| Object sensor has no effect | It is disabled by default. Set `OBJECT_SENSOR_ENABLED = true`; then check `OBJECT_SENSOR_PIN`, `OBJECT_SENSOR_ACTIVE_LOW`, and the module output polarity. |
| Color pick/place aborts | The source or destination color was not found, or both resolved to the same 5-degree M2 position. Check `x`, `v`, and the calibration table. |
| Auto calibration just moves 5 degrees and flashes light | That is the post-calibration color sweep, not the white-balance step. Wait for `Base sweep done`, then send pickup and drop colors. Make sure the LDR is over the white patch at `85` before pressing `auto`. |
| Every sweep angle says `WHITE` | Auto calibrated against the wrong/dark surface. Put the LDR exactly over the white patch at `85`, keep all three pots at minimum resistance, and rerun `auto`. Check the printed `White average`; a very high target also prints the dark-white warning. |

## Configuration quick reference

Most behavior is controlled from `config.h`:

- pin assignments, including LCD and object sensor pins;
- `OBJECT_SENSOR_ENABLED` (currently `false`);
- `LCD_USE_I2C` and the I2C address;
- white-patch angle and RGB calibration timing/tolerance;
- M2 travel and lift travel limits;
- servo step size and delay;
- color sweep samples and settle times;
- detection sample counts and stability threshold;
- magnet timings;
- command buffer size and idle timeout.

Color values live in `color_data.cpp`. Four-strip calibration and detection
logic live in `sensing.cpp`, `calibration.cpp`, and `detection.cpp`.
White-patch RGB calibration and color search live in `rgb_calibration.cpp`.
The assignment flow lives in `assignment_flow.cpp`.
