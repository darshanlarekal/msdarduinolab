# Arduino Color Sorter / Pick-and-Place

Firmware for an Arduino UNO R4 WiFi that:

- calibrates a white-patch RGB reference;
- sweeps a servo (M2) and classifies each angle by reflected R/G/B;
- drives a second servo (A0) to raise and lower a magnet;
- picks an object from one color and places it on another;
- exposes the assignment flow through the serial monitor and a browser UI.

Color positions are not hardcoded. The arm sweeps the base, and the RGB
classifier decides what is under the LDR at each angle.

## Features

- White-patch RGB calibration: manual pot adjustment or automatic LED balancing.
- Base sweep with per-angle RGB and color output.
- Pickup/drop color assignment flow.
- Five-second active-low electromagnet pickup on `D7`.
- Optional object-sensor-assisted lowering (disabled in `config.h` until fitted).
- Optional 16x2 LCD, currently disabled with `LCD_ENABLED = 0`.
- Wi-Fi web UI and JSON status endpoint.
- Serial control fallback when Wi-Fi is unavailable.
- Servos are not attached or driven at reset, so flashing does not make the arm
  jump to a default position.

## Hardware

| Function | Arduino pin | Notes |
|---|---|---|
| LDR | `A2` | Analog input. The LDR is mounted on the M2 arm. |
| RGB LED - red | `D9` | PWM output. |
| RGB LED - green | `D10` | PWM output. |
| RGB LED - blue | `D11` | PWM output. |
| M2 servo signal | `D6` | Board silk marks the M2 header as `6 / + / -`; DIP switch 8 connects D6 to M2. |
| Lift servo signal | `A0` | Used as a digital servo output. Power the servo from 5 V and GND, not from A0. |
| Electromagnet driver | `D7` | Active low. Use a MOSFET or relay driver; never drive the coil directly from a pin. |
| Object sensor | `D5` | Optional input, disabled by default. Set `OBJECT_SENSOR_ENABLED` and adjust polarity when fitted. |
| LCD | disabled | The LCD module still compiles as no-ops. **D7 is the magnet**, so a parallel LCD must not use `LCD_D5_PIN = 7`; use I2C or move a pin before enabling it. |

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
angle raises the arm. The firmware clamps automatic moves to these limits.

## Repository layout

| File | Purpose |
|---|---|
| `arduino_maintain.ino` | Arduino `setup()` and main `loop()`. |
| `config.h` | Pins, limits, timings, calibration sizes, and command settings. |
| `color_data.cpp/.h` | Compile-time color table, LED helpers, and strip-name mapping. |
| `servo_arm.cpp/.h` | M2 servo movement. |
| `pick_place.cpp/.h` | Lift servo, magnet, object sensor, and lowering. |
| `rgb_calibration.cpp/.h` | White-patch calibration, RGB classification, base sweep, and color search. |
| `assignment_flow.cpp/.h` | Calibration choice, pickup/drop prompts, and the pick/place cycle. |
| `lcd_display.cpp/.h` | Parallel/I2C LCD output; disabled by default. |
| `command_input.cpp/.h` | Serial and queued-command input. |
| `wifi_control.cpp/.h` | Wi-Fi connection, HTTP server, web UI, and `/status` JSON. |
| `menu.cpp/.h`, `modes.h` | Menu state and calibration/assignment entry points. |
| `arduino_secrets.example.h` | Template for Wi-Fi credentials. |
| `arduino_secrets.h` | Local Wi-Fi credentials; git-ignored. |

## Build and upload

### Arduino IDE

1. Install the **Arduino UNO R4 Boards** core.
2. Install the **Servo** library. `LiquidCrystal` is only needed if the LCD is
   enabled in parallel mode.
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

## Boot behavior

`setup()` initializes the pins, turns the RGB LED off, initializes the
white-patch defaults, and starts serial and Wi-Fi. It does **not** attach or
write either servo.

The servos attach on the first movement. This avoids the reset jump caused by
`Servo::attach()`. The magnet is initialized off at boot.

## Assignment flow

The firmware starts in `MENU` mode and prints:

| Command | Action |
|---|---|
| `man` | Manual white-patch calibration: adjust the RGB pots, then type `1`. |
| `auto` | Automatic white balancing by dimming the brighter LED channels. |
| `none` | Use the current/default RGB calibration and start the assignment flow. |

All three paths then:

1. Raise the lift to `24` degrees.
2. Position the arm over the white patch at `WHITE_PATCH_ANGLE` for `man` or
   `auto`.
3. Sweep M2 from `0` to `120` in 5-degree steps and classify each angle.
4. Prompt for the pickup color and then the drop color.
5. Find the pickup color, lower the lift, turn the magnet on for 5000 ms, and
   raise.
6. Find the drop color, lower, turn the magnet off, raise, and return to the
   menu.

### Manual calibration

`man` moves M2 to the white patch and repeatedly prints raw R/G/B and the
maximum reading. Adjust the two brighter-channel potentiometers until the three
readings match, then type `1`.

