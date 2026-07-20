import { Routes, Route } from "react-router-dom";
import { Login } from "./auth/Login";
import { RequireAuth } from "./auth/RequireAuth";
import { DiveGraph } from "./screens/DiveGraph";

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<Login />} />
      <Route path="/" element={<RequireAuth><DiveGraph /></RequireAuth>} />
      <Route path="*" element={<RequireAuth><DiveGraph /></RequireAuth>} />
    </Routes>
  );
}
