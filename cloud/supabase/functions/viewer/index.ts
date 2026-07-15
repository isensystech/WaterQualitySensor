// viewer — serves the static dive-viewer app (index.html, bundled via static_files in
// config.toml). Supabase URL + publishable key are injected from the function env at
// serve time so no project keys live in git. Deployed with verify_jwt=false: the page
// itself is public; all data access from it requires a Supabase Auth login.

let html: string | null = null;

Deno.serve(async () => {
  if (html === null) {
    html = (await Deno.readTextFile(new URL("./index.html", import.meta.url)))
      .replaceAll("__SUPABASE_URL__", Deno.env.get("SUPABASE_URL") ?? "")
      .replaceAll(
        "__SUPABASE_ANON_KEY__",
        Deno.env.get("SB_PUBLISHABLE_KEY") ?? Deno.env.get("SUPABASE_ANON_KEY") ?? "",
      );
  }
  return new Response(html, {
    headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-cache" },
  });
});
