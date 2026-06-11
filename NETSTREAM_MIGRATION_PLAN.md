# Bounce World Atari Netstream Migration Plan

This document is the working guide for moving the Atari 8-bit Bounce World client from FujiNet `fujinet-lib` TCP calls over SIO to FujiNet Netstream raw serial mode.

Source references:

- Bounce World client: `bounce-world-client`
- Netstream handler repo: `fujinet-atari-netstream`
- Netstream handler source: checked-out known-good MADS commit `41ed3f9`

Important constraints:

- The migration is Atari-specific. Other targets should continue using the current `fujinet-lib` network code.
- Netstream setup must use `57600` baud.
- Transport must be TCP.
- RX clock must be external.
- TX clock must be internal.
- Do not send the REGISTER flag. The existing Bounce World server does not support it.
- NTSC/PAL should be detected by the Netstream handler, as shown in the Atari examples.
- App-key and user setup must happen before Netstream raw serial mode begins, because Netstream disables normal SIO after activation.

Netstream flags for this client:

```c
#define NETSTREAM_FLAG_TCP     0x01
#define NETSTREAM_FLAG_TX_INT  0x00
#define NETSTREAM_FLAG_RX_EXT  0x08
#define NETSTREAM_FLAGS        (NETSTREAM_FLAG_TCP | NETSTREAM_FLAG_TX_INT | NETSTREAM_FLAG_RX_EXT)
```

Expected value: `0x09`.

Do not include:

- `0x02` REGISTER
- `0x04` TX external clock
- `0x20` UDP sequencing

## Phase 1: Integrate the MADS Netstream Handler

- [x] Use the known-good MADS handler source from `fujinet-atari-netstream/handler/netstream.s`.
- [x] Build `NSENGINE.OBX` with `mads` at fixed base `$2800`.
- [x] Add a small Atari-only C header for the exported Netstream functions:
  - `ns_begin_stream`
  - `ns_end_stream`
  - `ns_send_byte`
  - `ns_recv_byte`
  - `ns_bytes_avail`
  - `ns_get_status`
  - `ns_get_video_std`
  - `ns_init_netstream`
  - optional diagnostics: `ns_get_final_flags`, `ns_get_final_audf3`, `ns_get_final_audf4`
- [x] Confirm the MADS handler uses a fixed jump table and requires a cc65 `_ns_*` wrapper shim.

## Phase 2: Update Atari Build Integration

- [x] Update the Atari build so only `src/atari/netstream/netstream_api.s` is assembled and linked by cc65.
- [x] Remove the CA65 handler include path from Atari `ASFLAGS`; the MADS build uses the include directory from the checked-out handler repo.
- [x] Replace the direct-linked CA65 handler with the cc65 jump-table shim in `src/atari/netstream/netstream_api.s`.
- [x] Add a MADS build step for `build/NSENGINE.OBX`.
- [x] Concatenate `NSENGINE.OBX` before the cc65 app binary so the Atari DOS loader loads the handler segment first.
- [x] Move the cc65 app start address to `$4000` so it cannot overlap the handler at `$2800`.
- [x] Verify `make TARGETS=atari release` compiles far enough to catch assembler/linker integration errors.

## Phase 3: Add Atari Netstream Connection Backend

- [x] Keep the existing public connection API in `src/include/connection.h` unchanged.
- [x] In `src/common/connection.c`, split Atari networking from the existing `fujinet-lib` implementation with preprocessor guards.
- [x] Preserve the current `fujinet-lib` implementation for non-Atari targets.
- [x] Implement Atari `connect_service()` using:
  - parse current `server_url`
  - call `ns_init_netstream(host, NETSTREAM_FLAGS, 57600, swap16(port))`
  - call `ns_begin_stream()`
- [x] Implement Atari `disconnect_service()` using:
  - send existing `close <client>` command if a client was registered
  - call `ns_end_stream()`
- [x] Decide how to surface Netstream init failures through the existing `err` / `handle_err()` path.

## Phase 4: Parse Existing Server URL Format

- [x] Support the current stored endpoint format: `n1:tcp://host:port`.
- [x] Strip optional `n1:` prefix.
- [x] Strip optional `tcp://` prefix.
- [x] Extract host and decimal port.
- [x] Convert port to the byte-swapped value required by `ns_init_netstream()`.
- [x] Reject or handle missing/invalid host or port consistently with the current app behavior.
- [x] Keep parsing Atari-only unless it becomes useful to share.

## Phase 5: Replace Atari Read/Write Primitives

- [x] Implement Atari send loop around `ns_send_byte()`.
- [x] Add bounded waiting or retry behavior for a full Netstream TX buffer.
- [x] Implement `send_command()` through the Netstream send loop.
- [x] Implement `request_client_data()` through the Netstream send loop.
- [x] Implement `read_response_wait(buf, len)` to block until exactly `len` bytes are received or an error condition is raised.
- [x] Implement `read_response_min(buf, min, max)` to block until at least `min` bytes are received, then continue draining available bytes up to `max` as the old behavior requires.
- [x] Preserve the higher-level protocol behavior used by shapes, world state, keyboard commands, broadcasts, and client data requests.

