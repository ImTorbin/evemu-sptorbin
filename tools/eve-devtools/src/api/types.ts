// Hand-written mirrors of the server JSON shapes.  A future pass can generate
// these from /api/v1/openapi.json (which the server already emits).

export type ServerStatus = {
  status: string;
  serverBuild: string;
  buildDate: string;
  projectVersion: string;
  clientBuild: number;
  onlinePlayers: number;
  isTestServer: boolean;
};

export type Dungeon = {
  dungeonID: number;
  dungeonName: string;
  dungeonStatus: number;
  factionID: number;
  archetypeID: number;
  dungeonUUID?: string | null;
  rooms?: Room[];
};

export type Room = {
  roomID: number;
  roomName: string;
  objects?: RoomObject[];
};

export type RoomObject = {
  objectID: number;
  roomID: number;
  typeID: number;
  groupID: number;
  x: number;
  y: number;
  z: number;
  yaw: number;
  pitch: number;
  roll: number;
  radius: number;
};

export type Archetype = { archetypeID: number; archetypeName: string };
export type GroupRef  = { groupID: number; groupName: string };
export type Faction   = { factionID: number; factionName: string };

export type Client = {
  characterID: number;
  characterName: string;
  userID: number;
  systemID: number;
};

export type LogEntry = {
  ts: string;
  level: string;
  tag: string;
  message: string;
};
