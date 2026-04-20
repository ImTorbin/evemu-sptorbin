import { useEffect, useMemo, useRef, useState } from "react";
import { Link, Route, Routes, useNavigate, useParams } from "react-router-dom";

import { api } from "../api/client";
import { Dungeon, Room, RoomObject } from "../api/types";

export default function DungeonsPage() {
  return (
    <Routes>
      <Route index element={<DungeonList />} />
      <Route path=":id" element={<DungeonDetail />} />
    </Routes>
  );
}

function DungeonList() {
  const [items, setItems] = useState<Dungeon[]>([]);
  const [err, setErr] = useState<string | null>(null);
  const nav = useNavigate();

  async function refresh() {
    try {
      const r = await api.get<{ items: Dungeon[] }>("/api/v1/dungeons");
      setItems(r.items);
    } catch (e: any) { setErr(e?.message ?? String(e)); }
  }
  useEffect(() => { refresh(); }, []);

  async function create() {
    const name = prompt("New dungeon name?");
    if (!name) return;
    const r = await api.post<{ dungeonID: number }>("/api/v1/dungeons",
      { dungeonName: name, dungeonStatus: 0, factionID: 0, archetypeID: 0 });
    nav(`/dungeons/${r.dungeonID}`);
  }

  async function remove(id: number) {
    if (!confirm(`Delete dungeon ${id}?  This removes its rooms and objects.`)) return;
    await api.delete(`/api/v1/dungeons/${id}`);
    refresh();
  }

  return (
    <div>
      <div className="toolbar">
        <h2 style={{ margin: 0 }}>Dungeons</h2>
        <div className="spacer" />
        <button onClick={create}>New dungeon</button>
      </div>
      {err && <div className="error">{err}</div>}
      <div className="card">
        <table>
          <thead>
            <tr><th>ID</th><th>Name</th><th>Status</th><th>Faction</th><th>Archetype</th><th /></tr>
          </thead>
          <tbody>
            {items.map(d => (
              <tr key={d.dungeonID}>
                <td>{d.dungeonID}</td>
                <td><Link to={`/dungeons/${d.dungeonID}`}>{d.dungeonName}</Link></td>
                <td>{d.dungeonStatus}</td>
                <td>{d.factionID}</td>
                <td>{d.archetypeID}</td>
                <td>
                  <button className="danger" onClick={() => remove(d.dungeonID)}>Delete</button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function DungeonDetail() {
  const { id } = useParams();
  const [d, setD] = useState<Dungeon | null>(null);
  const [err, setErr] = useState<string | null>(null);

  async function refresh() {
    setErr(null);
    try {
      const r = await api.get<Dungeon>(`/api/v1/dungeons/${id}`);
      setD(r);
    } catch (e: any) { setErr(e?.message ?? String(e)); }
  }
  useEffect(() => { refresh(); }, [id]);

  async function save() {
    if (!d) return;
    await api.put(`/api/v1/dungeons/${d.dungeonID}`, {
      dungeonName:   d.dungeonName,
      dungeonStatus: d.dungeonStatus,
      factionID:     d.factionID,
      archetypeID:   d.archetypeID,
    });
    refresh();
  }

  async function addRoom() {
    const name = prompt("Room name?");
    if (!name || !d) return;
    await api.post(`/api/v1/dungeons/${d.dungeonID}/rooms`, { roomName: name });
    refresh();
  }

  async function queueSpawn() {
    if (!d) return;
    const systemID = Number(prompt("System ID to spawn in?") ?? "0");
    if (!systemID) return;
    const typeID = Number(prompt("Type ID (ship/structure)?") ?? "0");
    if (!typeID) return;
    await api.post("/api/v1/spawns", { systemID, typeID, x: 0, y: 0, z: 0 });
    alert("Spawn queued; SpawnMgr picks it up on next tick.");
  }

  if (!d) return <div>{err ? <div className="error">{err}</div> : "Loading…"}</div>;

  return (
    <div>
      <div className="toolbar">
        <h2 style={{ margin: 0 }}>{d.dungeonName}</h2>
        <div className="spacer" />
        <button onClick={save}>Save</button>
        <button className="secondary" onClick={queueSpawn}>Live spawn</button>
        <Link to="/dungeons"><button className="secondary">Back</button></Link>
      </div>

      <div className="card">
        <div className="grid-2">
          <div>
            <label>Name</label>
            <input value={d.dungeonName} onChange={e => setD({ ...d, dungeonName: e.target.value })} />
            <label style={{ marginTop: 8, display: "block" }}>Status</label>
            <input type="number" value={d.dungeonStatus} onChange={e => setD({ ...d, dungeonStatus: Number(e.target.value) })} />
          </div>
          <div>
            <label>Faction ID</label>
            <input type="number" value={d.factionID} onChange={e => setD({ ...d, factionID: Number(e.target.value) })} />
            <label style={{ marginTop: 8, display: "block" }}>Archetype ID</label>
            <input type="number" value={d.archetypeID} onChange={e => setD({ ...d, archetypeID: Number(e.target.value) })} />
          </div>
        </div>
      </div>

      <div className="card">
        <div className="toolbar">
          <h3 style={{ margin: 0 }}>Rooms</h3>
          <div className="spacer" />
          <button onClick={addRoom}>Add room</button>
        </div>
        {(d.rooms ?? []).map(r => (
          <RoomEditor key={r.roomID} room={r} onChanged={refresh} />
        ))}
      </div>
    </div>
  );
}

function RoomEditor({ room, onChanged }: { room: Room; onChanged: () => void }) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [selObj, setSelObj] = useState<number | null>(null);

  const objects = useMemo(() => room.objects ?? [], [room.objects]);

  // Simple top-down projection: flatten x/z to a 2D plane, ignore y.
  function draw() {
    const c = canvasRef.current;
    if (!c) return;
    const ctx = c.getContext("2d");
    if (!ctx) return;

    const w = c.clientWidth, h = c.clientHeight;
    c.width = w; c.height = h;
    ctx.fillStyle = "#06090f";
    ctx.fillRect(0, 0, w, h);

    if (objects.length === 0) return;

    let minX = Infinity, minZ = Infinity, maxX = -Infinity, maxZ = -Infinity;
    for (const o of objects) {
      minX = Math.min(minX, o.x); maxX = Math.max(maxX, o.x);
      minZ = Math.min(minZ, o.z); maxZ = Math.max(maxZ, o.z);
    }
    if (!isFinite(minX)) { minX = -1; maxX = 1; minZ = -1; maxZ = 1; }
    const rangeX = Math.max(1, maxX - minX);
    const rangeZ = Math.max(1, maxZ - minZ);
    const scale = Math.min((w - 40) / rangeX, (h - 40) / rangeZ);

    ctx.strokeStyle = "#2a3a5c";
    ctx.strokeRect(20, 20, w - 40, h - 40);

    for (const o of objects) {
      const px = 20 + (o.x - minX) * scale;
      const py = 20 + (o.z - minZ) * scale;
      const selected = o.objectID === selObj;
      ctx.fillStyle = selected ? "#61a6ff" : "#6bdc8a";
      ctx.beginPath(); ctx.arc(px, py, 6, 0, Math.PI * 2); ctx.fill();
      ctx.fillStyle = "#e6ecf5";
      ctx.font = "11px sans-serif";
      ctx.fillText(`#${o.objectID} t${o.typeID}`, px + 8, py + 4);
    }
  }

  useEffect(draw, [objects, selObj]);

  async function addObject() {
    const typeID = Number(prompt("Type ID?") ?? "0");
    if (!typeID) return;
    await api.post(`/api/v1/rooms/${room.roomID}/objects`, {
      typeID, groupID: 0, x: 0, y: 0, z: 0, yaw: 0, pitch: 0, roll: 0, radius: 0,
    });
    onChanged();
  }

  async function deleteObject(id: number) {
    if (!confirm(`Delete object ${id}?`)) return;
    await api.delete(`/api/v1/objects/${id}`);
    onChanged();
  }

  async function rename() {
    const name = prompt("New room name?", room.roomName);
    if (name == null) return;
    await api.put(`/api/v1/rooms/${room.roomID}`, { roomName: name });
    onChanged();
  }

  return (
    <div style={{ borderTop: "1px solid var(--border)", paddingTop: 10, marginTop: 10 }}>
      <div className="toolbar">
        <strong>#{room.roomID}</strong> {room.roomName}
        <div className="spacer" />
        <button className="secondary" onClick={rename}>Rename</button>
        <button onClick={addObject}>Add object</button>
      </div>
      <div className="canvas-host">
        <canvas
          ref={canvasRef}
          style={{ position: "absolute", inset: 0, width: "100%", height: "100%" }}
        />
      </div>
      <table style={{ marginTop: 10 }}>
        <thead>
          <tr><th>Object</th><th>Type</th><th>Group</th><th>X</th><th>Y</th><th>Z</th><th>Radius</th><th /></tr>
        </thead>
        <tbody>
          {objects.map(o => (
            <tr key={o.objectID} onClick={() => setSelObj(o.objectID)} style={{ cursor: "pointer", background: selObj === o.objectID ? "var(--panel-alt)" : undefined }}>
              <td>#{o.objectID}</td>
              <td>{o.typeID}</td>
              <td>{o.groupID}</td>
              <td>{o.x.toFixed(0)}</td>
              <td>{o.y.toFixed(0)}</td>
              <td>{o.z.toFixed(0)}</td>
              <td>{o.radius.toFixed(0)}</td>
              <td><button className="danger" onClick={() => deleteObject(o.objectID)}>Delete</button></td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