## Phase 6: Startup and SIO Ordering Audit

- [x] Confirm `init_appkey()` still runs before Netstream mode begins.
- [x] Confirm `get_info()` still runs before Netstream mode begins.
- [x] Confirm all FujiNet app-key reads/writes are complete before `connect_service()`.
- [x] Search for any remaining Atari runtime calls to `fujinet-lib` network or Fuji APIs after `ns_begin_stream()`.
- [x] Remove or guard any remaining Atari SIO/FujiNet calls that would run during Netstream mode.

## Phase 7: Validation

- [x] Build Atari release with `make TARGETS=atari release`.
- [x] If release succeeds, build Atari disk with `make TARGETS=atari disk`.
- [x] Check binary size and memory layout after embedding the MADS handler. `build/NSENGINE.OBX` is 1173 bytes and `build/bwc.atari` is 15314 bytes; the final executable starts with the `$2800` handler segment and the cc65 app starts at `$4001`.
- [x] If the linker reports memory pressure, inspect `cfg/atari.cfg` and decide whether the start address or memory regions need adjustment. No linker memory pressure was reported.
- [ ] Run under emulator if the repo's existing Atari test workflow is configured locally.
- [ ] Smoke test against the Bounce World TCP server:
  - app-key prompt still works
  - Netstream init succeeds
  - shapes load
  - client registers
  - world state updates continuously
  - keyboard commands still receive aligned responses
  - quit path calls `ns_end_stream()`

## Phase 8: Cleanup and Documentation

- [x] Update README Atari notes if the launch/deploy workflow changes.
- [x] Keep docs clear that this is Atari Netstream only; other platforms still use `fujinet-lib`.
- [x] Remove any unused temporary diagnostics before finalizing.
- [x] Record final build/test commands and results in the completion summary.

## Server-Side TCP Framing Recommendation

- [ ] Update the Bounce World TCP server to treat incoming client data as a byte stream, not as packet-delimited commands.
- [ ] Accumulate bytes per client connection until ASCII LF (`0x0a`) is received.
- [ ] Process only complete command lines, trimming optional CR/LF before dispatch.
- [ ] Keep any trailing partial bytes in the per-client accumulator for the next TCP read.
- [ ] Continue writing the existing binary responses unchanged after each complete command.

The Netstream Atari client now sends text commands with an ASCII LF terminator, for example:

```text
x-shape-count\n
x-shape-data\n
x-add-client mozz,2,40,24\n
```

The remaining live failure is caused by TCP stream framing on the server side. The current server reads one available TCP chunk and processes that chunk as one complete command. Live FujiNet logs showed `x-add-client mozz,2,40,24\n` arriving at the server as two chunks:

```text
x-add-client mozz
,2,40,24\n
```

The server then responds `00` to each partial string, which the Atari client correctly reports as a bad client id. The Netstream handler transmit buffer is not the limiting factor: the handler TX ring is 128 bytes, while the full `x-add-client` command is roughly 26 bytes with the current name and LF terminator. Even if the Atari client queues the whole command before transmission, TCP and the FujiNet bridge can still segment the byte stream before the server's `readAvailable()` call returns.

Durable fix: change the server's TCP loop to line-frame commands by LF. A practical implementation is to maintain a `StringBuilder` or byte buffer per connection, append every successful read, and repeatedly process complete lines while leaving any final unterminated fragment in the buffer for the next read. This preserves the existing command protocol and binary response payloads while making command parsing reliable over Netstream TCP.

Current build/test results:

