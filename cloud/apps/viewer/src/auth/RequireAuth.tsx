import type { ReactNode } from "react";
import { Navigate, useLocation } from "react-router-dom";
import { useAuth } from "./AuthProvider";

export function RequireAuth({ children }: { children: ReactNode }) {
  const { session, loading } = useAuth();
  const loc = useLocation();
  if (loading) return <p className="hint" style={{ padding: 24 }}>Loading…</p>;
  if (!session) return <Navigate to="/login" state={{ from: loc }} replace />;
  return <>{children}</>;
}
