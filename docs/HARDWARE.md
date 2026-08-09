# Hardware

## Board

LilyGO TTGO T-Display (ESP32, 1.14" ST7789 TFT, 135×240 physical panel).
The firmware runs the panel in portrait mode as a 135×240 canvas
(`tamagotchi_arcade/gfx.h`).

Any TTGO T-Display clone with the same ST7789 panel and pinout should work
unmodified. Other ESP32 boards will need different pin numbers (see below)
and possibly a different display driver.

## Buttons

| Button | GPIO | Notes |
|---|---|---|
| L (Next / Options) | 0 | Has an internal pull-up; safe as a plain digital input. |
| R (Select / Confirm) | 35 | Input-only pin with **no internal pull-up or pull-down**. Configured as plain `INPUT` in `input.cpp`; the board's onboard button wiring supplies the pull-up externally. If you wire your own button to GPIO35, add an external pull-up resistor. |

Pin numbers are defined in `tamagotchi_arcade/input.h`.

## Display wiring (built into the TTGO T-Display board)

| Signal | GPIO |
|---|---|
| MOSI | 19 |
| SCLK | 18 |
| CS | 5 |
| DC | 16 |
| RST | 23 |
| Backlight | 4 |

The backlight is wired but not currently driven by firmware (no brightness
control or auto screen-off yet).

## TFT_eSPI configuration

This project uses the [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) library
but does not ship its own `User_Setup.h`. Instead, all display configuration
is passed as compiler flags at build time via `--build-property`, so a global
TFT_eSPI install does not need to be edited:

```
compiler.cpp.extra_flags=-DUSER_SETUP_LOADED=1 -DST7789_DRIVER=1 -DTFT_SDA_READ=1 -DTFT_WIDTH=135 -DTFT_HEIGHT=240 -DCGRAM_OFFSET=1 -DTFT_MOSI=19 -DTFT_SCLK=18 -DTFT_CS=5 -DTFT_DC=16 -DTFT_RST=23 -DTFT_BL=4 -DTFT_BACKLIGHT_ON=HIGH -DSPI_FREQUENCY=40000000 -DSPI_READ_FREQUENCY=6000000 -DLOAD_GLCD=1
```

See the compile command in `README.md` for how this is passed to
`arduino-cli`. Without these flags, TFT_eSPI falls back to whatever
`User_Setup.h` happens to be active in your local library install, which will
not match this panel.

`SCREEN_ROTATION` in `tamagotchi_arcade/gfx.h` controls portrait orientation;
flip it if your unit displays upside down.

## Bring-up sketches

Two minimal sketches in `examples/` are useful when bringing up a new board
or verifying wiring before flashing the full firmware:

- `examples/ttgo_board_test/` — confirms flashing, serial output, and basic
  chip info (no display library required).
- `examples/ttgo_display_test/` — exercises the TFT_eSPI display config above
  in isolation.

## Finding the serial port

Don't assume the port name is stable across sessions or machines. Find it
each time:

```sh
arduino-cli board list
```
