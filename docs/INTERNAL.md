# TrackClick — internal developer notes

> **Internal / confidential.** Not for the public README, website, or store
> listings. This file collects the things a developer needs to know that we
> either don't want to advertise or that are too in-the-weeds for users. Keep
> sensitive specifics here, not in user-facing docs.

Current version: **0.9.4** (pre-1.0 — see "Release gaps" below).
macOS bundle id: `com.trackclick.TrackClick`.

---

## 1. Why this app looks like malware (and our answer to that)

TrackClick legitimately does several things that overlap with the behaviour of
keyloggers / RATs. Anyone auditing the binary or an AV heuristic will flag these.
We should be ready to explain each, and we should **expect AV false positives**
(especially SmartScreen / Defender on unsigned Windows builds).

| Capability | Where | Why it's needed |
|---|---|---|
| Creates virtual input devices (`/dev/uinput`) | `clickinjector.cpp` | Inject clicks/scroll/modifiers on Wayland where XTest is blocked |
| Reads raw kernel input (`/dev/input/event*`) | `clickinjector.cpp` | Track cursor across windows on Wayland (see §2) |
| System-wide input injection (SendInput / CGEvent / XTest) | `clickinjector.cpp` | Core function — it's a virtual mouse |
| Installs a **root** udev rule via `pkexec` | `mainwindow.cpp` ~439 | Grant evdev read access without re-login |
| Runs `pkexec apt/dnf/zypper/pacman install onboard` | `settingsdialog.cpp` | Auto-install an on-screen keyboard |
| Writes autostart entries (registry Run key / .desktop / login item) | `mainwindow.cpp` ~1487 | "Launch on startup" option |

None of this is hidden, all of it is user-initiated, and we deliberately do
**not** capture keystrokes. But treat the framing carefully in public materials —
lead with accessibility, not "injects input system-wide."

## 2. The Wayland problem (the reason half this code exists)

On Wayland, `XQueryPointer` / `QCursor::pos()` **freezes the moment the cursor
leaves our own window**. A dwell-clicker that can't tell whether the cursor is
moving over *other* apps is useless. Mitigations, in fallback order:

1. **evdev** — read motion straight from `/dev/input/event*`. Requires read
   access to root-owned nodes → we ship `installer/71-trackclick-input.rules`
   which tags pointer devices with `uaccess` (session ACL, no group change, no
   re-login). Rule matches **pointer-class devices only — keyboards are
   deliberately excluded.**
2. **XInput2 raw motion** — XWayland surfaces, when evdev isn't available.
3. **XQueryPointer** — last resort; the broken-on-Wayland path.

If cursor tracking "stops working when the pointer leaves the toolbar," it's
almost always this fallback chain dropping to level 3 because the udev rule
isn't installed.

### The two-uinput-device hack
We create **two** virtual devices: a pointer device (EV_REL) and a separate
**keyboard-only** device for Ctrl/Alt/Shift. Reason: libinput won't classify a
device that declares `EV_REL` as a keyboard, so modifier key events from the
mouse device are silently dropped on Wayland. Don't "simplify" this into one
device — it will break modifier-clicks on Wayland. The fake USB IDs
(`0x1234/0x5678`, `…5679`) are arbitrary placeholders.

## 3. Privileged / shell-out paths to keep audited

These are the spots where we elevate or shell out. Any change here is
security-sensitive — review carefully and keep inputs app-controlled.

- **udev rule install** (`mainwindow.cpp`): writes our bundled rule to a
  `QTemporaryFile`, then `pkexec /bin/sh -c "cp … && chmod 0644 … && udevadm …"`.
  The paths are ours; don't ever interpolate user-supplied strings into this script.
- **Package install** (`settingsdialog.cpp`): `pkexec <pkgmgr> install onboard`.
  Package name is hardcoded — keep it that way. apt path forces
  `DEBIAN_FRONTEND=noninteractive`; the progress dialog has **no cancel button**
  on purpose (interrupting apt mid-run can leave dpkg wedged).
- **Windows autostart** (`mainwindow.cpp` ~1487): writes `HKCU\…\Run` and scrubs
  the `StartupApproved\Run` "disabled" entry so Windows doesn't silently suppress
  us. `syncLaunchOnStartup()` re-asserts this every launch (self-heals stale paths).
- The `.deb` `postinst` reloads udev + icon/desktop caches; all failures are
  swallowed so install never fails in containers/build envs.

## 4. Platform footguns

- **Click indicator ring is Windows-only by default.** On Linux/Wayland the
  cursor position is unreliable enough that the ring lands on the wrong monitor —
  hence the "(Windows)" tag and the `#ifdef Q_OS_WIN` default in `AppSettings`.
  This was the subject of several recent Linux fixes; **regression-test click
  position on multi-monitor + scaling every release.**
- **Don't move the cursor before clicking** (`clickinjector.cpp` ~808). For a
  dwell clicker the cursor is already where it should be, and the delta math is
  wrong on Wayland.
- **macOS**: requires Accessibility permission (no input without it). Window uses
  `NSWindowCollectionBehaviorStationary | CanJoinAllSpaces` (`macos_utils.mm`) so
  the toolbar floats on all Spaces and survives Mission Control.
- **Audio click** depends on the Qt multimedia backend; on Ubuntu 22.04 that's
  GStreamer and PulseAudio/PipeWire capture is finicky. We poll on a timer
  instead of `readyRead` because pull-mode `readyRead` is unreliable there.
  Build can be compiled **without** audio (`HAVE_MULTIMEDIA` guards).
- The microphone audio-click is a **loudness threshold only** — no speech/word
  recognition. Don't let marketing imply otherwise.

## 5. Release gaps / things not done yet

- **No code signing or notarization.** Windows builds will trip SmartScreen;
  macOS builds will hit Gatekeeper and need a manual right-click→Open. This is
  the single biggest "looks sketchy to users" gap. Pre-1.0.
- **Linux ships `.deb` only** now (the non-deb/AppImage-style build was removed —
  see recent commits). RPM users fall back to manual build.
- Versioning flows from `CMakeLists.txt` `project(VERSION …)` into the macOS
  bundle and CPACK — bump it there, one place.

## 6. Testing seams

`DwellManager` exposes injectable function seams (cursor-pos, click, clock)
behind `#ifdef TRACKCLICK_TESTING` so the dwell state machine can be driven with
no display and no platform input layer. Use these in tests rather than trying to
simulate real input. See `setTestCursorPosFn` / `setTestClickFn` / `triggerPoll`
in `dwellmanager.h`.

---

*Keep this list current — when you add another privileged path, AV-sensitive
behaviour, or platform hack, add a row here.*
