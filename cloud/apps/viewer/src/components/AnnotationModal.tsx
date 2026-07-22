import { useEffect, useState, type ChangeEvent } from "react";
import {
  fetchDiveAnnotations, groupBySeq, upsertNote, addPhoto, deletePhoto, deleteNote, signedPhotoUrl,
  type Annotation,
} from "../lib/annotations";

// Modal to add/manage a POI's Name (title), Description (body) and Photos. Anchored to
// (dive_id, seq). Opened by clicking a POI marker on the graph.
export function AnnotationModal({
  diveId, seq, ordinal, timeLabel, authorId, onClose, onSaved,
}: {
  diveId: string; seq: number; ordinal: number; timeLabel: string;
  authorId: string; onClose: () => void; onSaved: () => void;
}) {
  const [note, setNote] = useState<Annotation | null>(null);
  const [photos, setPhotos] = useState<Annotation[]>([]);
  const [urls, setUrls] = useState<Record<string, string>>({});
  const [title, setTitle] = useState("");
  const [body, setBody] = useState("");
  const [busy, setBusy] = useState(false);
  const [err, setErr] = useState("");

  const load = async () => {
    try {
      const all = await fetchDiveAnnotations(diveId);
      const g = groupBySeq(all).get(seq) ?? { photos: [] };
      setNote(g.note ?? null);
      setTitle(g.note?.title ?? "");
      setBody(g.note?.body ?? "");
      setPhotos(g.photos);
      const entries = await Promise.all(
        g.photos.filter((p) => p.image_path).map(async (p) => [p.image_path!, await signedPhotoUrl(p.image_path!)] as const),
      );
      setUrls(Object.fromEntries(entries));
    } catch (e) { setErr((e as Error).message); }
  };
  useEffect(() => { load(); /* eslint-disable-next-line react-hooks/exhaustive-deps */ }, [diveId, seq]);

  const save = async () => {
    setBusy(true); setErr("");
    try {
      await upsertNote({ id: note?.id, diveId, seq, title, body, authorId });
      await load(); onSaved();
    } catch (e) { setErr((e as Error).message); } finally { setBusy(false); }
  };

  const upload = async (e: ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    e.target.value = "";
    if (!file) return;
    setBusy(true); setErr("");
    try { await addPhoto({ diveId, seq, file, authorId }); await load(); onSaved(); }
    catch (e2) { setErr((e2 as Error).message); } finally { setBusy(false); }
  };

  const removePhoto = async (a: Annotation) => {
    setBusy(true); setErr("");
    try { await deletePhoto(a); await load(); onSaved(); }
    catch (e) { setErr((e as Error).message); } finally { setBusy(false); }
  };

  const clearNote = async () => {
    if (!note) { onClose(); return; }
    setBusy(true); setErr("");
    try { await deleteNote(note.id); onSaved(); onClose(); }
    catch (e) { setErr((e as Error).message); setBusy(false); }
  };

  return (
    <div className="modalback" onClick={onClose}>
      <div className="modal" onClick={(e) => e.stopPropagation()}>
        <div className="chdr">
          <b>📌 POI {ordinal} · t+{timeLabel}</b>
          <button className="xbtn" onClick={onClose}>close</button>
        </div>

        <label className="fld">Name
          <input value={title} onChange={(e) => setTitle(e.target.value)} placeholder="e.g. Thermocline" />
        </label>
        <label className="fld">Description
          <textarea rows={3} value={body} onChange={(e) => setBody(e.target.value)}
                    placeholder="What was observed at this point of interest?" />
        </label>

        <div className="phdr">Photos</div>
        <div className="photos">
          {photos.map((p) => (
            <div key={p.id} className="photo">
              {p.image_path && urls[p.image_path]
                ? <img src={urls[p.image_path]} alt="annotation" />
                : <div className="phload">…</div>}
              <button className="pdel" title="Delete photo" onClick={() => removePhoto(p)} disabled={busy}>×</button>
            </div>
          ))}
          <label className={"photoadd" + (busy ? " dis" : "")}>
            +<input type="file" accept="image/*" onChange={upload} disabled={busy} hidden />
          </label>
        </div>

        {err && <p className="err">{err}</p>}
        <div className="mactions">
          <button onClick={save} disabled={busy}>{busy ? "Saving…" : "Save"}</button>
          {note && <button className="ghost" onClick={clearNote} disabled={busy}>Delete POI note</button>}
        </div>
      </div>
    </div>
  );
}
