/* The messages that name the Cesium form of a habit from C and other
 * languages (cesiumlang/cesium#82), where the checker would otherwise say
 * only "unknown type" or "unknown name". The parser's are in s0_parse.c;
 * stage1/habit.csm has the same. */
#include "s0.h"

static bool is_one_of(const char *name, const char *const *names, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    if (strcmp(names[i], name) == 0) {
      return true;
    }
  }
  return false;
}

bool s0_habit_type(S0Diag *diag, S0Loc loc, const char *name) {
  static const char *const ints[] = {"int", "long", "short", "char", "unsigned", "signed", "size_t"};
  static const char *const floats[] = {"float", "double"};
  if (is_one_of(name, ints, sizeof ints / sizeof ints[0])) {
    s0_error(diag, loc, "E0320", "unknown type '%s'; the integer types of S0 are u8, i32, u32, i64, u64 and uword",
             name);
    return true;
  }
  if (strcmp(name, "bool") == 0) {
    s0_error(diag, loc, "E0321",
             "unknown type 'bool'; there is no bool type: a condition is an integer, nonzero is true");
    return true;
  }
  if (is_one_of(name, floats, sizeof floats / sizeof floats[0])) {
    s0_error(diag, loc, "E0322", "unknown type '%s'; S0 has no floating point", name);
    return true;
  }
  return false;
}

bool s0_habit_name(S0Diag *diag, S0Loc loc, const char *name) {
  if (strcmp(name, "true") == 0 || strcmp(name, "false") == 0) {
    s0_error(diag, loc, "E0323", "unknown name '%s'; true and false are not decided for S0: write 1 or 0", name);
    return true;
  }
  return false;
}
