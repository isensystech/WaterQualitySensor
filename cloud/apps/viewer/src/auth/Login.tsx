import { useState, type FormEvent } from "react";
import { Navigate, useLocation } from "react-router-dom";
import { Button, Container, Divider, Paper, PasswordInput, Stack, Text, TextInput, Title } from "@mantine/core";
import { supabase } from "../supabase";
import { useAuth } from "./AuthProvider";

// Pilot component for the Mantine migration (see src/theme.ts). Same logic as before — this
// only changes how the screen is built: Mantine primitives + theme tokens instead of the
// hand-rolled #login/.c/input/button classes from index.css. Nothing else in the app depends
// on this file, so it's a safe first screen to convert and react to before doing the rest.
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
    <Container size={420} my={80}>
      <Title order={2} ta="center" c="brand">WQL Dive Viewer</Title>
      <Paper withBorder radius="md" p="lg" mt="lg" bg="dark.7">
        <Text size="sm" c="dimmed" mb="md">
          Sign in to browse dives uploaded by the water-quality loggers.
        </Text>
        <form onSubmit={signIn}>
          <Stack gap="sm">
            <TextInput
              label="Email" type="email" autoComplete="username" required
              value={email} onChange={(e) => setEmail(e.currentTarget.value)}
            />
            <PasswordInput
              label="Password" autoComplete="current-password" required
              value={pass} onChange={(e) => setPass(e.currentTarget.value)}
            />
            <Button type="submit" fullWidth loading={busy} mt="xs">
              Sign in
            </Button>
          </Stack>
        </form>
        <Divider label="or" labelPosition="center" my="md" />
        <Button variant="default" fullWidth onClick={google}>
          Continue with Google
        </Button>
        {err && <Text c="red" size="sm" mt="md">{err}</Text>}
      </Paper>
    </Container>
  );
}
