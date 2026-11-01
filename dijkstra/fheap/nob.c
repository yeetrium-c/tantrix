#include <stdbool.h>

#include "nob.h"
#include "flag.h"

#define SRCDIR "src/"
#define ODIR SRCDIR "obj/"
#define MDIR SRCDIR "main/"
#define IDIR "include/"
#define BINDIR "bin/"
#define LDIR "lib/"

Cmd cmd = {0};
File_Paths deps = {0};
bool test = false;

bool cc_flags(Cmd* c)
{
  cmd_append(c, "-Wall");
  cmd_append(c, "-Wextra");
  cmd_append(c, "-pedantic");

  cmd_append(c, "-ggdb");

  return true;
}

bool i_flags(Cmd* c)
{
  cmd_append(c, "-I" IDIR);
  cmd_append(c, "-I.");

  return true;
}

bool l_flags(Cmd* c)
{
  cmd_append(c, "-L" LDIR);
  cmd_append(c, "-lm");
  return true;
}

bool deps_append_file(Nob_Walk_Entry entry);
bool c_to_o(Nob_Walk_Entry entry);
bool build_main(const char* const name);
bool build_lib(const char* const name);

int main(int argc, char** argv)
{
  NOB_GO_REBUILD_URSELF(argc, argv);

  flag_bool_var(&test, "test", false, "build and run the test");

  flag_parse(argc, argv);

  if (!mkdir_if_not_exists(BINDIR)) return 1;
  if (!mkdir_if_not_exists(ODIR)) return 1;
  if (!mkdir_if_not_exists(LDIR)) return 1;

  da_append(&deps, "nob.c");
  if (!walk_dir(IDIR, deps_append_file)) return 1;

  if (!walk_dir(SRCDIR, c_to_o)) return 1;

  if (!build_lib("fheap")) return 1;

  if (test)
  {
    cmd_append(&cmd, "gcc", MDIR "test.c", "-o", BINDIR "test");
    cc_flags(&cmd);
    i_flags(&cmd);
    l_flags(&cmd);
    cmd_append(&cmd, "-lfheap");
    if (!cmd_run(&cmd)) return 1;

    cmd_append(&cmd, BINDIR "test");
    if (!cmd_run(&cmd)) return 1;
  }

  // leak deps
  return 0;
}

// returns true iff the entry is a file
bool do_not_enter_dirs(Nob_Walk_Entry entry)
{
  if (entry.type == NOB_FILE_DIRECTORY)
  {
    if (entry.level > 0)
      *entry.action = NOB_WALK_SKIP;
    return false;
  }
  return true;
}

bool deps_append_file(Nob_Walk_Entry entry)
{
  if (!do_not_enter_dirs(entry)) return true;

  da_append(&deps, strdup(entry.path));
  return true;
}

bool cmd_append_file(Nob_Walk_Entry entry)
{
  if (!do_not_enter_dirs(entry)) return true;

  cmd_append(&cmd, strdup(entry.path));
  return true;
}

bool c_to_o(Nob_Walk_Entry entry)
{
  if (!do_not_enter_dirs(entry)) return true;

  char buff[256] = {0};

  // extract just the file nmae
  char* s = (char *)entry.path;
  while (*s) ++s;
  --s; // walk back the '\0'
  --s; // walk back the 'c' file extension
  char* save = s;
  *s = '\0';
  while (*s != '/') --s;
  ++s; // wald forward the '/'

  snprintf(buff, 255, "%s%s.o", ODIR, s);

  *save = '.'; // restore the file extension

  // no need to strdup it because we release it right next after
  da_append(&deps, entry.path);

  bool rebuild = !needs_rebuild(buff, deps.items, deps.count);

  // remove the last element
  // no need to free it, as it was allocated by walk_dir
  --deps.count;

  if (rebuild)
  {
    nob_log(NOB_INFO, "Nothing to be done for %s", entry.path);
    return true;
  }

  // compile the file
  Cmd c = {0};
  cmd_append(&c, "gcc");
  cmd_append(&c, entry.path);
  cmd_append(&c, "-c");
  cmd_append(&c, "-o", buff);
  (void) cc_flags(&c);
  (void) i_flags(&c);

  if (!cmd_run(&c)) return false;

  return true;
}

bool build_main(const char* const name)
{
  da_append(&deps, temp_sprintf(MDIR"%s.c", name));

  if (needs_rebuild(temp_sprintf(MDIR "%s", name), deps.items, deps.count))
  {
    // .os -> elf
    cmd_append(&cmd, "gcc");
    if (!walk_dir(ODIR, cmd_append_file)) return false;
    cmd_append(&cmd, temp_sprintf(MDIR"%s.c", name));
    cmd_append(&cmd, "-o", temp_sprintf(BINDIR "%s", name));

    cc_flags(&cmd);
    i_flags(&cmd);
    l_flags(&cmd);

    if (!cmd_run(&cmd)) return false;
    nob_log(INFO, "%s build successfully", name);
  }
  else
    nob_log(INFO, "Nothing to be done for %s", name);

  // remove the file we just added
  // it does not need to be free as it was not strduped
  --deps.count;

  return true;
}

bool build_lib(const char* const name)
{
  da_append(&deps, temp_sprintf(LDIR"lib%s.a", name));

  if (needs_rebuild(temp_sprintf(MDIR "%s", name), deps.items, deps.count))
  {
    // .os -> .a
    cmd_append(&cmd, "ar", "rcs");
    cmd_append(&cmd, temp_sprintf(LDIR "lib%s.a", name));
    if (!walk_dir(ODIR, cmd_append_file)) return false;

    if (!cmd_run(&cmd)) return false;
    nob_log(INFO, "lib%s build successfully", name);
  }
  else
    nob_log(INFO, "Nothing to be done for lib%s", name);

  // remove the file we just added
  // it does not need to be free as it was not strduped
  --deps.count;

  return true;
}

#define NOB_IMPLEMENTATION
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"