- `make TARGETS=atari release`: passed.
- `make TARGETS=c64 release`: passed as a non-Atari regression spot check with pre-existing warnings.
- `make TARGETS=atari disk`: passed after `dir2atr` was added to `PATH`; produced `dist/bwc.atr`.
- Runtime issue observed: after Netstream enable, FujiNet logs showed motor-line activity and the client stalled while parsing shapes.
- Motor-hold fix applied in `src/atari/netstream/netstream.s`: clear SIO countdown timer 1 and `critic` before asserting PACTL for the streaming session.
- Root cause found after repeated motor logs continued: `_ns_send_byte` was exported directly from the handler, but the handler originally reported success/full only via carry. C ignored carry and read A, causing `netstream_write()` to resend nonzero bytes indefinitely. `NS_SendByte_Impl` now returns `A=0` on success and `A=1` when full while preserving carry.
- `_ns_recv_byte` empty path now returns `A/X=$FFFF` for C safety; successful receives still return the byte in A.
- SKCTL clock-mode fix applied: the handler now preserves only the low nibble of `SSKCTL` and replaces serial clock bits 4-6 with the requested Netstream mode. This prevents stale SIO clock bits from overriding RX external / TX internal mode.
- Symbol/listing verification: `_ns_send_byte=$3B19`, `_ns_recv_byte=$3B50`, `_ns_bytes_avail=$3B9C`, `_ns_init_netstream=$3BC0`, `_ns_begin_stream=$3A0A`. `connection.o` calls these `_ns_*` symbols for Atari network I/O.
- Raw Netstream response boundary fix applied in `src/common/shapes.c`: Atari now reads exactly `shape_count` shape records from `x-shape-data` instead of stopping at a temporary empty receive buffer. This prevents leftover shape bytes from being consumed as the following `x-add-client` client id.
- Server protocol audit: the TCP server has no explicit initial handshake beyond accepting the connection. It reads one available TCP chunk as one command string, processes commands like `x-shape-count`, `x-shape-data`, `x-add-client`, `x-ws`, `x-who`, and `x-w <id>`, then writes one binary response. This means the Atari Netstream client must keep response reads aligned itself.
- Raw Netstream response boundary fix applied in `src/common/world.c`: Atari now reads `x-w <id>` client-state responses exactly as `[step][status][shapeCount][shape triples...]`, and reads `x-who` responses as `num_clients * 8` bytes after `x-ws` updates `num_clients`.
- Atari Netstream command writes now append LF (`\n`) after each text command. The server trims CR/LF before command processing, and the terminator gives the raw TCP stream a clear text-command boundary.
- Atari variable-length fallback reads now drain until a short idle gap instead of reading only bytes that are immediately available after the first byte. Exact reads are still used wherever the protocol provides enough length information.
- MADS handler SKCTL clock-mode fix applied in `fujinet-atari-netstream/handler/netstream.s`: the handler now saves only the low nibble of `SSKCTL` and ORs requested clock bits with that saved value. This prevents stale SIO clock mode bits from overriding RX external / TX internal after Netstream activation.
- Send/receive API carry fixes: `NS_SendByte_Impl` in the MADS handler now returns `A=0` on success and `A=1` on full because its `php`/`plp` sequence destroys carry. The cc65 `_ns_recv_byte` shim also ignores carry and returns `A` with `X=0`, because the client only calls it after `ns_bytes_avail() > 0`; this prevents valid bytes from being converted to `$FFFF`.
- MADS handler RX buffer increased from 128 bytes to 1024 bytes and its size initialization now stores both low and high bytes. The `x-shape-data` response is currently 191 bytes in one TCP burst, so the old 128-byte IRQ ring could overflow before the C parser drained it, corrupting record alignment and causing `shape data read`.
- Added tight cc65 assembly send helpers `_ns_send_cmd_tmp` and `_ns_send_client_data_cmd` in `src/atari/netstream/netstream_api.s`. Atari command sends now queue the whole command plus ASCII LF from assembly, avoiding the slower C byte-at-a-time loop that let the server observe split commands such as `x-add-client ` and `mozz,2,40,24`.
- Remaining protocol issue found in live logs: the server can still receive long commands as multiple TCP chunks, e.g. `x-add-client mozz` followed by `,2,40,24\n`. Because the current server processes each `readAvailable()` chunk as a complete command, it returns `00` for both partial strings. This cannot be made reliable from the client side; the TCP server must accumulate stream bytes until ASCII LF before processing a command.
- Handler audit: generated listings show only two compiled writes to `$D302/PACTL`, `NS_BeginConcurrent_Impl` asserting motor with `$34` and `NS_EndConcurrent_Impl` deasserting with `$3c`. The only compiled `SIOV` call in the embedded handler is the enable-netstream setup call before `ns_begin_stream()`.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing` and `make TARGETS=atari disk` passed after the Netstream handler and shape-stream fixes.
- CA65 handler conversion removed from the build. Atari now builds `build/NSENGINE.OBX` from the known-good MADS handler in `fujinet-atari-netstream` and links only `src/atari/netstream/netstream_api.s` as the cc65 shim.
- Stale copied CA65 handler include files were removed from `src/atari/netstream`; only `netstream.h` and `netstream_api.s` remain there.
- `make TARGETS=atari clean && make TARGETS=atari release OPTIONS=mapfile,labelfile,listing`: passed after switching to MADS.
- `make TARGETS=atari disk`: passed after switching to MADS; produced `dist/bwc.atr`.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing`: passed after removing the stale copied include files.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing` and `make TARGETS=atari disk`: passed after exact Netstream reads for client-state and who-list.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing` and `make TARGETS=atari disk`: passed after LF command terminators, idle-gap variable reads, and the MADS SKCTL fix. `build/NSENGINE.OBX` is now 1176 bytes; final `build/bwc.atari` is 15600 bytes.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing` and `make TARGETS=atari disk`: passed after the receive-side carry fix.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing` and `make TARGETS=atari disk`: passed after increasing the MADS handler RX buffer to 1024 bytes. `build/NSENGINE.OBX` is 1179 bytes, final `build/bwc.atari` is 15589 bytes, and the cc65 app still starts at `$4001`.
- `make TARGETS=atari release OPTIONS=mapfile,labelfile,listing` and `make TARGETS=atari disk`: passed after adding the tight assembly command sender.
- `make TARGETS=c64 release`: passed after the generic cc65 post-link hook change, with pre-existing warnings.
- Emulator and live Bounce World TCP server smoke tests have not been run in this environment.
