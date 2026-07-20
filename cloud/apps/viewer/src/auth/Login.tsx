import { useState, type FormEvent } from "react";
import { Navigate, useLocation } from "react-router-dom";
import { supabase } from "../supabase";
import { useAuth } from "./AuthProvider";

export function Login() {
  const { session } = useAuth();
  const loc = useLocation() as { state?: { from?: { pathname?: string } } };
  const [email, setEmail] = useState("");
  const [pass, setPass] = useState("");
  const [err, setErr] = useState("");
  const [busy, setBusy] = useState(false);

  if (session) return <Navigate to={loc.state?.from?.pathname ?? "/"} replace />;

  const signIn = async (e: FormEvent) => {
    e.preventDefault();
    setErr(""); setBusy(true);
    const { error } = await supabase.auth.signInWithPassword({ email: email.trim(), password: pass });
    setBusy(false);
    if (error) setErr(error.message);
  };

  const google = async () => {
    setErr("");
    const { error } = await supabase.auth.signInWithOAuth({
      provider: "google",
      options: { redirectTo: window.location.origin },
    });
    if (error) setErr(error.message);
  };

  return (
    <div id="login">
      <h2>WQL Dive Viewer</h2>
      <div className="c">
        <p className="hint">Sign in to browse dives uploaded by the water-quality loggers.</p>
        <form onSubmit={signIn}>
          <input type="email" placeholder="Email" autoComplete="username"
                 value={email} onChange={(e) => setEmail(e.target.value)} />
          <input type="password" placeholder="Password" autoComplete="current-password"
                 value={pass} onChange={(e) => setPass(e.target.value)} />
          <button type="submit" style={{ width: "100%" }} disabled={busy}>
            {busy ? "Signing in…" : "Sign in"}
          </button>
        </form>
        <button className="ghost" style={{ width: "100%" }} onClick={google}>
          Continue with Google
        </button>
        {err && <p className="err">{err}</p>}
      </div>
    </div>
  );
}
