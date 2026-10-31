// Minimal JSON value + parser + writer (enough for config, data tables and save files).
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace tz {
struct Json {
  enum Type { Null, Bool, Number, String, Array, Object } type = Null;
  bool b = false; double num = 0; std::string str;
  std::vector<Json> arr; std::map<std::string, Json> obj;
  Json() = default;
  static Json makeNum(double v) { Json j; j.type = Number; j.num = v; return j; }
  static Json makeBool(bool v) { Json j; j.type = Bool; j.b = v; return j; }
  static Json makeStr(const std::string& v) { Json j; j.type = String; j.str = v; return j; }
  static Json makeArr() { Json j; j.type = Array; return j; }
  static Json makeObj() { Json j; j.type = Object; return j; }
  const Json& operator[](const std::string& k) const;
  const Json& operator[](size_t i) const;
  Json& set(const std::string& k, Json v) { type = Object; obj[k] = std::move(v); return obj[k]; }
  bool has(const std::string& k) const { return type == Object && obj.count(k) > 0; }
  double numOr(double d) const { return type == Number ? num : (type == Bool ? (b ? 1 : 0) : d); }
  std::string strOr(const std::string& d) const { return type == String ? str : d; }
  bool boolOr(bool d) const { return type == Bool ? b : (type == Number ? num != 0 : d); }
  size_t size() const { return type == Array ? arr.size() : (type == Object ? obj.size() : 0); }
  std::string dump(int indent = 0) const;
  static bool parse(const std::string& text, Json& out, std::string* err = nullptr);
};
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
}  // namespace tz
