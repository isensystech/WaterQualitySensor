-- 0011_annotation_title.sql  (DiveViewer frontend v2 — POI annotations)
-- A POI (a sample row with poi=true) can now carry a short Name in addition to the free-text
-- description already held in annotations.body. Photos stay as multiple kind='image' rows at
-- the same (dive_id, seq) in the existing annotation-images bucket — no new table needed.
-- `title` is generic (any annotation may set it); RLS/grants from 0008 are unchanged.

alter table public.annotations add column if not exists title text;
