#include "json.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
namespace tz {
static const Json kNull;
const Json& Json::operator[](const std::string& k) const { if (type != Object) return kNull; auto it = obj.find(k); return it == obj.end() ? kNull : it->second; }
const Json& Json::operator[](size_t i) const { return (type == Array && i < arr.size()) ? arr[i] : kNull; }

namespace {
struct Parser {
  const std::string& s; size_t i = 0; std::string err;
  explicit Parser(const std::string& t) : s(t) {}
  void ws() {
    for (;;) {
      while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
      if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') { while (i < s.size() && s[i] != '\n') i++; continue; } // allow // comments in data files
      break;
    }
  }
  bool fail(const char* m) { if (err.empty()) { err = std::string(m) + " at offset " + std::to_string(i); } return false; }
  bool str(std::string& out) {
    if (s[i] != '"') return fail("expected string"); i++;
    while (i < s.size() && s[i] != '"') {
      char c = s[i++];
      if (c == '\\' && i < s.size()) {
        char e = s[i++];
        switch (e) { case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break; case 'b': out += '\b'; break; case 'f': out += '\f'; break;
          case 'u': { if (i + 4 > s.size()) return fail("bad unicode"); unsigned cp = (unsigned)strtoul(s.substr(i, 4).c_str(), nullptr, 16); i += 4;
            if (cp < 0x80) out += (char)cp; else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 63)); } else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 63)); out += (char)(0x80 | (cp & 63)); } break; }
          default: out += e; }
      } else out += c;
    }
    if (i >= s.size()) return fail("unterminated string"); i++; return true;
  }
  bool value(Json& v, int depth) {
    if (depth > 64) return fail("too deep");
    ws(); if (i >= s.size()) return fail("unexpected end");
    char c = s[i];
    if (c == '{') {
      v = Json::makeObj(); i++; ws();
      if (i < s.size() && s[i] == '}') { i++; return true; }
      for (;;) {
        ws(); std::string k; if (i >= s.size() || !str(k)) return fail("expected key");
        ws(); if (i >= s.size() || s[i] != ':') return fail("expected ':'"); i++;
        Json child; if (!value(child, depth + 1)) return false; v.obj[k] = std::move(child);
        ws(); if (i < s.size() && s[i] == ',') { i++; ws(); if (i < s.size() && s[i] == '}') { i++; return true; } continue; }
        if (i < s.size() && s[i] == '}') { i++; return true; }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      v = Json::makeArr(); i++; ws();
      if (i < s.size() && s[i] == ']') { i++; return true; }
      for (;;) {
        Json child; if (!value(child, depth + 1)) return false; v.arr.push_back(std::move(child));
        ws(); if (i < s.size() && s[i] == ',') { i++; ws(); if (i < s.size() && s[i] == ']') { i++; return true; } continue; }
        if (i < s.size() && s[i] == ']') { i++; return true; }
        return fail("expected ',' or ']'");
      }
    }
    if (c == '"') { v = Json::makeStr(""); return str(v.str); }
    if (s.compare(i, 4, "true") == 0) { v = Json::makeBool(true); i += 4; return true; }
    if (s.compare(i, 5, "false") == 0) { v = Json::makeBool(false); i += 5; return true; }
    if (s.compare(i, 4, "null") == 0) { v = Json(); i += 4; return true; }
    if (c == '-' || (c >= '0' && c <= '9')) {
      const char* start = s.c_str() + i; char* end = nullptr; double d = strtod(start, &end);
      if (end == start) return fail("bad number"); i += (size_t)(end - start); v = Json::makeNum(d); return true;
    }
    return fail("unexpected character");
  }
};
void esc(std::string& o, const std::string& s) {
  o += '"';
  for (unsigned char c : s) { if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n"; else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o += (char)c; }
  o += '"';
}
void dumpTo(const Json& j, std::string& o, int ind, int lvl) {
  auto nl = [&](int l) { if (ind) { o += '\n'; o.append((size_t)(l * ind), ' '); } };
  switch (j.type) {
    case Json::Null: o += "null"; break;
    case Json::Bool: o += j.b ? "true" : "false"; break;
    case Json::Number: { char b[32]; if (j.num == (double)(long long)j.num && std::abs(j.num) < 1e15) snprintf(b, sizeof b, "%lld", (long long)j.num); else snprintf(b, sizeof b, "%.6g", j.num); o += b; break; }
    case Json::String: esc(o, j.str); break;
    case Json::Array: { o += '['; bool f = true; for (auto& c : j.arr) { if (!f) o += ','; f = false; nl(lvl + 1); dumpTo(c, o, ind, lvl + 1); } if (!j.arr.empty()) nl(lvl); o += ']'; break; }
    case Json::Object: { o += '{'; bool f = true; for (auto& kv : j.obj) { if (!f) o += ','; f = false; nl(lvl + 1); esc(o, kv.first); o += ind ? ": " : ":"; dumpTo(kv.second, o, ind, lvl + 1); } if (!j.obj.empty()) nl(lvl); o += '}'; break; }
  }
}
}  // namespace

bool Json::parse(const std::string& text, Json& out, std::string* err) {
  Parser p(text); Json v;
  if (!p.value(v, 0)) { if (err) *err = p.err; return false; }
  p.ws(); if (p.i != text.size()) { if (err) *err = "trailing characters at offset " + std::to_string(p.i); return false; }
  out = std::move(v); return true;
}
std::string Json::dump(int indent) const { std::string o; dumpTo(*this, o, indent, 0); return o; }
bool readTextFile(const std::string& path, std::string& out) { std::ifstream f(path, std::ios::binary); if (!f) return false; std::stringstream ss; ss << f.rdbuf(); out = ss.str(); return true; }
bool writeTextFile(const std::string& path, const std::string& text) {
  std::string tmp = path + ".tmp";
  { std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) return false; f << text; if (!f.good()) return false; }
  std::remove(path.c_str());
  return std::rename(tmp.c_str(), path.c_str()) == 0;
}
}  // namespace tz
