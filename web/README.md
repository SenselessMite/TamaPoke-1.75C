# TamaPoke web installer

A one-click page that flashes the firmware and loads the sprites from the browser
(Chrome/Edge), with no Arduino or drivers. It uses
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) to flash and **Web
Serial** to push the sprites with the firmware's `PUT` protocol (the same one as
`tools/send_sd.py`): to the microSD on the ESP32-S3-Touch-AMOLED-1.75, to the
internal flash's FAT partition on the 1.75C (this fork's default board).

## Contents

- `index.html` — the page (flashing + sprite loader).
- `manifest.json` — ESP Web Tools config (points at the firmware).
- `firmware/tamapoke.bin` — combined firmware, flashable at `0x0`, for **one**
  board (the one it was built for). In this fork it is still the upstream v1.18
  build for the 1.75 until you run `tools/build_web.sh`.
- `sprites.pak` — all the sprites in one bundle (TPAK), so the page sends them in
  one click. **Generated** by `tools/pack_bundle.py`; each file inside is
  zlib-compressed (`TPKZ`, see `tools/tpkz.py`), ~1.2 MB in total.

## Regenerate

After changing the firmware or the sprites:

```bash
bash tools/build_web.sh        # 1.75C: recompiles -> firmware/tamapoke.bin AND rebuilds sprites.pak
bash tools/build_web.sh 175    # same, for the original 1.75
```

## Test locally

Web Serial and ESP Web Tools need a **secure context**: `https://` or
`http://localhost`. To test:

```bash
cd web && python3 -m http.server 8000
# open http://localhost:8000 in Chrome/Edge
```

## End-user flow

1. **Install TamaPoke** → flashes the firmware (pick the USB port; tick "Erase
   device" for a fresh board).
2. **Connect board** + **Load sprites** → downloads `sprites.pak` and copies it
   over USB (progress bar, under a minute now that it's compressed) to the microSD
   (1.75) or the internal flash (1.75C). Close the step-1 install tab — or the
   Arduino IDE's Serial Monitor — first: only one program can use the port.
   Flashed from the Arduino IDE? Skip step 1 and just do this one.
   On the 1.75C a full **Erase** also wipes the sprites: load them again.
3. Restart (PWR button) → choose your starter and play.

A hidden "pick them manually" option lets advanced users send their own `.bin`.

## Hosting the sprites

Compressed, `sprites.pak` is only ~1.2 MB, so this fork simply **commits it**
and serves it from Pages next to the page. Alternatives if you prefer:

- **Release asset** — attach `sprites.pak` to a GitHub Release and change the
  `fetch('sprites.pak')` URL in `index.html` to the release URL (keeps the repo
  small; watch out for CORS on the asset host).

All sprites are from PMD SpriteCollab, CC BY-NC (non-commercial sharing with
attribution is allowed); see [`../CREDITS.md`](../CREDITS.md).

## Deploy (GitHub Pages)

1. Repo settings → Pages → serve from `main`, folder `/web` (or move `web/` to
   `docs/`). Pages gives HTTPS automatically.
2. URL ends up at `https://<user>.github.io/<repo>/`.

> **Pages on private repos** needs GitHub Pro/Team. If you make the repo
> **public** to use Pages for free, decide about the sprites first (see above and
> CREDITS).

## Limitations

- Desktop **Chrome/Edge** only (Web Serial isn't in Firefox/Safari).
