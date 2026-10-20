# Native communication parser and first-fault diagnostics

The source client's variable-size command stream now waits for both length
bytes before consuming either. The custom command-table upper bound is exclusive;
the command immediately after the table enters the existing native error path.
No overflow is truncated, retried, ignored, or converted into a successful send.

Desktop QA can set `PLATINUM_COMMUNITY_DIAGNOSTICS=1` before launch. The exact
value is sampled once on the first fault, and at most one record is emitted to
stderr for that process. Other values leave diagnostics disabled. ARM builds
compile the diagnostic calls away. The record contains a fixed reason, an address
of the ring (or the native error caller), and six integer counters. It contains
no payload, credentials, account names, profile paths, or save data.

Counter meanings depend on the reason:

| Reason | Counters in order |
| --- | --- |
| `ring-overflow` | start, published end, pending end, capacity, requested bytes, existing native caller tag |
| `wrong-server-send` | command, size, local native net ID, communication type, 0, 0 |
| `invalid-command-callback` | command, custom table count, source net ID, payload size, 0, 0 |
| `invalid-command-size` | command, custom table count, 0, 0, 0, 0 |
| `missing-command-table` | command, 0, 0, 0, 0, 0 |
| `native-error` | communication type, transmission type, shutdown flag, prior error flag, 0, 0 |

`site` is a process-local address, useful with that build's own symbols. It is
not a stable identifier across processes. Specific error paths record their
context before the generic native-error function runs. The gate and first-record
claim use atomics because receive callbacks may run on a different thread.

`python tests/run_community_comm_parser.py --engine-build build_win64` compiles
the actual public ring/parser/command functions with isolated dependencies and
checks every byte split of a variable command, ring wrap, allocated receive
buffers, exclusive table limits, unchanged overflow refusal, exact opt-in and
immutable disabled state. Reinstating only the old one-byte check causes the
fixture to fail. The optional engine argument also compiles all three complete
changed translation units using the configured real engine headers, without
writing shared build outputs.

These tests prove the parser defect and diagnostics behavior. They do not prove
that this defect caused a particular multiplayer failure or that a full battle
or trade completes. A fresh two-engine session must capture its first fault if
the failure recurs.
