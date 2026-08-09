# Tamagotchi Arcade

A pixel-art tamagotchi and arcade game for the ESP32 TTGO/Tenstar T-Display
with a 240×135 screen in portrait mode. Your pet is a cat with a choosable
fur color, pattern, hat, accessory, and background. Play five arcade
minigames to earn coins and XP, then spend coins in the shop on cosmetics.

## Project layout

- `tamagotchi_arcade/` — the Arduino firmware.
- `art_reference/` — editable style sheets and simulator preview images.
- `tools/` — asset generators and exact software simulators.
- `docs/HARDWARE.md` — board, wiring, and TFT_eSPI build configuration.
- `docs/ART_PIPELINE.md` — how the graphics go from source image to ESP32.
- `docs/issues/` — long-form specs for known graphics debt; tracked as
  [GitHub Issues](../../issues).
- `CLAUDE.md` — working instructions for Claude or another coding assistant.

`cat_sprites.h` and `premium_assets.h` are generated files and should not be
edited by hand.

## Setup

### 1. Hardware and TFT_eSPI

Read `docs/HARDWARE.md` first. The display driver configuration is passed as
compiler flags at build time (see below), not via a `User_Setup.h` edit, so
no changes to a global TFT_eSPI install are required.

### 2. Python tooling

The asset generators and simulators need Pillow:

```sh
pip install -r tools/requirements.txt
```

### 3. Common commands

Run from the project root:

```sh
python3 tools/generate_cat_sprites.py
python3 tools/generate_premium_assets.py
python3 tools/render_hat_simulator.py
python3 tools/render_accessory_simulator.py

arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-property "compiler.cpp.extra_flags=-DUSER_SETUP_LOADED=1 -DST7789_DRIVER=1 -DTFT_SDA_READ=1 -DTFT_WIDTH=135 -DTFT_HEIGHT=240 -DCGRAM_OFFSET=1 -DTFT_MOSI=19 -DTFT_SCLK=18 -DTFT_CS=5 -DTFT_DC=16 -DTFT_RST=23 -DTFT_BL=4 -DTFT_BACKLIGHT_ON=HIGH -DSPI_FREQUENCY=40000000 -DSPI_READ_FREQUENCY=6000000 -DLOAD_GLCD=1" \
  tamagotchi_arcade
```

To flash a connected device, find its port first — don't assume the name is
stable across sessions:

```sh
arduino-cli board list
arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32 \
  --board-options UploadSpeed=115200 tamagotchi_arcade
```

## Controls

- **L** (GPIO0) — Next (press) / Options menu (press, Home screen only)
- **R** (GPIO35) — Select / Confirm (press)

## Status

The firmware builds and runs. Hats have their own occlusion rules, and
accessories currently use a provisional back/front composite. The accessory
system doesn't yet look natural on the cat. See the
[open issues](../../issues) — especially the accessory-layering work — before
further cosmetic polish.
