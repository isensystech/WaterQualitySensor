// annotations.ts — POI notes + photos on the 0008/0011 schema. A POI (sample with poi=true)
// at (dive_id, seq) is represented by: one kind='text' annotation holding the Name (title) +
// Description (body), plus zero-or-more kind='image' annotations (image_path) at the same seq.
// Photos live in the private 'annotation-images' bucket under '<dive_id>/...' so the 0008
// bucket RLS (leading path segment = dive id) authorizes member reads / annotator writes.
import { supabase } from "../supabase";
import type { Database } from "../database.types";

export type Annotation = Database["public"]["Tables"]["annotations"]["Row"];
const BUCKET = "annotation-images";

export async function fetchDiveAnnotations(diveId: string): Promise<Annotation[]> {
  const { data, error } = await supabase
    .from("annotations").select("*").eq("dive_id", diveId).order("created_at");
  if (error) throw error;
  return data ?? [];
}

// group by seq -> { note (first text row), photos (image rows) }
export interface SeqAnnotations { note?: Annotation; photos: Annotation[] }
export function groupBySeq(rows: Annotation[]): Map<number, SeqAnnotations> {
  const m = new Map<number, SeqAnnotations>();
  for (const a of rows) {
    if (a.seq == null) continue;
    const g = m.get(a.seq) ?? { photos: [] };
    if (a.kind === "text" && !g.note) g.note = a;
    else if (a.kind === "image") g.photos.push(a);
    m.set(a.seq, g);
  }
  return m;
}

export async function upsertNote(p: {
  id?: string; diveId: string; seq: number; title: string; body: string; authorId: string;
}): Promise<Annotation> {
  if (p.id) {
    const { data, error } = await supabase.from("annotations")
      .update({ title: p.title || null, body: p.body || null })
      .eq("id", p.id).select("*").single();
    if (error) throw error;
    return data;
  }
  const { data, error } = await supabase.from("annotations")
    .insert({ dive_id: p.diveId, seq: p.seq, author_id: p.authorId, kind: "text", title: p.title || null, body: p.body || null })
    .select("*").single();
  if (error) throw error;
  return data;
}

export async function addPhoto(p: {
  diveId: string; seq: number; file: File; authorId: string;
}): Promise<Annotation> {
  const ext = (p.file.name.split(".").pop() || "jpg").toLowerCase().replace(/[^a-z0-9]/g, "");
  const path = `${p.diveId}/${p.seq}/${crypto.randomUUID()}.${ext}`;
  const up = await supabase.storage.from(BUCKET).upload(path, p.file, {
    contentType: p.file.type || "image/jpeg", upsert: false,
  });
  if (up.error) throw up.error;
  const { data, error } = await supabase.from("annotations")
    .insert({ dive_id: p.diveId, seq: p.seq, author_id: p.authorId, kind: "image", image_path: path })
    .select("*").single();
  if (error) throw error;
  return data;
}

export async function deletePhoto(a: Annotation): Promise<void> {
  if (a.image_path) await supabase.storage.from(BUCKET).remove([a.image_path]);
  const { error } = await supabase.from("annotations").delete().eq("id", a.id);
  if (error) throw error;
}

export async function deleteNote(id: string): Promise<void> {
  const { error } = await supabase.from("annotations").delete().eq("id", id);
  if (error) throw error;
}

// private bucket -> signed URLs for <img> display
export async function signedPhotoUrl(path: string): Promise<string> {
  const { data, error } = await supabase.storage.from(BUCKET).createSignedUrl(path, 3600);
  if (error || !data) throw error ?? new Error("sign failed");
  return data.signedUrl;
}
