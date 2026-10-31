#include "tables.h"
#include <algorithm>
#include <cstdlib>
#include "../core/json.h"
#include "config.h"
namespace tz {
Tables T;
static Color parseHex(const Json& j, Color d) { if (j.type != Json::String) return d; return hexColor((uint32_t)strtoul(j.str.c_str(), nullptr, 16)); }
std::string MoveDef::unlockText() const {
  switch (unlock) {
    case UnlockType::Start: return "Starter move";
    case UnlockType::Buildings: return "Smash " + std::to_string(unlockCount) + " buildings";
    case UnlockType::Landmark: {
      const char* n = unlockLandmark == "empress" ? "the Empress Hotel" : unlockLandmark == "parliament" ? "the Parliament Buildings" : unlockLandmark == "museum" ? "the Royal BC Museum"
                    : unlockLandmark == "ferry" ? "the Ferry Terminal" : unlockLandmark == "chinatown" ? "the Chinatown Gate" : unlockLandmark == "school" ? "Ruth King Elementary" : unlockLandmark.c_str();
      return std::string("Smash ") + n;
    }
    case UnlockType::Hidden: return "???";
    case UnlockType::Milestone: {
      int n = (int)C.roarUnlockLandmarks;
      return std::string("Smash ") + (n == 1 ? "any landmark" : std::to_string(n) + " landmarks") + " or survive " + std::to_string((int)C.roarUnlockHits) + " hits";
    }
  }
  return "";
}
int Tables::moveIndex(const std::string& id) const { for (size_t i = 0; i < moves.size(); i++) if (moves[i].id == id) return (int)i; return -1; }
int Tables::monsterIndex(const std::string& id) const { for (size_t i = 0; i < monsters.size(); i++) if (monsters[i].id == id) return (int)i; return -1; }

static MoveShape shapeOf(const std::string& s) {
  static const char* n[] = {"front", "rear", "around", "spin", "dash", "cone", "beam", "projectile", "lob", "roar", "quake", "meteor", "nova", "tornado", "blaster", "glove"};
  for (int i = 0; i < (int)(sizeof n / sizeof n[0]); i++) if (s == n[i]) return (MoveShape)i;
  return MoveShape::Front;
}
bool Tables::load(const std::string& mj, const std::string& nj, const std::string& lj, std::string* err) {
  Json a, b, c;
  if (!Json::parse(mj, a, err) || !Json::parse(nj, b, err) || !Json::parse(lj, c, err)) return false;
  moves.clear(); monsters.clear(); levels.clear();
  for (auto& m : a.arr) {
    MoveDef d; d.id = m["id"].strOr("?"); d.name = m["name"].strOr(d.id); d.desc = m["desc"].strOr(""); d.sound = m["sound"].strOr(d.id);
    std::string k = m["kind"].strOr("melee"); d.kind = k == "direct" ? MoveKind::Direct : k == "super" ? MoveKind::Super : MoveKind::Melee;
    d.shape = shapeOf(m["shape"].strOr("front"));
    d.radius = (float)m["radius"].numOr(6); d.reach = (float)m["reach"].numOr(6); d.bricks = (float)m["bricks"].numOr(10); d.cooldown = (float)m["cooldown"].numOr(1);
    d.impulse = (float)m["impulse"].numOr(15); d.duration = (float)m["duration"].numOr(0.5); d.speed = (float)m["speed"].numOr(50); d.chargeTime = (float)m["chargeTime"].numOr(0);
    d.color = parseHex(m["color"], hexColor(0x3ad7ff)); d.glow = m["glow"].boolOr(false); d.slows = m["slows"].boolOr(false);
    std::string u = m["unlock"]["type"].strOr("start");
    d.unlock = u == "buildings" ? UnlockType::Buildings : u == "landmark" ? UnlockType::Landmark : u == "milestone" ? UnlockType::Milestone : u == "hidden" ? UnlockType::Hidden : UnlockType::Start;
    d.unlockCount = (int)m["unlock"]["count"].numOr(0); d.unlockLandmark = m["unlock"]["landmark"].strOr("");
    d.rapid = m["rapid"].boolOr(false); d.chest = m["chest"].boolOr(false);
    int defSlot = d.id == "stomp" ? 0 : (d.kind == MoveKind::Melee || d.shape == MoveShape::Cone) ? 1 : 2;
    d.slot = (int)m["slot"].numOr(defSlot); if (d.slot < 0 || d.slot > 2) d.slot = defSlot;
    moves.push_back(d);
  }
  for (auto& m : b.arr) {
    MonsterDef d; d.id = m["id"].strOr("?"); d.name = m["name"].strOr(d.id); d.desc = m["desc"].strOr("");
    d.variant = m["variant"].strOr("");
    std::string p = m["plan"].strOr("biped");
    d.plan = p == "quad" ? BodyPlan::Quad : p == "blob" ? BodyPlan::Blob : p == "crab" ? BodyPlan::Crab : p == "bird" ? BodyPlan::Bird : p == "robot" ? BodyPlan::Robot : p == "serpent" ? BodyPlan::Serpent : BodyPlan::Biped;
    d.body = parseHex(m["body"], hexColor(0x12141a)); d.accent = parseHex(m["accent"], hexColor(0x3ad7ff)); d.belly = parseHex(m["belly"], hexColor(0x3a3f4c));
    d.scale = (float)m["scale"].numOr(1); d.speed = (float)m["speed"].numOr(1); d.hp = (float)m["hp"].numOr(1); d.melee = (float)m["melee"].numOr(1); d.power = (float)m["power"].numOr(1);
    d.starter = m["starter"].boolOr(false);
    monsters.push_back(d);
  }
  for (auto& m : c.arr) {
    LevelDef d; d.id = m["id"].strOr("?"); d.name = m["name"].strOr(d.id); d.desc = m["desc"].strOr("");
    const Json& o = m["objective"]; std::string t = o["type"].strOr("smash");
    d.obj = t == "survive" ? ObjType::Survive : t == "reach" ? ObjType::Reach : t == "destroy" ? ObjType::Destroy : t == "landmarks" ? ObjType::Landmarks : t == "boss" ? ObjType::Boss : t == "box" ? ObjType::Box : t == "stone" ? ObjType::Stone : ObjType::Smash;
    d.count = (int)o["count"].numOr(0); d.time = (float)o["time"].numOr(0); d.x = (float)o["x"].numOr(0); d.z = (float)o["z"].numOr(0); d.r = (float)o["r"].numOr(25);
    d.filter = o["filter"].strOr(""); d.unit = o["unit"].strOr("any");
    for (auto& l : o["landmarks"].arr) d.landmarks.push_back(l.strOr(""));
    if (o["zone"].size() == 3) { d.hasZone = true; d.zx = (float)o["zone"][0].numOr(0); d.zz = (float)o["zone"][1].numOr(0); d.zr = (float)o["zone"][2].numOr(50); }
    d.startX = (float)m["start"][0].numOr(0); d.startZ = (float)m["start"][1].numOr(0);
    d.bossX = (float)m["boss"][0].numOr(d.startX); d.bossZ = (float)m["boss"][1].numOr(d.startZ - 60);
    const Json& e = m["enemies"];
    d.cops = (int)e["cops"].numOr(0); d.cars = (int)e["cars"].numOr(0); d.soldiers = (int)e["soldiers"].numOr(0);
    d.tier = std::max(0, std::min(2, (int)m["tier"].numOr(0))); d.miniBoss = m["miniboss"].strOr(""); d.chestDrop = m["chests"].boolOr(false);
    d.jeeps = (int)e["jeeps"].numOr(0); d.helis = (int)e["helis"].numOr(0); d.tanks = (int)e["tanks"].numOr(0); d.jets = (int)e["jets"].numOr(0); d.interval = (float)e["interval"].numOr(5);
    d.reward = (int)m["reward"].numOr(100);
    d.riots = (int)e["riots"].numOr(0); d.armored = (int)e["armored"].numOr(0); d.flamers = (int)e["flamers"].numOr(0);
    d.noRebuild = m["noRebuild"].boolOr(false); d.unlockGauntletSlot = m["unlockGauntletSlot"].boolOr(false); d.pedestals = m["pedestals"].boolOr(false);
    d.stone = (int)o["stone"].numOr(-1);
    if (o["arena"].size() == 3) { d.ax = (float)o["arena"][0].numOr(0); d.az = (float)o["arena"][1].numOr(0); d.ar = (float)o["arena"][2].numOr(60); }
    levels.push_back(d);
  }
  if (moves.empty() || monsters.empty() || levels.empty()) { if (err) *err = "a data table is empty"; return false; }
  return true;
}
}  // namespace tz
