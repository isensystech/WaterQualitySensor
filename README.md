# Water Quality Logger — Monorepo

Three stacks, one repo. The shared contract between them is **the Supabase schema** —
not code. Each stack builds and deploys independently.

/firmware      ESP32-C6 dive logger  (PlatformIO / pioarduino)
/cloud         Supabase: schema migrations + viewer app + edge functions   <- START HERE
/basestation   Raspberry Pi 5: sensor drivers, local API, kiosk UI, offload
/docs          architecture + to-do docs

Open each folder in its own VS Code window -> one Claude Code context per stack.
Each folder has its own CLAUDE.md.

## Build order
1. Cloud — schema/RLS -> seed MAC allowlist -> (then firmware upload leg) -> viewer.
2. Firmware — add cloud-upload leg, test one logger end to end.
3. Base station — Pi AP + Timescale -> one Modbus driver -> MQTT -> offload -> kiosk UI.

## The interface all three agree on
- dives — logger-specific (cast/mission/POI). Loggers write via publishable key + MAC allowlist.
- stations + readings — generic env suite. Base station writes via secret key (trusted server).

## Hard rules
- Secrets never in git. .gitignore service/secret keys; use .env + Supabase secrets.
- Schema is the source of truth in /cloud/supabase/migrations/ — apply via CLI, not dashboard.
- Firmware project files lag the working tree — verify version markers before editing.
