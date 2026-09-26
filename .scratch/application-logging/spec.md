# Application logging

Status: ready-for-agent

## Problem Statement

When a live stream stalls after a brief network outage, the viewer can be left with a frozen frame and no practical way to supply a diagnostic trace. The application already emits Qt category messages, but it does not retain them in a file. A directly launched desktop build therefore loses the sequence of player, stream proxy, and live-message events needed to investigate intermittent failures.

## Solution

Record the application's Qt logs in one bounded, rotating file with a timestamp, severity, thread identifier, and module category on every entry. Enable file logging by default at `info`. Let the viewer turn file logging on or off, choose Normal or Playback diagnostics mode, and open the log directory from Settings. Save those choices in the user configuration directory and apply changes immediately. Allow configuration-file category overrides and temporary startup environment overrides. Protect credentials and playback URL authentication parameters before they reach any output. Logging failure or congestion must not interrupt viewing.

## User Stories

1. As a viewer, I want a log file to exist by default, so that an intermittent playback failure can be investigated after it happens.
2. As a viewer, I want the default log level to be `info`, so that routine runs capture useful events without excessive detail.
3. As a viewer, I want to disable file logging, so that I can stop retaining diagnostic data on disk.
4. As a viewer, I want Qt system output to remain available when file logging is disabled, so that startup and logging failures remain observable.
5. As a viewer, I want Settings changes to take effect as soon as I save them, so that I do not need to restart a live session.
6. As a viewer, I want my saved choices restored on the next launch, so that diagnostic behavior is predictable.
7. As a viewer, I want a Normal mode, so that everyday logging stays at the default level.
8. As a viewer, I want a Playback diagnostics mode, so that detailed player and stream proxy events are captured while investigating a stall.
9. As a viewer, I want to open the log directory from Settings, so that I can find a trace without locating an operating-system-specific path.
10. As a viewer, I want Settings to show when file logging is unavailable, so that I know whether a future problem can be diagnosed from a file.
11. As a viewer, I want turning off file logging to leave earlier log files intact, so that I do not accidentally discard an incident trace.
12. As a viewer, I want old files rotated, so that logs cannot grow without a defined size bound.
13. As a viewer, I want playback, proxy, and live-message entries on one timeline, so that I can correlate a network outage across modules.
14. As a developer, I want each entry to retain its Qt category and thread identity, so that I can filter a shared file by module and distinguish concurrent work.
15. As a developer, I want a global level with optional per-category overrides in the configuration file, so that I can increase detail only where the failure occurs.
16. As a developer, I want temporary environment overrides for a single launch, so that I can diagnose a startup or UI failure without changing saved preferences.
17. As a developer, I want dropped messages counted, so that a saturated logging queue is visible in the resulting trace.
18. As a viewer sharing a trace, I want credentials, connection keys, and playback URL authentication parameters absent from logs, so that diagnosis does not expose account access.
19. As a viewer, I want playback to continue if the log directory cannot be written or the logging queue is congested, so that diagnostics never become the cause of a viewing failure.
20. As a developer, I want concurrent Qt log calls to reach one consistent output path, so that multi-threaded operation does not corrupt records or rotation.

## Implementation Decisions

- Keep Qt category logging as the application-facing API. Bridge its process-wide message handler to spdlog; do not replace existing category call sites wholesale.
- Use the Conan-managed spdlog dependency and its multi-thread-capable rotating file sink. Preserve the existing Qt system output through the bridge.
- Initialize logging during application startup before ordinary application messages are emitted. Read saved settings first, then apply any startup environment overrides for that run without persisting them.
- Store logging preferences in the same per-user configuration location used by other application settings. Save and apply Settings edits together; manual file edits are read on the next launch.
- Enable file logging by default with a global `info` level. Normal mode uses that baseline. Playback diagnostics mode enables `debug` for player and proxy categories; individual category overrides remain configurable in the file.
- Route every application module to one log file. Include timestamp, severity, thread identifier, and Qt category in each entry.
- Rotate at 10 MB per file, keeping the current file and three older files. Disabling file logging stops new file writes and does not delete existing files.
- Use a bounded asynchronous write queue. Playback and other producer threads must not wait on disk writes. Under saturation, discard `debug` and `info` entries first; if the queue contains only higher-severity entries, discard the oldest entry so producers remain nonblocking. Count and report all discarded entries without recursively logging from the message handler.
- Flush the file sink at least once per second while idle and immediately after warnings or more severe records, so the shared log stays readable during a running session and preserves important failures.
- Accept `SHOWROOM_LOG_FILE_ENABLED`, `SHOWROOM_LOG_MODE`, and `SHOWROOM_LOG_RULES` as one-run environment overrides. Also honor existing `QT_LOGGING_RULES` on startup, then remove that environment value so saved Settings can take effect during the run.
- Redact known sensitive values at their source and defensively sanitize messages at the logging boundary. This includes cookies, tokens, live connection keys, and authentication parameters in playback URLs. Apply the same protection to file and preserved system output.
- Treat file open, write, and rotation failures as logging faults. Keep the application running, surface the fault in system output and Settings, and avoid recursive logging in the handler.
- The logging Settings section exposes a file-logging switch, Normal and Playback diagnostics modes, an availability/error indicator, and an action to open the log directory. Advanced per-category rules remain in the configuration file.
- File logging and a future playback stall recovery mechanism are separate features. Logging must capture evidence without changing the player's retry or reconnect behavior.

## Testing Decisions

- Prefer one Qt application-level integration seam that starts the public logging service and settings object, emits real Qt category messages from multiple threads, changes settings, and examines externally observable output files and status. Test behavior rather than private queue or sink implementation.
- At this seam, verify default-on `info` behavior; Normal and Playback diagnostics filtering; configuration persistence and startup override precedence; immediate changes after Save; shared-file category and thread fields; 10 MB rotation and retention; disabling without deletion; sensitive-value redaction in file and system output; concurrent record integrity; bounded-queue drop accounting; and nonfatal unwritable-directory behavior.
- Exercise queue saturation and rotation using controllable small limits in the test environment while preserving the production limits in normal runs.
- Perform one manual Settings UI smoke check for switch, mode selector, failure state, and open-directory action because the repository has no established QML UI test seam.
- No network outage or live Showroom service is required for logging tests; synthetic categorized events should make the test deterministic.

## Out of Scope

- Automatically recovering a frozen video stream after a network outage.
- Separate files for player, proxy, chat, gifts, or other modules.
- Automatic upload or sharing of log files.
- Deleting existing logs when file logging is disabled.
- A full UI editor for arbitrary per-category rules.
- Replacing Qt logging calls throughout the application with direct spdlog calls.

## Further Notes

- The accepted logging policy is recorded in the application's logging ADR.
- The current application enables detailed Qt categories at startup and has no file message handler. The implementation must remove that unconditional debug behavior and preserve the established categories.
- The existing Settings dialog handles proxy configuration. The logging controls should fit that user flow without changing proxy behavior.
- The Conan recipe, lockfile, CMake linkage, and CI dependency installation for spdlog are already present as uncommitted workspace changes. This spec covers the remaining logging behavior and its validation; an implementing agent should account for those changes without overwriting unrelated work.
