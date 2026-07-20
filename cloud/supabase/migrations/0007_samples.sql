-- 0007_samples.sql  (DiveViewer Phase 5 — DECISION #1: hybrid)
-- Derived, re-derivable explosion of a dive's immutable CSV blob into rows. The blob stays
-- the source of truth (Hard Rule 5); samples exists for SQL compare (Phase 8) and as the
-- stable (dive_id, seq) anchor for annotations (Phase 7). parse-dive owns writes here.
--
-- Column <- CSV mapping (see firmware ms,utc,submerged,poi,...,cyc_conc,cels_T_C header):
--   t_ms<-ms  ts<-utc  submerged<-submerged  poi<-poi  depth_m<-depth_m
--   temp_c<-cels_T_C, else bar30T_C, else poetT_mC/1000   (resolved at parse time)
--   ph<-pH  orp_mv<-ORP_Eh_mV  ec_mscm<-EC_mScm  sal_psu<-sal_PSU  cyc_conc<-cyc_conc
-- Raw channels (P_mbar,*_uV,ec_nA,cyc_V,bar30T_C,poetT_mC) live in the blob only.

create table if not exists public.samples (
  dive_id   uuid not null references public.dives(id) on delete cascade,
  seq       int  not null,                 -- row index within the dive = annotation anchor
  t_ms      bigint,
  ts        timestamptz,
  submerged boolean,
  poi       boolean,
  depth_m   real,
  temp_c    real,
  ph        real,
  orp_mv    real,
  ec_mscm   real,
  sal_psu   real,
  cyc_conc  real,
  primary key (dive_id, seq)
);

alter table public.samples enable row level security;
grant select on public.samples to authenticated;
-- writes are performed by parse-dive with the service-role key, which bypasses RLS —
-- so no insert/update grant or policy for authenticated/anon here (read-only surface).
-- Defensive/idempotent: this project's default privileges already grant service_role ALL on
-- new public tables (verified live on dives/allowed_devices), so parse-dive's writes work
-- regardless; this keeps them working if that default is ever tightened. (RLS is bypassed by
-- service_role either way.) NOTE: those same defaults also grant ANON select/insert/delete on
-- samples — RLS (no anon policy) is what keeps anon out. See SMOKE-TEST.md "Optional hardening".
grant all on public.samples to service_role;

drop policy if exists samples_read_member on public.samples;
create policy samples_read_member on public.samples
  for select to authenticated using (public.can_read_dive(dive_id));
