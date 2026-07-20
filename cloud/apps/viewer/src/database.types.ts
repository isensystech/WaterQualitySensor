export type Json =
  | string
  | number
  | boolean
  | null
  | { [key: string]: Json | undefined }
  | Json[]

export type Database = {
  // Allows to automatically instantiate createClient with right options
  // instead of createClient<Database, { PostgrestVersion: 'XX' }>(URL, KEY)
  __InternalSupabase: {
    PostgrestVersion: "14.5"
  }
  graphql_public: {
    Tables: {
      [_ in never]: never
    }
    Views: {
      [_ in never]: never
    }
    Functions: {
      graphql: {
        Args: {
          extensions?: Json
          operationName?: string
          query?: string
          variables?: Json
        }
        Returns: Json
      }
    }
    Enums: {
      [_ in never]: never
    }
    CompositeTypes: {
      [_ in never]: never
    }
  }
  public: {
    Tables: {
      allowed_devices: {
        Row: {
          added_at: string
          label: string | null
          mac: string
          secret_hash: string | null
        }
        Insert: {
          added_at?: string
          label?: string | null
          mac: string
          secret_hash?: string | null
        }
        Update: {
          added_at?: string
          label?: string | null
          mac?: string
          secret_hash?: string | null
        }
        Relationships: []
      }
      annotations: {
        Row: {
          author_id: string
          body: string | null
          created_at: string
          dive_id: string
          id: string
          image_path: string | null
          kind: string
          seq: number | null
        }
        Insert: {
          author_id: string
          body?: string | null
          created_at?: string
          dive_id: string
          id?: string
          image_path?: string | null
          kind: string
          seq?: number | null
        }
        Update: {
          author_id?: string
          body?: string | null
          created_at?: string
          dive_id?: string
          id?: string
          image_path?: string | null
          kind?: string
          seq?: number | null
        }
        Relationships: [
          {
            foreignKeyName: "annotations_author_id_fkey"
            columns: ["author_id"]
            isOneToOne: false
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "annotations_dive_id_fkey"
            columns: ["dive_id"]
            isOneToOne: false
            referencedRelation: "dives"
            referencedColumns: ["id"]
          },
        ]
      }
      comments: {
        Row: {
          author_id: string
          body: string
          created_at: string
          dive_id: string
          id: string
        }
        Insert: {
          author_id: string
          body: string
          created_at?: string
          dive_id: string
          id?: string
        }
        Update: {
          author_id?: string
          body?: string
          created_at?: string
          dive_id?: string
          id?: string
        }
        Relationships: [
          {
            foreignKeyName: "comments_author_id_fkey"
            columns: ["author_id"]
            isOneToOne: false
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "comments_dive_id_fkey"
            columns: ["dive_id"]
            isOneToOne: false
            referencedRelation: "dives"
            referencedColumns: ["id"]
          },
        ]
      }
      dives: {
        Row: {
          air_temp_c: number | null
          archived: boolean
          bar30_en: boolean | null
          cal_cyc: boolean | null
          cal_ec: boolean | null
          cal_orp: boolean | null
          cal_ph: boolean | null
          cast_num: number | null
          cels_en: boolean | null
          cyc_en: boolean | null
          cyclops_units: string | null
          device_id: string
          ended_at: string | null
          filename: string
          id: string
          label: string | null
          lat: number | null
          lon: number | null
          mission: string | null
          notes: string | null
          operator: string | null
          poet_en: boolean | null
          project_id: string | null
          row_count: number | null
          site: string | null
          site_id: string | null
          started_at: string | null
          storage_path: string
          time_source: string | null
          uploaded_at: string
          utc_start: string | null
          water_type: string | null
          weather: string | null
        }
        Insert: {
          air_temp_c?: number | null
          archived?: boolean
          bar30_en?: boolean | null
          cal_cyc?: boolean | null
          cal_ec?: boolean | null
          cal_orp?: boolean | null
          cal_ph?: boolean | null
          cast_num?: number | null
          cels_en?: boolean | null
          cyc_en?: boolean | null
          cyclops_units?: string | null
          device_id: string
          ended_at?: string | null
          filename: string
          id?: string
          label?: string | null
          lat?: number | null
          lon?: number | null
          mission?: string | null
          notes?: string | null
          operator?: string | null
          poet_en?: boolean | null
          project_id?: string | null
          row_count?: number | null
          site?: string | null
          site_id?: string | null
          started_at?: string | null
          storage_path: string
          time_source?: string | null
          uploaded_at?: string
          utc_start?: string | null
          water_type?: string | null
          weather?: string | null
        }
        Update: {
          air_temp_c?: number | null
          archived?: boolean
          bar30_en?: boolean | null
          cal_cyc?: boolean | null
          cal_ec?: boolean | null
          cal_orp?: boolean | null
          cal_ph?: boolean | null
          cast_num?: number | null
          cels_en?: boolean | null
          cyc_en?: boolean | null
          cyclops_units?: string | null
          device_id?: string
          ended_at?: string | null
          filename?: string
          id?: string
          label?: string | null
          lat?: number | null
          lon?: number | null
          mission?: string | null
          notes?: string | null
          operator?: string | null
          poet_en?: boolean | null
          project_id?: string | null
          row_count?: number | null
          site?: string | null
          site_id?: string | null
          started_at?: string | null
          storage_path?: string
          time_source?: string | null
          uploaded_at?: string
          utc_start?: string | null
          water_type?: string | null
          weather?: string | null
        }
        Relationships: [
          {
            foreignKeyName: "dives_device_id_fkey"
            columns: ["device_id"]
            isOneToOne: false
            referencedRelation: "allowed_devices"
            referencedColumns: ["mac"]
          },
          {
            foreignKeyName: "dives_project_id_fkey"
            columns: ["project_id"]
            isOneToOne: false
            referencedRelation: "projects"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "dives_site_id_fkey"
            columns: ["site_id"]
            isOneToOne: false
            referencedRelation: "sites"
            referencedColumns: ["id"]
          },
        ]
      }
      profiles: {
        Row: {
          created_at: string
          display_name: string | null
          id: string
        }
        Insert: {
          created_at?: string
          display_name?: string | null
          id: string
        }
        Update: {
          created_at?: string
          display_name?: string | null
          id?: string
        }
        Relationships: []
      }
      project_members: {
        Row: {
          created_at: string
          project_id: string
          role: string
          user_id: string
        }
        Insert: {
          created_at?: string
          project_id: string
          role: string
          user_id: string
        }
        Update: {
          created_at?: string
          project_id?: string
          role?: string
          user_id?: string
        }
        Relationships: [
          {
            foreignKeyName: "project_members_project_id_fkey"
            columns: ["project_id"]
            isOneToOne: false
            referencedRelation: "projects"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "project_members_user_id_fkey"
            columns: ["user_id"]
            isOneToOne: false
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
        ]
      }
      projects: {
        Row: {
          archived: boolean
          created_at: string
          id: string
          name: string
          owner_id: string
        }
        Insert: {
          archived?: boolean
          created_at?: string
          id?: string
          name: string
          owner_id: string
        }
        Update: {
          archived?: boolean
          created_at?: string
          id?: string
          name?: string
          owner_id?: string
        }
        Relationships: [
          {
            foreignKeyName: "projects_owner_id_fkey"
            columns: ["owner_id"]
            isOneToOne: false
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
        ]
      }
      samples: {
        Row: {
          cyc_conc: number | null
          depth_m: number | null
          dive_id: string
          ec_mscm: number | null
          orp_mv: number | null
          ph: number | null
          poi: boolean | null
          sal_psu: number | null
          seq: number
          submerged: boolean | null
          t_ms: number | null
          temp_c: number | null
          ts: string | null
        }
        Insert: {
          cyc_conc?: number | null
          depth_m?: number | null
          dive_id: string
          ec_mscm?: number | null
          orp_mv?: number | null
          ph?: number | null
          poi?: boolean | null
          sal_psu?: number | null
          seq: number
          submerged?: boolean | null
          t_ms?: number | null
          temp_c?: number | null
          ts?: string | null
        }
        Update: {
          cyc_conc?: number | null
          depth_m?: number | null
          dive_id?: string
          ec_mscm?: number | null
          orp_mv?: number | null
          ph?: number | null
          poi?: boolean | null
          sal_psu?: number | null
          seq?: number
          submerged?: boolean | null
          t_ms?: number | null
          temp_c?: number | null
          ts?: string | null
        }
        Relationships: [
          {
            foreignKeyName: "samples_dive_id_fkey"
            columns: ["dive_id"]
            isOneToOne: false
            referencedRelation: "dives"
            referencedColumns: ["id"]
          },
        ]
      }
      sites: {
        Row: {
          created_at: string
          id: string
          lat: number | null
          lon: number | null
          name: string
          radius_m: number
        }
        Insert: {
          created_at?: string
          id?: string
          lat?: number | null
          lon?: number | null
          name: string
          radius_m?: number
        }
        Update: {
          created_at?: string
          id?: string
          lat?: number | null
          lon?: number | null
          name?: string
          radius_m?: number
        }
        Relationships: []
      }
      thresholds: {
        Row: {
          created_at: string
          crit_hi: number | null
          crit_lo: number | null
          id: string
          metric: string
          project_id: string | null
          warn_hi: number | null
          warn_lo: number | null
        }
        Insert: {
          created_at?: string
          crit_hi?: number | null
          crit_lo?: number | null
          id?: string
          metric: string
          project_id?: string | null
          warn_hi?: number | null
          warn_lo?: number | null
        }
        Update: {
          created_at?: string
          crit_hi?: number | null
          crit_lo?: number | null
          id?: string
          metric?: string
          project_id?: string | null
          warn_hi?: number | null
          warn_lo?: number | null
        }
        Relationships: [
          {
            foreignKeyName: "thresholds_project_id_fkey"
            columns: ["project_id"]
            isOneToOne: false
            referencedRelation: "projects"
            referencedColumns: ["id"]
          },
        ]
      }
    }
    Views: {
      [_ in never]: never
    }
    Functions: {
      can_annotate: { Args: { pid: string }; Returns: boolean }
      can_annotate_dive: { Args: { did: string }; Returns: boolean }
      can_read_dive: { Args: { did: string }; Returns: boolean }
      can_read_dive_object: { Args: { obj: string }; Returns: boolean }
      is_project_admin: { Args: { pid: string }; Returns: boolean }
      is_project_member: { Args: { pid: string }; Returns: boolean }
      safe_uuid: { Args: { t: string }; Returns: string }
    }
    Enums: {
      [_ in never]: never
    }
    CompositeTypes: {
      [_ in never]: never
    }
  }
}

type DatabaseWithoutInternals = Omit<Database, "__InternalSupabase">

type DefaultSchema = DatabaseWithoutInternals[Extract<keyof Database, "public">]

export type Tables<
  DefaultSchemaTableNameOrOptions extends
    | keyof (DefaultSchema["Tables"] & DefaultSchema["Views"])
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof (DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"] &
        DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Views"])
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? (DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"] &
      DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Views"])[TableName] extends {
      Row: infer R
    }
    ? R
    : never
  : DefaultSchemaTableNameOrOptions extends keyof (DefaultSchema["Tables"] &
        DefaultSchema["Views"])
    ? (DefaultSchema["Tables"] &
        DefaultSchema["Views"])[DefaultSchemaTableNameOrOptions] extends {
        Row: infer R
      }
      ? R
      : never
    : never

export type TablesInsert<
  DefaultSchemaTableNameOrOptions extends
    | keyof DefaultSchema["Tables"]
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"]
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"][TableName] extends {
      Insert: infer I
    }
    ? I
    : never
  : DefaultSchemaTableNameOrOptions extends keyof DefaultSchema["Tables"]
    ? DefaultSchema["Tables"][DefaultSchemaTableNameOrOptions] extends {
        Insert: infer I
      }
      ? I
      : never
    : never

export type TablesUpdate<
  DefaultSchemaTableNameOrOptions extends
    | keyof DefaultSchema["Tables"]
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"]
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"][TableName] extends {
      Update: infer U
    }
    ? U
    : never
  : DefaultSchemaTableNameOrOptions extends keyof DefaultSchema["Tables"]
    ? DefaultSchema["Tables"][DefaultSchemaTableNameOrOptions] extends {
        Update: infer U
      }
      ? U
      : never
    : never

export type Enums<
  DefaultSchemaEnumNameOrOptions extends
    | keyof DefaultSchema["Enums"]
    | { schema: keyof DatabaseWithoutInternals },
  EnumName extends DefaultSchemaEnumNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaEnumNameOrOptions["schema"]]["Enums"]
    : never = never,
> = DefaultSchemaEnumNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaEnumNameOrOptions["schema"]]["Enums"][EnumName]
  : DefaultSchemaEnumNameOrOptions extends keyof DefaultSchema["Enums"]
    ? DefaultSchema["Enums"][DefaultSchemaEnumNameOrOptions]
    : never

export type CompositeTypes<
  PublicCompositeTypeNameOrOptions extends
    | keyof DefaultSchema["CompositeTypes"]
    | { schema: keyof DatabaseWithoutInternals },
  CompositeTypeName extends PublicCompositeTypeNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[PublicCompositeTypeNameOrOptions["schema"]]["CompositeTypes"]
    : never = never,
> = PublicCompositeTypeNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[PublicCompositeTypeNameOrOptions["schema"]]["CompositeTypes"][CompositeTypeName]
  : PublicCompositeTypeNameOrOptions extends keyof DefaultSchema["CompositeTypes"]
    ? DefaultSchema["CompositeTypes"][PublicCompositeTypeNameOrOptions]
    : never

export const Constants = {
  graphql_public: {
    Enums: {},
  },
  public: {
    Enums: {},
  },
} as const
