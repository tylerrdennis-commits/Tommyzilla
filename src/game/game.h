// Core game simulation. Everything here is platform independent: the Windows layer feeds an InputState in,
// and gets a RenderList (instances + UI triangles), sound events, loop volumes and rumble back out.
#pragma once
#include "titan.h"
#include "vault_powers.h"  // Vault
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "../audio/sound_ids.h"
#include "../core/math.h"
#include "../core/rng.h"
#include "config.h"
#include "eggs.h"
#include "input_state.h"
#include "progress.h"
#include "render_types.h"
#include "saveslots.h"
#include "tables.h"
namespace tz {
struct UIDraw;
// ---------------------------------------------------------------- shared primitives
struct Part { MeshId mesh; Mat4 local; Color color; float unlit = 0; };
// three.js-style local transform: position, Euler XYZ rotation, scale
Mat4 partM(float x, float y, float z, float rx = 0, float ry = 0, float rz = 0, float sx = 1, float sy = 1, float sz = 1);
inline Part part(MeshId m, Color c, const Mat4& l, float unlit = 0) { return Part{m, l, c, unlit}; }

// ---------------------------------------------------------------- world
struct DecorDef { Vec3 pos; float radius = 3; std::vector<Part> parts; };
struct Brick { uint8_t i, j, k, interior; float x, y, z; Color c; };
struct Building {
  int id = 0; std::string name, landmark; bool school = false, oldtown = false, windows = true; int mask = 0;
  Color color; std::vector<uint32_t> windowCols;
  float x = 0, z = 0; int w = 4, d = 4, h = 3; float minX = 0, maxX = 0, minZ = 0, maxZ = 0, maxY = 0;
  int chunk = 0; std::vector<DecorDef> decor;
  // runtime state (created when the chunk is first streamed in)
  bool generated = false;
  std::vector<int> grid; std::vector<Brick> bricks; std::vector<uint8_t> alive; int aliveCount = 0;
  float lastHit = -1e9f, acc = 0; int ptr = 0;
  bool rebuilding = false, smashed = false, landmarkSmashed = false, collapsed = false, decorDown = false;
  std::vector<int> decorBodies;
  std::vector<Part> rubble;
  float radius = 10; int stamp = 0;
  float health() const { return bricks.empty() ? 1.f : (float)aliveCount / (float)bricks.size(); }
  bool intersects(float cx, float cy, float cz, float r) const;
};
struct Chunk { int cx, cz; float x0, z0; std::vector<int> buildings, props; bool generated = false, active = false; };
struct WaterRect { float x0, x1, z0, z1; };
struct Label { std::string text; Vec3 pos; int kind; };  // 0 landmark, 1 small, 2 water
struct HitResult { int bricks = 0, buildings = 0; bool blocked = false; };
struct World {
  std::vector<Building> buildings; std::vector<Chunk> chunks; std::vector<WaterRect> water; std::vector<Label> labels;
  std::vector<std::pair<Part, int>> statics;  // static props (ferry dock, playground, totem) with their chunk
  std::vector<Part> ground;                   // ground planes, streets, water (flat-colour boxes: the fallback look)
  std::vector<GroundQuad> groundQuads;        // the same surfaces as textured top quads (+ intersection patches)
  int nch = 8; int lastChunk = -1; int queryStamp = 0; float schoolMsgT = 0;
  bool noRebuild = false;  // level flag (L13): smashed buildings stay smashed
  std::vector<int> stoneBuildings, decoyBuildings; uint32_t stoneColor = 0xb04dff; uint32_t stonesFound = 0;  // Atlas: 6 gauntlet-stone hiding spots + 20 same-colour decoys (procedural city)
  bool procedural = false; uint64_t citySeed = 0xC17E5EEDull;  // Atlas: adventure mode uses the invented procedural city (city_gen.cpp); sandbox = Victoria
  std::vector<std::vector<int>> hash; int gn = 32; float cell = 25;
  struct Ferry { Vec3 pos, home, vel; float yaw = 0, spin = 0; std::vector<Part> parts; } ferry;
  void init();
  void buildLayout();
  void generate(Building& b);
  int chunkIndex(float x, float z) const;
  bool isWater(float x, float z) const;
  bool isActiveAt(float x, float z) const { return chunks[chunkIndex(x, z)].active; }
  int activeChunkCount() const;
  void updateStreaming(float x, float z, bool force, const Vec3* second = nullptr);  // second: co-op P2 keeps its own area loaded too
  template <class F> void buildingsNear(float x, float z, float r, F fn);
  HitResult damageSphere(float cx, float cy, float cz, float r, float maxN, float ox, float oz, float impulse);
  int hitBuilding(Building& b, float cx, float cy, float cz, float r, float maxN, float ox, float oz, float impulse);
  void detach(Building& b, int i, float vx, float vy, float vz);
  void afterDamage(Building& b);
  void collapse(Building& b);
  void addRubble(Building& b);
  void finishRebuild(Building& b, bool announce);
  void restoreAll();
  void resolveCircle(Vec3& p, float r);
  bool blockedForPerson(float x, float z);
  void pushFerry(float x, float z, float r, float power);
  bool schoolLocked() const;
  void update(float dt);
  void render(RenderList& rl, const Frustum& fr, const Vec3& eye);
  Building* landmark(const std::string& id);
  // first building / ground hit along a ray (step-marched); returns false if nothing within tmax
  bool rayHit(const Vec3& o, const Vec3& d, float tmin, float tmax, float& tHit);
};

// ---------------------------------------------------------------- props, debris, effects, splats
enum BodyKind { BODY_TREE, BODY_ROCK, BODY_DECOR, BODY_PROP };  // BODY_PROP: street props / parked vehicles (kPropIds)
struct Body {
  bool used = false; int kind = BODY_TREE; std::vector<Part> parts; Color color;
  Vec3 pos, vel, ang; float rx = 0, ry = 0, rz = 0; float radius = 1, mass = 1, scale = 1;
  bool held = false, resting = true, alive = true, thrown = false, lying = false;
  Vec3 homePos; float homeRx = 0, homeRy = 0, homeRz = 0; float respawnAt = 0;
  int prop = -1;  // BODY_PROP: index into kPropIds (model id; the parts are the primitive fallback)
  float groundOffset() const { return kind == BODY_TREE ? (lying ? 0.6f * scale : 0.f) : kind == BODY_ROCK ? 1.0f * scale : kind == BODY_PROP ? 0.f : radius * 0.5f; }
};
enum PropType { PROP_LAMP, PROP_BARREL, PROP_BARRIER, PROP_CONTAINER, PROP_AMBULANCE, PROP_FIRETRUCK, PROP_SEDAN, PROP_TAXI, PROP_VAN, PROP_COUNT };
extern const char* const kPropIds[PROP_COUNT];  // data/models.json ids: "street_light", "barrel", ... "car_van"
struct Bodies {
  std::vector<Body> list;
  int add(const Body& b);
  void remove(int i) { if (i >= 0 && i < (int)list.size()) list[i].used = false; }
  void fling(int i, float vx, float vy, float vz, bool thrown);
  int nearest(float x, float z, float r, bool pickable);
  template <class F> void inRadius(float x, float z, float r, F fn);
  void shatter(int i);
  void update(float dt);
  void render(RenderList& rl);
};
struct Debris {
  static constexpr int N = 2500;
  Vec3 pos[N], vel[N], rot[N], ang[N]; float life[N] = {}, size[N] = {}; Color col[N]; bool alive[N] = {}, sleep[N] = {};
  int next = 0, live = 0;
  void spawn(const Vec3& p, const Vec3& v, Color c, float s);
  void kick(float x, float z, float r, float power);
  void update(float dt);
  void render(RenderList& rl, const Vec3& eye);
  void clear() { for (int i = 0; i < N; i++) alive[i] = false; }
};
struct Effects {
  static constexpr int NP = 900;
  Vec3 pp[NP], pv[NP]; float pl[NP] = {}, pml[NP] = {}, ps[NP] = {}; Color pc[NP]; int pNext = 0;
  struct Item { int type; Vec3 pos; float t, dur, maxR; Color c; };  // 0 ring, 1 shell
  std::vector<Item> items;
  void particle(const Vec3& p, const Vec3& v, float life, float size, Color c);
  void ring(float x, float z, float maxR, float dur, Color c);
  void shell(const Vec3& p, float maxR, float dur, Color c);
  void dust(float x, float z, int n, float r);
  void rainbowBurst(const Vec3& p, float scale = 1);  // little rainbow arc + sparkles (a civilian 'turning into a rainbow')
  void update(float dt);
  void render(RenderList& rl);
  void clear() { items.clear(); for (int i = 0; i < NP; i++) pl[i] = 0; }
};
struct Splat { bool alive = false; float x = 0, z = 0, born = 0, fade = 0; struct Blob { float x, z, r, y; Color c; } blobs[7]; };
struct Splats {
  std::vector<Splat> list; int next = 0, count = 0;
  void init();
  void spawn(float x, float z);
  void fadeNear(float x, float z, float r);
  void fadeAll() { for (auto& s : list) if (s.alive && s.fade <= 0) s.fade = 0.5f; }
  void update(float dt);
  void render(RenderList& rl);
};

// ---------------------------------------------------------------- people
enum PersonState { P_WANDER, P_FLEE, P_TUMBLE, P_DIZZY, P_STUN };
struct Person { bool active = false; float x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0, dir = 0, t = 0, phase = 0, tilt = 0, spin = 0; int state = 0; bool idle = false; int look = 0; float biteT = 0; };  // biteT: Rex crazy-mode zombie bite cooldown
struct People {
  std::vector<Person> list; int activeCount = 0; float respawnT = 0;
  void init();
  void spawnChunk(const Chunk& ch);
  void despawnInactive();
  int pop(float x, float z, float r);
  void popOne(int i);                                  // turn one person into a rainbow (splat + rainbow burst)
  int nearestInRange(float x, float z, float r) const; // nearest grounded active person within r (horizontal), -1 if none
  int stun(float x, float z, float r, float secs);     // knock flat + stun everyone (on the ground) within r; returns count
  int pickRay(const Vec3& o, const Vec3& d, float tmin, float tmax, float radius, float* tHit) const;  // first person along a ray
  static Vec3 center(const Person& p) { return Vec3(p.x, p.y + 1.2f, p.z); }
  void knock(float x, float z, float r, float power);
  void spawnOne(float kx, float kz);
  void update(float dt, float kx, float kz, float kspeed, float kradius);
  void render(RenderList& rl, const Vec3& eye);
  void clear() { for (auto& p : list) p.active = false; }
};

// ---------------------------------------------------------------- police + military
enum UnitType { U_CAR, U_PHELI, U_PTANK, U_JEEP, U_AHELI, U_ATANK, U_JET, U_COP, U_SOLDIER, U_MBOSS, U_RIOT, U_ARMOR, U_FLAME, U_TYPES };  // ladder: riot cops, armoured (super-only) tanks, flamethrowers  // U_COP / U_SOLDIER on foot; U_MBOSS end-of-level boss
struct Unit {
  UnitType type = U_CAR; Vec3 pos, vel; float dir = 0, hp = 1, reload = 0, ang = 0, slowT = 0, t = 0, roll = 0, bombT = 0, hitCd = 0;
  bool wreck = false; int jetState = 0; Vec3 jetDir;
  int tier = 0, boss = 0; float dmgK = 1, speedK = 1, maxHp = 1;  // tier variant multipliers; boss 1 = Giant Super-Tank, 2 = Mega Mech
  uint32_t uid = 0; bool moving = false; float fireT = -1e9f;     // model animation: stable id, walking, last shot (G->time)
  bool air() const { return type == U_PHELI || type == U_AHELI || type == U_JET; }
  bool tank() const { return type == U_PTANK || type == U_ATANK || type == U_MBOSS || type == U_ARMOR; }
  bool infantry() const { return type == U_COP || type == U_SOLDIER || type == U_RIOT || type == U_FLAME; }
  bool armored() const { return type == U_ARMOR; }
  bool enraged = false;                  // end-of-level boss below half HP: fires faster, hits harder
  float flameT = 0, clangT = 0;          // flamethrower burst timer; armour 'clang' hint throttle
  bool police() const { return type == U_COP || type == U_CAR || type == U_PHELI || type == U_PTANK || type == U_RIOT; }
};
uint32_t tierTintHex(int tier); float tierTintAmount(int tier);  // enemy tier palette swap
int bossIdFromName(const std::string& n); const char* bossDisplayName(int b); float bossSpawnHeight(int b); bool bossFlies(int b);  // Titan
bool drawVariedBoss(RenderList& rl, const Unit& u, const Mat4& root);
void unitExtras(RenderList& rl, const Unit& u, const Mat4& root);  // riot shield / flamethrower props (models + primitives)
struct Shot { Vec3 pos, vel; float t = 3, dmg = 5; bool bomb = false, police = false, pea = false; };
struct Enemies {
  std::vector<Unit> units; std::vector<Shot> shots;
  PoliceChase chase; CheeseMeter cheese;  // Titan: police escape/search, boss anti-cheese
  float heat = 0, calmT = 0, spawnT = 0, minHeat = 0; bool forced = false;
  bool nukeActive = false; float nukeT = 0;
  const LevelDef* level = nullptr; float milSpawnT = 0; int spawnRot = 0;
  int destroyed[U_TYPES] = {};
  bool policeOn() const;
  int stars() const { return prog::starsFromHeat(heat); }
  void addHeat(float h);
  void reset();
  int count(UnitType t) const;
  Vec3 spawnPoint(float dist);
  void addUnit(UnitType t);
  void applyTier(Unit& u, int tier);
  static float tierHp(int t), tierDamage(int t), tierSpeed(int t), tierCount(int t);
  Unit* miniBoss();
  void wreckUnit(Unit& u);
  bool updateVariedBoss(Unit& u, float dt, float slow, bool frozen);  // Titan: titan_bosses.cpp
  void hitArea(float x, float z, float r, int moveIdx, bool thrown, float y = 0);  // y = blast height: air units count their height
  static bool shieldBlocks(const Unit& u, const Vec3& from, int moveIdx);   // riot shield: ranged (blaster / breath / beam) hits from the front
  static bool flameHits(const Unit& u, const Vec3& target);                 // flamethrower cone (short range)
  int blocked = 0, clangs = 0;                                               // stats for tests
  void slowArea(float x, float z, float r, float secs);
  bool payNuke();
  void update(float dt, const InputState& in);
  void render(RenderList& rl);
};

// ---------------------------------------------------------------- the player monster
struct Attack { int move = -1; float t = 0, dur = 0, tick = 0; bool hitDone = false, hit2 = false; float power = 1; bool throwAnim = false; };
struct Projectile { int move; Vec3 pos, vel; float t, life; bool lob; };
struct Bolt { int move; Vec3 pos, vel; float t, life; };               // Rainbow Blaster shot
struct EatTween { Vec3 from; float t; int look; };                      // a gulped civilian flying up to the mouth
struct Camera { float yaw = 0, pitch = 0.32f, dist = 34, idle = 99, shake = 0; Vec3 pos, target; };
struct Kaiju {
  int monster = 0; Vec3 pos; float facing = PI, twist = 0, lift = 0, hp = 100, hurtT = 0, hitReactT = 0;  // hitReactT: how long the one-shot flinch clip may play after a burst of damage
  Attack at; std::vector<float> cooldowns; bool charging = false; float charge = 0; int chargeSlot = -1; Action chargeBtn = A_SLOT1;
  int held = -1; float walkPhase = 0, speedNow = 0; bool sprinting = false, animRun = false; bool inWater = false; Camera cam;
  int player = 0;  // 0 = P1, 1 = P2 (travels with the object through the co-op swaps; keys its model animator)
  std::vector<Projectile> projectiles; float spawnGlow = 0;
  std::vector<Bolt> bolts; std::vector<EatTween> eats; float gloveDist = 0; Vec3 gloveLast;
  bool stompAim = false; Action stompAct = A_STOMP; float stompPendingT = 0, stompFlashT = 0;  // stomp: hold to aim (ring), release to stomp
  float lastHitCountT = -1e9f; float aimFlash = 0;
  uint16_t attackSeq = 0, hitSeq = 0;     // bumped per attack / hit taken (restarts the model clip)
  float camScale = -1;                    // smoothed scale() for the camera (pulls back as the kaiju grows)
  float popT = 0;                         // growth spike 'pop' animation (zoo gulp)
  float shrinkK = 1;                      // De-Evolution Ray: shrinks back into the egg (1 = normal)
  const MonsterDef& def() const;
  float scale() const;
  float radius() const;
  float maxHp() const;
  Vec3 fwd() const { return Vec3(std::sin(facing), 0, std::cos(facing)); }
  Vec3 mouth() const;
  Vec3 hand() const;
  float moveRadius(const MoveDef& m) const;
  float moveBricks(const MoveDef& m) const;
  void respawn(float x, float z, float facing);
  void damage(float n);
  bool tryStart(int move);
  float stompStunRadius() const;          // single source for the stomp stun AND the hold-to-aim ground ring
  float eatRange() const;
  bool eat();                             // gulp the nearest civilian in range; false if nobody is close enough
  int projectilesOf(int move) const;      // live projectiles / bolts of a move (rapid weapons are capped)
  void fireBlaster(int move);
  void updateBolts(float dt);
  void updateEats(float dt);
  void update(float dt, const InputState& in);
  void updateAttack(float dt);
  void updateProjectiles(float dt);
  void grabOrThrow();
  HitResult hit(int move, float cx, float cy, float cz, float r, int props, float maxBricks = -1);
  void updateCamera(float dt, bool snap);
  void render(RenderList& rl, float time);
};
// draws a monster rig (all 7 body plans); used for the player, the egg reveal and the character select preview
struct RigPose { float walkPhase = 0, walk = 0, twist = 0, lift = 0, jaw = 0.05f, headX = 0, armR = 0, armL = 0, tailCurl = 0, glow = 0.8f, sink = 0, squash = 1; float time = 0; };
void drawMonster(RenderList& rl, const MonsterDef& m, const Vec3& pos, float yaw, float scale, const RigPose& pose);
void drawMonsterRig(RenderList& rl, const MonsterDef& m, const Mat4& root, const RigPose& pose, Color glow);          // Rex
void drawGeneratedMonster(RenderList& rl, const MonsterDef& m, const Vec3& pos, float yaw, float scale, const RigPose& pose);  // Rex
// primitive-rig finish fallback: tags every instance added since `marks` with the finish id (shader params.z) + time
struct FinishMarks { size_t n[MESH_COUNT]; };
FinishMarks finishMarks(const RenderList& rl);
void applyFinish(RenderList& rl, const FinishMarks& m, int finish, float time);
struct Boss;
void drawBoss(RenderList& rl, const Boss& b, float time);
InputState p2InputFrom(const InputState& in, bool ownCamera = false);  // Titan co-op

// ---------------------------------------------------------------- menus
struct MenuItem { std::string label, right, desc; bool enabled = true; std::function<void()> act; std::function<void(int)> adjust; int icon = 0; Color iconColor{1, 1, 1, 1};
  std::string right2, sub;                 // Controls screen: right = controller, right2 = keyboard. Store tile: sub = "Lv 2/6" / "have 3"
  int price = -1; bool owned = false, afford = true;  // store tiles: coin price (-1 = none), maxed / owned, enough coins
};  // icon: 1 key ring, 2 gauntlet slot (empty outline), 3 gauntlet, 4 cracked gauntlet; store glyphs 5 claws, 6 reach, 7 boot, 8 egg, 9 ray, 10 back arrow
// Grid menus (store): left / right step through the tiles in reading order and wrap end <-> start; up / down move one row
// in the same column and wrap top <-> bottom; landing past the end of a short last row clamps to its last tile.
inline int gridMove(int idx, int n, int cols, int dx, int dy) {
  if (n <= 0 || cols <= 0) return 0;
  idx = std::max(0, std::min(n - 1, idx));
  if (dx) idx = ((idx + dx) % n + n) % n;
  if (dy) { int rows = (n + cols - 1) / cols, r = idx / cols, c = idx % cols; r = ((r + dy) % rows + rows) % rows; idx = std::min(n - 1, r * cols + c); }
  return idx;
}
struct Menu {
  std::string title, body; std::vector<MenuItem> items; int index = 0, scroll = 0;
  std::function<void(Menu&)> build; std::function<void()> onBack; bool noBack = false; bool wide = false; int kind = 0;
  int cols = 0;  // > 0: draw + navigate the items as a tile grid with this many columns (kind 8 = store)
};  // kind: 1 pause, 2 main, 3 characters, 4 prompt, 7 skills, 8 store grid, 9 controls

// ---------------------------------------------------------------- top level
enum class Mode { Intro, Title, Play, Reveal, GameOver, Spin, Ending };
struct Egg { bool active = false; Vec3 pos; int hits = 0; float hitCd = 0, wobble = 0, t = 0; int state = 0; std::string monster; int spin = 0; KaijuLook look; bool hasLook = false; };  // spin: SpinReason that hatched it
struct LevelRun { bool active = false, won = false, failed = false, bossPhase = false, bossDown = false; int idx = 0; float t = 0, elapsed = 0; int count = 0, destroyed = 0; float zoneT = 0;
  int boxBuilding = -1; bool boxShown = false, boxOpen = false; Vec3 boxPos; int arenaTotal = 0, arenaLeft = 0;   // L13
  bool boxKeeper = false; Vec3 boxKeeperPos; float boxKeeperDir = 0;   // L13 without the key: the zookeeper comes to check on his box (gulp him)
  bool stoneTaken = false; Vec3 stonePos; float repelT = 0;                                                    // L14-19
  int placed = 0; Vec3 pedestal[6]; Vec3 gauntletPedestal; bool fightStarted = false;                          // L20
  float noRebuildPrev = 0; };
struct BossRock { Vec3 pos, vel; float t = 0; };
struct Boss {  // THE BULK: a purple hunched brute that grows with every hit it takes
  bool active = false, dead = false; Vec3 pos; float facing = 0, hp = 0, maxHp = 1, scale = 1.6f, pulse = 0, introT = 0, t = 0, deadT = 0;
  int attack = 0; float attackT = 0, nextAttack = 3, hitCd = 0; bool attackHit = false; Vec3 rushDir; int hits = 0; float lift = 0, armSwing = 0;
  std::vector<BossRock> rocks;
  uint16_t attackSeq = 0;                // bumped per attack (restarts the model clip)
  int stage = 1;                         // L20: 1 = gauntlet one-shot, 2 = angry giant (real fight), 3 = FINAL FORM (snap at <= 50%)
  float angerT = 0, transformT = 0;      // stage 1->2 anger beat, 2->3 transformation (invulnerable while > 0)
  bool staged = false;                   // three-stage fight (the L20 gauntlet fight); false = classic single-stage
  float growth() const;                  // scale / starting scale (1 .. bossMaxScale)
  int phase() const;                     // 0 ground-pound, 1 + charging rush, 2 + debris throws (by growth)
  float radius() const;                  // hitbox radius, scales with him
  float height() const;
  bool intro() const { return introT > 0; }
};
struct Toast { std::string text; float t; };
struct GloveDrop { bool active = false; Vec3 pos; float t = 0; };  // the secret glove lying in the rubble
struct Chest { bool active = false; Vec3 pos; std::string move; int state = 0; float t = 0; bool free = false; float drop = 0; };  // free: level-5 supply drop (no key)  // state 0 locked, 1 opening
struct FloatText { std::string text; Vec3 pos; float t; Color c; };
enum SpinReason { SPIN_NONE = 0, SPIN_CREATE = 1, SPIN_TOKEN = 2, SPIN_DEEVO = 3 };
struct ZooRun {  // Kaiju Zoo opening: hatchling in a cage, the zookeeper walks too close, one gulp breaks out
  bool active = false; int state = 0;    // 0 keeper walking in, 1 waiting (E in range), 2 gulped (growth spike), 3 bars shattering
  float t = 0, idleT = 0, stateT = 0; Vec3 center; Vec3 keeper; float keeperDir = 0; bool keeperEaten = false, barsBroken = false; bool thenLevel = false;
  bool inRange = false;
};
struct GateRun { std::string id; Vec3 pos; bool open = false; float openT = 0; int stone = -1; std::string reward; int amount = 0; };
struct EndingRun {  // credits -> THE END -> after-credits video (not skippable) -> any button -> title
  int stage = 0;                          // 0 credits roll, 1 fade + THE END, 2 after-credits video, 3 video done (waiting for a button)
  float t = 0, stageT = 0; bool usedVideo = false, tried = false; float videoLen = 12; bool crazyEnding = false;
};
float mp4DurationSeconds(const std::string& path);  // from the MP4 header (tests + fallback length)
struct CreditLine { std::string role, name, text; bool head = false, thanks = false; };
struct VideoPlayer {  // platform video (Media Foundation on Windows); null in tests / unsupported -> in-engine closing shot
  virtual ~VideoPlayer() {}
  virtual bool start(const std::string& path) = 0;   // false: could not open / decode -> fallback
  virtual void update(float dt) = 0;
  virtual bool finished() const = 0;
  virtual float duration() const = 0;
  virtual void stop() = 0;
  virtual const uint8_t* frame(int& w, int& h, int& stride, uint64_t& serial) const { (void)w; (void)h; (void)stride; (void)serial; return nullptr; }  // current decoded frame
};

struct Game {
  // persistent
  SaveData save; std::function<bool(const std::string&)> saveText;
  // simulation
  World world; Bodies bodies; Debris* debris = nullptr; Effects* fx = nullptr; Splats splats; People people; Enemies enemies; Kaiju kaiju;
  Egg egg; LevelRun level; Boss boss; std::vector<Chest> chests; float chestHintT = 0; GloveDrop gloveDrop; float glovePulseT = 0; int glovePulses = 0;
  float time = 0, realTime = 0; Mode mode = Mode::Title; bool adventure = false; float fps = 60, fpsAcc = 0; int fpsFrames = 0;
  // presentation
  std::vector<Menu> menus;
  std::vector<std::string> pendingOffers;  // swap prompts waiting for the current menu to close
  std::vector<Toast> toasts; std::vector<FloatText> floats;
  std::string bigTop, bigText; float bigT = 0; float flashT = 0; float revealT = 0; float lastScreamT = -10;
  std::vector<SoundEvent> sounds; float loopVol[LOOP_COUNT] = {}; float rumble = 0, rumbleT = 0;
  bool quit = false, wantCapture = false, usingPad = false; std::string gameOverReason;
  float titleOrbit = 0; int previewMonster = 0; float menuCooldown = 0;
  struct MenuLayout { float x = 0, y = 0, w = 0, h = 0; int first = 0, count = 0; int cols = 0; float tw = 0, th = 0, gap = 0; } menuLayout;  // grid: tile w/h + gap, h = row pitch
  float lastCurX = -1, lastCurY = -1; bool mouseCaptured = false;
  Rng rng{12345};
  Game();
  ~Game();
  bool eatTargetNear();  // Rex (rex_play.cpp): something the one eat button would gulp / open right now
  bool crazy = false;  // Crazy difficulty (an adventure run with config multipliers); adventure is also true
  Profile& P() { return adventure ? (crazy ? save.crazy : save.adventure) : save.sandbox; }
  Profile& AP() { return crazy ? save.crazy : save.adventure; }  // the campaign profile being played
  int difficulty = DIFF_EASY, players = 1;  // Titan: Easy/Hard/You're Insane, 1-2 local players
  DiffMods diff() const { return difficultyMods(difficulty, players); }
  float enemyHpMult() const { return (crazy ? C.crazyEnemyHp : 1.f) * diff().enemyHp; }
  Kaiju kaiju2; bool coopJoined = false, coopSwapped = false; float coopNapT = 0;  // Titan co-op (titan_coop.cpp)
  bool coopOn() const; void updateCoop(float dt, const InputState& in); void renderCoop(RenderList& rl);
  bool coopP2Nearer(const Vec3& from) const;  // Wren: P2 is active and closer than P1 to `from`
  bool splitScreen() const;  // 2P split screen: Players = 2, P2 joined, in play (left = P1, right = P2)
  Camera coopHeldCam;        // P2's own camera while coopFocusBegin has P2 swapped into Game::kaiju
  bool coopFocusBegin(const Vec3& from); void coopFocusEnd(bool swapped);  // Wren: temporarily make the nearest player Game::kaiju (enemy / boss targeting)
  float enemyDamageMult() const { return adventure ? C.adventureEnemyDamage * (crazy ? C.crazyEnemyDamage : 1.f) * diff().enemyDamage : diff().enemyDamage; }  // kid-friendly campaign; Crazy = 2x that
  float superCostMult() const { return crazy ? C.crazySuperCost : 1.f; }  // Crazy: multiplies the super EAT COST
  // boss
  void startBoss(const LevelDef& L);
  void updateBoss(float dt); void updateBossFocused(float dt);
  bool bossHitTest(const Vec3& p, float r) const;
  bool hitBossArea(int move, float x, float y, float z, float r, float bricks);  // true if THE BULK took the hit
  void bossVictory();
  void init(const std::string* saveJson);
  bool loadSave(const std::string* saveJson);  // false if the text was damaged (a fresh save is used)
  // 3 save slots (the Windows layer sets `slots`; tests may too). slot -1 = legacy single save via saveText
  const SlotStore* slots = nullptr; int slot = -1;
  bool selectSlot(int n);
  bool deleteSlot(int n);
  void openSlotMenu();
  float tutFinaleT = -1;
  // startup cinematic (skippable). Stages: 0 studio card, 1 city reveal + title, 2 fade out, 3 done
  bool playIntro = false; float introT = 0; int introWarmup = 0; bool introHold = false;  // platform: window not shown/focused yet
  static constexpr float INTRO_CARD = 4.0f, INTRO_REVEAL_END = 12.5f, INTRO_END = 13.5f;
  int introStage() const { return introT < INTRO_CARD ? 0 : introT < INTRO_REVEAL_END ? 1 : introT < INTRO_END ? 2 : 3; }
  void updateIntro(float dt, const InputState& in);
  void finishIntro();
  void drawIntro(RenderList& rl);
  void frame(float dt, const InputState& in);   // update + menus
  // p2view (optional): when splitScreen(), the right half's 3D view is written there and rl becomes the left half
  // (plus all 2D: both HUDs and fullscreen menus). Returns true when it split; 1P always renders fullscreen into rl.
  bool render(RenderList& rl, int w, int h, RenderList* p2view = nullptr);
  void render3D(RenderList& rl, int w, int h);  // one camera's world view (Game::kaiju's camera)
  void persist();
  // events
  void onBricks(int n);
  void onBuildingSmashed(Building& b);
  void onLandmarkSmashed(Building& b);
  void onCollapse(Building& b);
  void onPeoplePopped(int n);
  void onUnitDestroyed(UnitType t);
  void awardCoins(int n, const Vec3& at);
  void schoolLockedHit();
  // feedback
  void play(SoundId id, float vol = 1, float pitch = 1);
  void playAt(SoundId id, const Vec3& p, float vol = 1, float pitch = 1);
  void scream(const Vec3& p);
  // police (cop) sounds: scaled by config policeVolume and capped at policeMaxVoices overlapping one-shots
  std::vector<float> policeVoiceEnds;
  bool playPolice(SoundId id, const Vec3& p, float vol = 1, float pitch = 1);
  void toast(const std::string& s);
  void bigMessage(const std::string& top, const std::string& big);
  void vibrate(float s, float secs) { rumble = std::max(rumble, s); rumbleT = std::max(rumbleT, secs); }
  // progression
  void unlockMove(int idx, bool fromChest = false);
  void offerEquip(const std::string& id);
  void checkUnlocks();
  void checkAdventureGate();
  void tutorialUsed(const std::string& id);
  bool tutorialActive() const { return !adventure && !save.tutorial.done; }
  bool tutorialGoalsMet() const;
  void onKaijuHit();
  // skill chests (locked; one store key each)
  // secret glove (hidden adventure drop)
  float levelSmashFraction() const;       // share of the loaded map's buildings that are smashed
  bool gloveEquipped() const;
  void updateGlove(float dt);
  void collectGlove();
  int glovePulse();                       // rainbow shockwave: collapses the nearest gloveTargets intact buildings; returns how many
  void drawGlove(RenderList& rl, const Mat4& root, float t);
  void spawnChest(const std::string& moveId, bool free = false);
  // eat-to-fuel super attacks
  int superEatCost(const MoveDef& m, float charge = 1) const;  // 0 for non-supers (and the glove)
  bool spendEat(const MoveDef& m, float charge = 1);           // checks + spends; shows 'Need N eaten' if short
  float needEatT = 0; int needEatN = 0;
  bool devSkipGoal = false;  // -devgoal (testing / screenshots)
  bool groundTextured = false;  // set by the platform once the renderer holds the ground textures; false = flat-colour ground boxes
  void openKaijuPick();
  bool kaijuLocked() const;
  void respawnPendingChests();
  void updateChests(float dt);
  bool tryOpenChest(Chest& c);
  Chest* chestNear(float x, float z, float r);
  bool interact();                        // E / pad Y: place a stone, gulp the box zookeeper, open a nearby chest, else gulp the nearest civilian; true if it did something
  bool oneButtonEat(InputState& in);      // E / pad Y: eat or open if anything is in reach and clear the same press's grab (pad Y also reports A_GRAB)
  int buyKey();                           // 0 bought, 1 not enough coins
  // centre-screen aim (Rainbow Blaster crosshair)
  static constexpr float AIM_SCREEN_Y = 0.36f;  // crosshair height as a fraction of the screen (0 = top)
  void aimRay(Vec3& o, Vec3& d) const;
  int aimAir(Vec3& point) const;          // flying enemy near the crosshair ray (soft lock-on so a kid can shoot helicopters), or -1
  int aimTarget(Vec3& point);             // person index under the crosshair (or -1); point = where a shot would go
  void startMode(bool adv, bool crazyMode = false);
  void startLevel(int idx);
  void stopLevel();
  void updateLevel(float dt);
  void spawnEgg();
  void hitEgg(int move, float x, float z, float r);
  void updateEgg(float dt);
  void gameOver(const std::string& reason);
  void restartAfterGameOver();
  void selectMonster(int idx);
  // menus (src/ui/menus.cpp)
  void pushMenu(Menu m);
  void popMenu();
  void openMainMenu();
  void openPause();
  void openStore();
  void openEquip();
  void openMelee();
  void openMoveList();
  void openCharacterSelect(bool fromPause);
  void openInfinityStones();
  bool tryShiny(const std::string& id);          // spend 10 eggs for a 1-in-10 shiny; false if it could not spend
  bool hasInfinity(const char* id) const { return save.infinity.count(id) != 0; }
  void openOptions();
  void openSkills();
  void openControls();
  void prompt(const std::string& title, const std::string& body, std::vector<MenuItem> items, bool noBack = true);
  void updateMenus(const InputState& in, float dt);
  void drawMenus(RenderList& rl);
  void drawMainMenu(UIDraw& u, Menu& m);  // modern left-aligned title screen
  void drawGridMenu(UIDraw& u, Menu& m);  // store icon grid (Menu::cols > 0)
  void drawHud(RenderList& rl);
  // Vault: menu style + bottom HUD bar (src/ui/ui_style.*, src/ui/hud_bar.*)
  float menuAnim(const Menu& m); std::string menuAnimKey; float menuAnimStart = 0, menuPressT = 0;
  void drawHudBar(UIDraw& u); bool hudBarClick(float x, float y); float lastW = 1280, lastH = 720;  // returns true if a bar button consumed the click
  vpow::State vault;                                   // Vault: kaiju powers + cheats (TODO Wren: persist via SaveData)
  struct PowerSpin { bool active = false, shown = false; float t = 0; int target = -1, laps = 3; } pspin;
  void openPowerSpinner(); void drawPowerSpin(UIDraw& u, Menu& m); void updatePowerSpin(float dt); void openCheats(); void vaultTick();
  void addKaijuPowerTiles(Menu& mm);  // store: the 5 kaiju-specific upgradable powers
  std::string tutorialText(bool& isTutorial);
  std::string levelHud();
  std::string btn(const char* pad, const char* key) const { return usingPad ? pad : key; }
  // loaded 3D models (src/game/models.cpp). `models` is the platform's catalog (null in tests and with -nomodels);
  // every helper returns false when there is no model so the caller draws its primitive rig as before.
  ModelCatalog* models = nullptr;
  bool modelsOn() const { return models && C.modelsEnabled > 0.5f; }
  int modelAsset(const std::string& id);                  // -1 if none (cached per catalog)
  int modelVariant(int asset, const std::string& name);
  bool monsterModel(RenderList& rl, const MonsterDef& m, const Vec3& pos, float yaw, float scale, ModelReq r);
  bool kaijuModel(RenderList& rl, float time);
  bool bossModel(RenderList& rl, const Boss& b);
  bool unitModel(RenderList& rl, const Unit& u, const Mat4& root);
  bool personModel(RenderList& rl, const Person& p, int index, const Vec3& eye);
  bool buildingModel(RenderList& rl, const Building& b);
  bool bodyModel(RenderList& rl, const Body& b, int index, const Mat4& W);
  struct ModelCache { ModelCatalog* owner = nullptr; std::map<std::string, int> ids; std::map<std::pair<int, std::string>, int> variants; std::vector<int> buildings; } modelCache;
  float lastAnimT = -1; Vec3 lastEye{-1e30f, 0, 0};
  // ---- eggs, looks, growth (src/game/features.cpp)
  Spinner spinner; int spinReason = SPIN_NONE; uint64_t spinSeed = 0; std::string spinNote; bool spinDup = false;
  KaijuLook lookOf(const std::string& id) const;          // rolled look, or the classic default
  bool hasLook(const std::string& id) const { return save.looks.count(id) > 0; }
  MonsterDef lookedDef(int idx) const;                    // monster def recoloured with its look (primitive rigs)
  Color uiAccent() const;                                 // current kaiju's accent (HUD bars / frames)
  const RarityDef& rarityOf(const std::string& id) const;
  bool beginSpin(int reason);                             // false if no token (SPIN_TOKEN)
  KaijuLook decideSpin(int reason);
  bool applySpinResult(const KaijuLook& l, int reason);   // true = new kaiju, false = duplicate (coins + XP + finish upgrade)
  void updateSpin(float dt, const InputState& in);
  void drawSpinner(RenderList& rl);
  void grantBonusSuper();
  float kaijuXp() const;                                  // growth XP of the active kaiju in this mode (full if none)
  float growthFrac() const;
  void addXp(float xp, const Vec3* at = nullptr);
  float healKaiju(float hp);                              // heals up to max; the overflow becomes XP (returns XP gained)
  float xpPopAcc = 0, xpPopT = 0, heartPopAcc = 0, heartPopT = 0;
  void tickPops(float dt);
  // De-Evolution Ray
  float deevoT = -1; Vec3 deevoPos;
  bool canDeevo(std::string* why = nullptr);
  void openDeevoConfirm();
  bool buyDeevo();
  void updateDeevo(float dt);
  void renderDeevo(RenderList& rl);
  // Kaiju Zoo opening + Zookeeper's Key + Zoo Gates
  ZooRun zoo;
  bool zooEligible() const;
  void startZoo(bool thenLevel);
  void updateZoo(float dt, const InputState& in);
  void skipZoo();
  void finishZoo();
  void renderZoo(RenderList& rl);
  void drawZookeeper(RenderList& rl, const Vec3& at, float dir, bool walking);
  void grantZookeeperKey();                               // silent: no toast, no message
  std::vector<GateRun> gates;
  void setupGates();
  bool stoneRevealed(int stone);  // Atlas (atlas_stones.cpp): procedural city hides stone gates inside one purple building until it is smashed
  void updateGates(float dt);
  void openGate(GateRun& g);
  void renderGates(RenderList& rl);
  Vec3 openSpot(float x, float z, float r) ;              // nearest unblocked ground near (x, z)
  // ---- gauntlet arc + three-stage Bulk + ending (src/game/arc.cpp)
  bool gauntletReady() const;                             // gauntlet owned + equipped (gauntlet slot)
  int stoneCount() const;
  void equipGauntlet(bool on);
  // Infinity Stones: pick an owned stone and assign each of its powers to a different action
  // (0 pulse, 1 strike, 2 blast, 3 roar). Empty powerId clears that action.
  bool assignStonePower(int channel, const std::string& stoneId, const std::string& powerId);
  bool stonePowerOn(int channel, StonePower& out) const;  // equipped gauntlet + owned stone + a matching bind
  float stoneBrickMul(int moveKind) const;
  float stoneRangeMul(int moveKind) const;
  void onSuperFired();
  std::string stoneBindSummary() const;
  void openStoneMenu();
  void openStonePowers(int stoneIndex);
  void setupArcLevel(const LevelDef& L);
  void updateArc(float dt);
  void renderArc(RenderList& rl);
  bool tryPickStone();
  void repelKaiju(const Vec3& from);
  bool tryPlaceStone();
  bool tryGulpBoxKeeper();
  void startBulkFight();
  float skyDark = 0, skyDarkTarget = 0; int skyDarkEvents = 0;
  bool bulkGauntletHit();                                 // stage-1 one-shot (true if it happened)
  bool snapReady() const;                                 // stage 3 at <= 50% HP + gauntlet with 6 stones
  bool doSnap();
  float snapT = -1; float ultCdT = 0; int sandboxUlts = 0;
  bool sandboxUltimate();                                 // Future Sandbox: the rainbow wave
  void updateSnap(float dt);
  void finishSnap();
  void restartBulkStage2();
  EndingRun ending; std::vector<CreditLine> credits; std::string afterSign = "Do not feed people.", afterVideo = "data/video/after_credits.mp4";
  float endHold = 2.5f, endFallback = 12;
  VideoPlayer* video = nullptr;
  void loadCredits(const std::string& json);
  void startEnding(bool crazyRun);
  void updateEnding(float dt, const InputState& in);
  void drawEnding(RenderList& rl);
  void renderClosingShot(RenderList& rl, float t);
  float endingSceneTime() const; float endingZoom() const; bool signRect(float& x0, float& y0, float& x1, float& y1) const;
  float signOverlayAlpha() const;   // 0..1 sign overlay (0:00-0:07)
  float signCaptionAlpha() const;   // 0..1 caption (0:07 - end)
  bool closingShotVisible() const { return mode == Mode::Ending && ending.stage >= 2; }
};
extern Game* G;
// load config + tables: from data folder text if given, else the embedded copies; returns error text or ""
std::string embeddedJson(const char* name);  // "config", "moves", "monsters", "levels"
std::string loadGameData(const std::string& cfg, const std::string& moves, const std::string& monsters, const std::string& levels, const std::string& eggs = "");  // eggs "" = embedded
}  // namespace tz

// ---------------------------------------------------------------- template helpers
namespace tz {
template <class F> void World::buildingsNear(float x, float z, float r, F fn) {
  queryStamp++;
  float H = C.worldHalf;
  int a0 = std::max(0, (int)std::floor((x - r + H) / cell)), a1 = std::min(gn - 1, (int)std::floor((x + r + H) / cell));
  int b0 = std::max(0, (int)std::floor((z - r + H) / cell)), b1 = std::min(gn - 1, (int)std::floor((z + r + H) / cell));
  for (int a = a0; a <= a1; a++) for (int b = b0; b <= b1; b++) {
    for (int id : hash[a * gn + b]) {
      Building& bd = buildings[id];
      if (!bd.generated || !chunks[bd.chunk].active || bd.stamp == queryStamp) continue;
      bd.stamp = queryStamp; fn(bd);
    }
  }
}
template <class F> void Bodies::inRadius(float x, float z, float r, F fn) {
  for (int i = 0; i < (int)list.size(); i++) {
    Body& b = list[i];
    if (!b.used || !b.alive || b.held) continue;
    float d = dist2D(b.pos.x, b.pos.z, x, z);
    if (d < r + b.radius) fn(i, d);
  }
}
}  // namespace tz
