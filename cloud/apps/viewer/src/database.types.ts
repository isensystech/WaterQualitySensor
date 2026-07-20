// database.types.ts — HAND-AUTHORED to match migrations 0001–0009.
// Canonical source is `supabase gen types typescript --linked > src/database.types.ts`
// (run AFTER `supabase db push`). This stub keeps the app typed until then; regenerate to
// pick up any drift. Only the columns the viewer reads are guaranteed exhaustive.

export type Json = string | number | boolean | null | { [k: string]: Json } | Json[];

export interface Database {
  public: {
    Tables: {
      allowed_devices: {
        Row: { mac: string; label: string | null; secret_hash: string | null; added_at: string };
        Insert: { mac: string; label?: string | null };
        Update: { label?: string | null };
        Relationships: [];
      };
      dives: {
        Row: {
          id: string;
          device_id: string;
          filename: string;
          storage_path: string;
          cast_num: number | null;
          mission: string | null;
          operator: string | null;
          site: string | null;
          water_type: string | null;
          lat: number | null;
          lon: number | null;
          weather: string | null;
          air_temp_c: number | null;
          notes: string | null;
          utc_start: string | null;
          time_source: string | null;
          cal_ph: boolean | null;
          cal_ec: boolean | null;
          cal_orp: boolean | null;
          cal_cyc: boolean | null;
          poet_en: boolean | null;
          bar30_en: boolean | null;
          cels_en: boolean | null;
          cyc_en: boolean | null;
          cyclops_units: string | null;
          row_count: number | null;
          uploaded_at: string;
          // 0006 additions
          project_id: string | null;
          site_id: string | null;
          archived: boolean;
          label: string | null;
          started_at: string | null;
          ended_at: string | null;
        };
        Insert: { device_id: string; filename: string; storage_path: string } & Record<string, unknown>;
        Update: Partial<Database["public"]["Tables"]["dives"]["Row"]>;
        Relationships: [];
      };
      profiles: {
        Row: { id: string; display_name: string | null; created_at: string };
        Insert: { id: string; display_name?: string | null };
        Update: { display_name?: string | null };
        Relationships: [];
      };
      projects: {
        Row: { id: string; name: string; owner_id: string; archived: boolean; created_at: string };
        Insert: { name: string; owner_id: string; archived?: boolean };
        Update: { name?: string; archived?: boolean };
        Relationships: [];
      };
      project_members: {
        Row: { project_id: string; user_id: string; role: "viewer" | "annotator" | "admin"; created_at: string };
        Insert: { project_id: string; user_id: string; role: "viewer" | "annotator" | "admin" };
        Update: { role?: "viewer" | "annotator" | "admin" };
        Relationships: [];
      };
      sites: {
        Row: { id: string; name: string; lat: number | null; lon: number | null; radius_m: number; created_at: string };
        Insert: { name: string; lat?: number | null; lon?: number | null; radius_m?: number };
        Update: Partial<{ name: string; lat: number | null; lon: number | null; radius_m: number }>;
        Relationships: [];
      };
      samples: {
        Row: {
          dive_id: string;
          seq: number;
          t_ms: number | null;
          ts: string | null;
          submerged: boolean | null;
          poi: boolean | null;
          depth_m: number | null;
          temp_c: number | null;
          ph: number | null;
          orp_mv: number | null;
          ec_mscm: number | null;
          sal_psu: number | null;
          cyc_conc: number | null;
        };
        Insert: Database["public"]["Tables"]["samples"]["Row"];
        Update: Partial<Database["public"]["Tables"]["samples"]["Row"]>;
        Relationships: [];
      };
      annotations: {
        Row: {
          id: string;
          dive_id: string;
          seq: number | null;
          author_id: string;
          kind: "text" | "image";
          body: string | null;
          image_path: string | null;
          created_at: string;
        };
        Insert: { dive_id: string; author_id: string; kind: "text" | "image"; seq?: number | null; body?: string | null; image_path?: string | null };
        Update: Partial<{ body: string | null; image_path: string | null }>;
        Relationships: [];
      };
      comments: {
        Row: { id: string; dive_id: string; author_id: string; body: string; created_at: string };
        Insert: { dive_id: string; author_id: string; body: string };
        Update: { body?: string };
        Relationships: [];
      };
      thresholds: {
        Row: {
          id: string;
          project_id: string | null;
          metric: string;
          warn_lo: number | null;
          warn_hi: number | null;
          crit_lo: number | null;
          crit_hi: number | null;
          created_at: string;
        };
        Insert: { metric: string; project_id?: string | null; warn_lo?: number | null; warn_hi?: number | null; crit_lo?: number | null; crit_hi?: number | null };
        Update: Partial<Database["public"]["Tables"]["thresholds"]["Row"]>;
        Relationships: [];
      };
    };
    Views: Record<string, never>;
    Functions: Record<string, never>;
    Enums: Record<string, never>;
    CompositeTypes: Record<string, never>;
  };
}
