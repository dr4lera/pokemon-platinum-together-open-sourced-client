# Reactive private QA controller

Set `PLATINUM_COMMUNITY_CONTROLLER` to an absolute private command-file path
before launching an own-client disposable QA process. The simulator checks that
regular file once per emulated frame. It never sends OS keyboard/mouse events,
changes focus, or evaluates commands. Without this variable, existing input and
scenario behavior are unchanged. Enabling it together with
`PLATINUM_COMMUNITY_SCENARIO` is refused with exit status2.

Each complete ASCII record is at most64 bytes, with exactly one space between
fields and a final LF:

```text
1 001 2
```

This holds A for two emulated frames and then releases all buttons. The fields
are decimal ID (`1..2147483647`), one to three hexadecimal mask digits
(`000..fff`, without `0x`), and decimal duration (`1..180`). Records with CRLF,
BOM, extra whitespace, trailing content, NUL bytes, out-of-range numbers, or
missing LF are ignored. Only IDs greater than the last accepted ID apply.
Re-reading the same file never extends a hold. A newer ID replaces an active
hold; `2 000 1` explicitly releases it. IDs are scoped to the running process.

| Button | Mask |
| --- | --- |
| A / B | `001` / `002` |
| Select / Start | `004` / `008` |
| Right / Left | `010` / `020` |
| Up / Down | `040` / `080` |
| R / L | `100` / `200` |
| X / Y | `400` / `800` |

For example, PowerShell can publish a record using
`[IO.File]::WriteAllText($commandPath, "1 001 2`n", [Text.UTF8Encoding]::new($false))`.
The path belongs to the disposable test process; never point the controller at
a user document or profile. Atomic replacement is suitable for a producer, but
is not required for safe release: incomplete records are ignored and the
current hold still expires. Files larger than64 bytes and non-regular files are
refused before reading. Each actual read is capped at65 bytes so a grown file
cannot cause an unbounded read. This private file protocol assumes its owner
does not concurrently replace the selected regular file with a device/pipe.

Optionally set `PLATINUM_COMMUNITY_CONTROLLER_ACK` to a different absolute
private file. This is the only file the helper writes. Its bounded record is
`id held-mask remaining-frames` with LF, updated when a command is accepted or
its held mask changes. After release it reads, for example, `1 000 0`.
Acknowledgments are observational and do not represent native menu acceptance
or completed gameplay. Without an explicit acknowledgment path, the controller
does not write any file.

`python tests/run_community_controller.py` compiles the actual portable helper
with C11 warnings as errors. Tests cover strict parsing, bounds/overflow,
monotonic IDs, replay immunity, replacement, explicit release, exact 1/2/180
frame release, and the65-byte maximum read. The updated simulator source also
passes an isolated syntax check with its actual native compilation arguments.
These checks do not substitute for a whole-client reactive QA run.

For reactive captures of this application's own OpenGL framebuffer, set
`PLATINUM_COMMUNITY_CAPTURE_REQUEST` to an absolute private request file and
`PLATINUM_COMMUNITY_CAPTURE` to an absolute output base such as `C:/Private/host`.
Write a positive decimal ID followed by LF, for example `1` plus LF. The record
is at most16 bytes and IDs use the same `1..2147483647` range. A newer request is
captured at the end of the rendered frame into `C:/Private/host.1.ppm`. Replays
and malformed requests are ignored. Generated images use exclusive creation
and never replace existing files. This reads no desktop/window pixels outside
the application's own framebuffer. Request mode supersedes fixed-frame capture;
without a request-file setting, the existing one-shot capture behavior remains.
The controller fixture also tests the actual separate capture parser and
monotonic/read bounds. Actual GPU image contents require whole-client testing.