### Automatic calibration

`auto` moves M2 to the white patch, measures all three channels at full LED,
and dims the brighter channels until their readings match the dimmest channel.
It balances the white reference only; it does not locate the colored objects.
Keep the LDR over the white patch at `85` degrees and all three pots at minimum
resistance.

### Color prompts

The pickup and drop prompts accept `w`, `g`, `r`, or `b`. Typing `c` exits the
assignment flow. If the pickup and drop colors are the same, or if a color is
not found during the sweep, the prompt repeats.

### Classification

Each channel is normalized to its white-patch reference:

- a normalized spread of at most 3% is classified as white;
- otherwise the color is chosen from the raw channel minimum;
- a very dark reading is classified as `NONE`;
- the arm steps in 5-degree increments with an 80 ms step delay.

## Web control

The board connects as a Wi-Fi station. When it gets an IP address, serial
prints:

```text
=== WEB CONTROL READY ===
WiFi IP: 192.168.x.x
URL: http://192.168.x.x
========================
```

Open the URL in a browser. The page supports:

- **Manual RGB** (`man`);
- **Finish manual** (`1`);
- **Auto RGB** (`auto`);
- **Reuse RGB / start** (`none`);
- **Pickup W/G/R/B** and **Drop W/G/R/B**;
- **Cancel prompt** (`c`);
- a generic command input for the same serial commands;
- live status: mode, assignment prompt, M2 angle, lift angle, and magnet state.

Use the buttons in order: start calibration, finish manual if needed, wait for
the base sweep, then send the pickup color followed by the drop color.

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
  "prompt": "READY",
  "m2": 90,
  "lift": 24,
  "magnet": false
}
```

Only one remote command is queued at a time. A second command while the first
is still pending returns HTTP `409 Conflict`.

If Wi-Fi is unavailable, serial control still works.

## Calibration data

`color_data.cpp` contains the compile-time default table:

```cpp
{"WHITE", 532.54, 547.10, 490.02, 442.82},
{"GREEN", 835.46, 834.04, 756.88, 725.46},
{"RED",   754.56, 806.42, 784.76, 713.64},
{"BLUE",  822.54, 821.76, 809.80, 775.26}
```

The `WHITE` row supplies the default white-patch R/G/B reference. The color
names are used when printing sweep results.

White-patch calibration values are RAM-only. After a reset, `setup()` restores
the defaults above. To make a calibration permanent, update `defaultColors`
and re-upload.

## Troubleshooting

| Symptom | Check |
|---|---|
| Servos do not move immediately after reset | Expected. They attach on the first movement. |
| Lift moves the wrong way | Confirm `24` is up and `54` is down in `config.h`; swap `LIFT_UP_ANGLE` and `LIFT_DOWN_ANGLE` if the mechanics are reversed. |
| Every sweep angle says `WHITE` | Auto calibrated against the wrong/dark surface. Put the LDR exactly over the white patch at `85`, keep all three pots at minimum resistance, and rerun `auto`. |
| Auto calibration just moves 5 degrees and flashes light | That is the post-calibration base sweep, not the white-balance step. Wait for `Base sweep done`, then send pickup and drop colors. |
| Color is not found during the sweep | Check that the strip is under the LDR, confirm the white patch was sampled correctly, and recalibrate. |
| Web page does not load | Check `SECRET_SSID`/`SECRET_PASS`, use a 2.4 GHz network, and watch serial for the IP address. Serial control works without Wi-Fi. |
| Web command returns `409 Conflict` | Another command is already waiting in the queue. Wait for it to be consumed. |
| Servos twitch or the board resets during motion | Use a stronger separate 5 V servo supply and a common ground. |
| Magnet does not actuate | Check the MOSFET/relay, external coil supply, flyback diode, and `MAGNET_ACTIVE_HIGH`. |
| Calibration disappears after reset | Expected: white-patch RGB values are RAM-only. Update `defaultColors` for a permanent default. |
| LCD stays blank | Expected while `LCD_ENABLED = 0`. To enable it, set it to `1`, choose I2C or move the parallel LCD pins away from D7, and install `LiquidCrystal` for parallel mode. |
| Object sensor has no effect | It is disabled by default. Enable it and check `OBJECT_SENSOR_PIN`, `OBJECT_SENSOR_ACTIVE_LOW`, and module polarity. |

## Configuration quick reference

Most behavior is controlled from `config.h`:

- pin assignments, including LCD and object sensor pins;
- `OBJECT_SENSOR_ENABLED` (currently `false`);
- `LCD_ENABLED` (currently `0`), `LCD_USE_I2C`, and the I2C address;
- white-patch angle and RGB calibration timing/tolerance;
- M2 and lift travel limits;
- servo step size and delay;
- LDR sample count;
- magnet timings;
- command buffer size and idle timeout.

Color defaults live in `color_data.cpp`. White-patch calibration, classification,
sweep, and color search live in `rgb_calibration.cpp`. The assignment flow
lives in `assignment_flow.cpp`.
