# Rebuilding the ESP32-P4 framework libraries

**Why:** [espressif/esp-hosted-mcu#243](https://github.com/espressif/esp-hosted-mcu/issues/243).
One failed DMA-capable RX buffer allocation permanently disables host RX on a P4, and the retry
added in 2.12.12 never runs. Four of our eight boards reach WiFi this way and all four are
unusable. Full analysis in `LESSONS.md`; tracking issue is #61.

**What we are changing — two settings, and nothing else:**

```
CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y
CONFIG_CACHE_L2_CACHE_LINE_64B=y        # required alongside it, see esp-hosted-mcu#219
```

Upstream measured the same device and workload: **4 stalls in 13 minutes became 2 h 52 m with
zero failures.**

---

## THE THING MOST LIKELY TO GO WRONG, READ IT FIRST

**We do NOT use the `esp32p4` libraries. We use `esp32p4_es`.**

pioarduino ships two P4 variants and the board definition picks one:

```
.platformio/platforms/espressif32/boards/esp32-p4-evboard.json
    "chip_variant": "esp32p4_es"
```

| Variant | Silicon | Our boards |
|---|---|---|
| `esp32p4` | `REV_MIN_301` — rev 3 and later | **no** |
| `esp32p4_es` | `SELECTS_REV_LESS_V3=y`, `REV_MIN_1` — engineering sample / pre-rev3 | **yes** |

Building plain `esp32p4` and installing it would produce libraries for silicon we do not have.
CLAUDE.md already records that *reading* the wrong one of these two wasted most of a session;
*building* the wrong one would be worse, because it would link and boot and then misbehave in
ways that look like new bugs.

**Check `chip_variant` in the board JSON before you start, and check it again before you copy
anything into place.**

---

## Step 1 — install WSL

Needs admin and a reboot. From an elevated PowerShell:

```powershell
wsl --install -d Ubuntu-24.04
```

Name the LTS explicitly. The bare `Ubuntu` alias tracks whatever Canonical currently defaults to
and drifts over time; ESP-IDF is tested against the LTS releases. Confirm afterwards with
`wsl -l -v` — it must say **version 2**, not 1.

Reboot, let Ubuntu finish first-run setup, then:

```bash
sudo apt update
sudo apt install -y git wget curl libssl-dev libncurses-dev flex bison \
                    gperf python3 python3-pip python3-venv cmake ninja-build \
                    ccache libffi-dev dfu-util libusb-1.0-0 jq
```

**`jq` is not optional and is not in Espressif's own prerequisites list.** `build.sh` parses
`configs/builds.json` with it to select the target, so a machine without it fails at the very first
step with nothing useful in the output. Found on a fresh Ubuntu 24.04 install.

**Work inside the WSL filesystem, not `/mnt/c/`.** WSL2's cross-filesystem I/O is dramatically
slower and this build is I/O-bound. Clone to `~/`, and copy results across at the end.

Budget **~30 GB** for IDF, the toolchains and build output.

## Step 2 — get lib-builder, matched to our framework

We are pinned to **pioarduino 55.03.311 = arduino-esp32 3.3.11 = ESP-IDF 5.5.5**. The rebuild must
match, or we are changing far more than two settings.

```bash
cd ~
git clone https://github.com/espressif/esp32-arduino-lib-builder.git
cd esp32-arduino-lib-builder
```

**LIB-BUILDER'S BRANCHES ARE NAMED AFTER ESP-IDF, NOT ARDUINO.** This is a trap and it very nearly
went into this document as an instruction. `release/v3.3` sounds like it matches arduino-esp32
3.3.x — it is in fact from **December 2021** and pins `IDF_BRANCH="release/v3.3"`, i.e. ESP-IDF 3.3,
the arduino-esp32 1.x era. Building it would produce libraries from a different decade.

We need **ESP-IDF 5.5.5**, so:

```bash
git checkout master      # targets IDF_BRANCH="release/v5.5"
grep -n 'IDF_BRANCH=' tools/config.sh | head -2
```

Must print `IDF_BRANCH="release/v5.5"`. There is also a `idf-release_v5.5` tag if a pinned point is
preferred to a moving branch.

`master` tracks IDF 5.5's branch rather than the exact 5.5.5 point release, so it may resolve to a
slightly later patch. **That is what the sdkconfig diff in Step 5 is for** — it will show any
version drift as a difference, and you decide whether to accept it.

## Step 3 — set the two options

### `-t` TAKES THE CHIP VARIANT, NOT THE TARGET. Use `esp32p4_es`.

`configs/builds.json` has two entries under one target:

```json
{"chip_variant": "esp32p4_es", "target": "esp32p4", ...}
{                              "target": "esp32p4", ...}
```

and `build.sh` selects on the **variant**, defaulting it to the target name when absent:

```bash
export CHIP_VARIANT=$(... '.chip_variant // "'$target'"' ...)
for item in "${TARGET[@]}"; do
    if [ "$item" = "$CHIP_VARIANT" ]; then
```

So **`-t esp32p4` matches only the entry WITHOUT a `chip_variant`** — the rev3+ build — and skips
`esp32p4_es` silently. It would complete successfully and produce libraries for silicon we do not
have.

Verified at the source: `diff configs/defconfig.esp32p4 configs/defconfig.esp32p4_es` shows exactly
the pre-rev3 set our boards need, matching the shipped `esp32p4_es/sdkconfig`:

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_1=y
CONFIG_ESP32P4_REV_MAX_FULL=199
```

Building one variant instead of two is also faster.

```bash
./build.sh -t esp32p4_es -b menuconfig qio 80m_200m
```

In menuconfig:

- `Component config → ESP Hosted → ` enable **Prefer SPIRAM for mempool**
  (`CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM`)
- `Component config → Hardware Settings → Cache config → L2 cache line size → **64 bytes**`
  (`CONFIG_CACHE_L2_CACHE_LINE_64B`)

`qio 80m_200m` is not decoration — it is the flash/PSRAM configuration Espressif builds the P4
libraries with, taken from `configs/builds.json`. Omitting it produces a differently-configured
build.

## Step 4 — build

```bash
./build.sh -t esp32p4_es qio 80m_200m
```

One chip rather than all ten, so think **30–90 minutes**, not the "many hours" the README quotes
for a full run.

## Step 5 — VERIFY BEFORE INSTALLING. This is the step that makes it safe.

We kept a copy of the current configuration:

```
reference/framework-baseline/sdkconfig.esp32p4_es.55.03.311
```

1,968 `CONFIG_` lines, exactly as shipped. Diff the new one against it:

```bash
diff reference/framework-baseline/sdkconfig.esp32p4_es.55.03.311 \
     <path-to-new>/sdkconfig | grep -E "^[<>]" | sort
```

**Expected:** the two options we changed, plus whatever they imply — a `CACHE_L2_CACHE_LINE_SIZE`
of 64 instead of 128, and possibly a couple of derived alignment values.

**If dozens of unrelated lines differ, STOP.** That means the branch, the IDF version or the
config arguments do not match what we run today, and installing it would change many things at
once with no way to attribute a regression.

Also confirm you built the right variant:

```bash
grep -E "SELECTS_REV_LESS_V3|REV_MIN_" <path-to-new>/sdkconfig
```

Must show `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_1=y`.

## Step 6 — install, reversibly

```bash
# From Windows, with PlatformIO closed.
cd ~/.platformio/packages/framework-arduinoespressif32-libs
mv esp32p4_es esp32p4_es.stock.55.03.311     # keep it, do not delete it
cp -r /path/from/wsl/esp32p4_es .
```

**Keep the stock folder.** Reverting is then a rename, which matters because this is the only
change in the project that cannot be undone with git.

Then clear the compiler cache, which `pio run -t clean` does **not** touch:

```bash
rm -rf .pio/build_cache
pio run -e WS_P4_TOUCH_LCD_4B
```

## Step 7 — prove it worked

The HA dashboard reproduces #243 in **seconds** now — 18 REST fetches plus a websocket at boot is
exactly the bursty inbound TCP in that issue's title. That is the one gift this bug has given us:
the test is minutes, not an overnight soak.

**PASS:** the board renders all its cards, stays on WiFi, and serial shows no
`H_SDIO_DRV: RX buffer alloc failed` and no `rpc_core: Response not received`.

**Also check the boot log's internal heap line.** Upstream saw internal-heap low rise from 17–58 KB
to 145 KB, because the hosted mempool moved to PSRAM. `SystemCore::printIdentity()` already prints
it. If that number does not move, the setting did not take.

---

## What this costs the project, honestly

**It makes the repo harder for anyone else to build for P4.** A stock pioarduino install produces
the stalling firmware. Anyone wanting working P4 boards has to repeat this, or accept the bug. The
S3 boards are entirely unaffected and need nothing.

That is a real cost and it is worth stating in the README when this lands.

**The flip side, and it is not small:** a confirmed independent reproduction of #243 plus a
verified workaround on different hardware is genuinely useful to the upstream issue. Worth posting
back to esp-hosted-mcu#243 when it is done.

## The alternative, and why it is not the first choice

`framework = arduino, espidf` — Arduino as an ESP-IDF component — compiles IDF from source with a
project-level `sdkconfig.defaults`, so both options are simply ours to set. No WSL, no lib-builder.

It is a reasonable stepping stone toward a full ESP-IDF port and it is where the owner expects to
end up eventually. It is not the first choice **today** because it changes how every environment
builds, permanently: first builds go from ~60 s to 10–20 minutes, and the 26 `symlink://`
libraries in `platformio.ini` may need rework — CLAUDE.md records four alternative arrangements
that were tested and failed, so that area is fragile.

**Unverified:** whether PlatformIO honours `lib_deps` unchanged in mixed mode. Check that before
choosing this route; if it does, the cost is much lower than assumed.

The lib-builder route changes one folder in a package directory and leaves the project alone.
