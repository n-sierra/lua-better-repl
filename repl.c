#include <stdio.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <readline/readline.h>
#include <readline/history.h>

lua_State *g_state;
char **g_cands;
int g_quit;

void err(lua_State *L);

int cf_quit(lua_State *L);
int cf_ls(lua_State *L);

char** repl_completion(const char* text, int start, int end);
char* repl_match_generator(const char* text, int state);

char** make_words(lua_State *L);
char** make_words_table(lua_State *L, const char *text, int start, int end);

char** make_cands(lua_State *L, const char *prefix, int (*filter)(lua_State*));
int is_key_string(lua_State *L);

void xfree(void *obj);
void xfree_array(char **array);

int main(int argc, char **argv) {
  char *line;
  lua_State *L;
  int error_load;

  // initialize lua
  L = luaL_newstate();
  luaL_openlibs(L);
  lua_register(L, "quit", cf_quit);
  lua_register(L, "ls", cf_ls);

  // initial setting for gnu readline
  rl_readline_name = "repl";
  rl_basic_word_break_characters = " \t\n\"+-*/^%><=|;,{(#";
  rl_attempted_completion_function = repl_completion;

  g_state = L;
  g_cands = NULL;
  g_quit = 0;

  if(2 <= argc) {
    // exec argv[1]
    lua_getglobal(L, "dofile");
    lua_pushstring(L, argv[1]);

    if(lua_pcall(L, 1, 0, 0)) {
      err(L);
    }
  }

  while(g_quit == 0) {
    // read
    line = readline("lua> ");

    // eval
    error_load = 0;
    if(luaL_loadstring(L, line)) {
      char *p;
      p = (char*)malloc(strlen(line)*sizeof(char)+sizeof("return "));
      if(!p) {
        lua_pop(L, 1);
        lua_pushstring(L, "memory allocate error");
        error_load = 1;
      } else {
        sprintf(p, "return %s", line);
        if(luaL_loadstring(L, p)) {
          lua_pop(L, 1);
          error_load = 1;
        } else {
          lua_remove(L, lua_gettop(L)-1);
        }
        xfree(p);
      }
    }

    if(error_load || lua_pcall(L, 0, LUA_MULTRET, 0)) {
      err(L);
    }

    // print
    if(0 < lua_gettop(L)) {
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      if(lua_pcall(L, lua_gettop(L)-1, 0, 0)) {
        err(L);
      }
    }

    add_history(line);

    xfree(line);
  }

  lua_close(L);

  return 0;
}

void err(lua_State *L) {
  const char *p = lua_tostring(L, -1);
  printf("[pcall-err] %s\n", p);
  lua_pop(L, 1);
}

int cf_quit(lua_State *L) {
  int n = lua_gettop(L);

  if(n != 0) {
    lua_pushstring(L, "invalid arguments");
    return lua_error(L);
  }

  g_quit = 1;

  return 0;
}

int cf_ls(lua_State *L) {
  int n = lua_gettop(L);
  char** cands;
  int i;

  if(n == 0) {
    // no arguments => list global variables
    lua_pushvalue(L, LUA_GLOBALSINDEX);
  } else if(n == 1 && lua_type(L, -1) == LUA_TTABLE) {
    // 1 argument => list keys of the argument
    ;
  } else {
    // 2+ arguments => error
    lua_pushstring(L, "invalid arguments");
    return lua_error(L);
  }

  cands = make_cands(L, "", NULL);
  if(!cands) {
    lua_pushstring(L, "memory allocate error");
    return lua_error(L);
  }

  // print keys in the table
  i = 0;
  while(cands[i]) {
    printf("%s\n", cands[i]);
    i++;
  }

  xfree_array(cands);

  return 0;
}

char** repl_completion(const char *text, int start, int end) {
  char **matches = NULL;
  char *tablename, *prefix;
  char *p;
  lua_State *L = g_state;

  // generate canditate
  if(!strchr(text, '.')) {
    // global variables
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    g_cands = make_cands(L, "", NULL);
  } else {
    // string keys in table
    tablename = strdup(text);
    if(!tablename) goto err;
    p = strchr(tablename, '.');
    *p = '\0';
    prefix = strdup(text);
    if(!prefix) { xfree(tablename); goto err; }
    p = strchr(prefix, '.');
    *(p+1) = '\0';
    lua_getglobal(L, tablename);
    g_cands = make_cands(L, prefix, is_key_string);
    xfree(tablename);
    xfree(prefix);
  }

  matches = rl_completion_matches(text, repl_match_generator);

  xfree_array(g_cands);

err:
  // dont perform default filename completion
  rl_attempted_completion_over = 1;
  // nothing append after completion
  rl_completion_append_character = '\0';

  return matches;
}

char* repl_match_generator(const char *text, int state) {
  static int index, len;
  char *name;
  lua_State *L = g_state;

  if(state == 0) {
    index = 0;
    len = strlen(text);
  }

  while(name = g_cands[index]) {
    index++;
    // $text is prefix of $name ?
    if(strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return NULL;
}

// - precondition
// top of stack contains data for making canditates
// - postcondition
// remove top of stack
char** make_cands(lua_State *L, const char *prefix, int (*filter)(lua_State*)) {
  const char *keyname;
  char **cands;
  int size, i;
  int len_prefix, len_keyname;

  len_prefix = strlen(prefix);

  // stack[top]: data
  // if non-table or undefined, return an array with NULL
  if(lua_type(L, -1) != LUA_TTABLE) {
    cands = (char**)malloc(sizeof(char*));
    if(!cands) {
      lua_pop(L, 1);
      return NULL;
    }
    cands[0] = NULL;
    lua_pop(L, 1);
    return cands;
  }

  // size <- the (max possible) number of eitries in table
  size = 0;
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    size++;
    // dupe key
    lua_pushvalue(L, -2);
    lua_pop(L, 2);
  }

  // allocate memory for array
  cands = (char**)malloc((size+1)*sizeof(char*));
  if(!cands) {
    lua_pop(L, 1);
    return NULL;
  }

  // put names of variables into array
  i = 0;
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    // stack[top]     value
    // stack[top-1]   key

    if(filter && !filter(L)) {
      // if it should be skipped
      lua_pop(L, 1);
      continue;
    }

    // convert keyname into string
    lua_getglobal(L, "tostring");
    lua_pushvalue(L, -3); // dupe key
    lua_call(L, 1, 1);
    keyname = lua_tostring(L, -1);
    // cands[i] <- $prefix + $name
    len_keyname = strlen(keyname);
    cands[i] = (char*)malloc((len_prefix+len_keyname+1)*sizeof(char*));
    if(!cands[i]) {
      lua_pop(L, 4);
      xfree_array(cands);
      return NULL;
    }

    strncpy(cands[i],            prefix,  len_prefix);
    strncpy(cands[i]+len_prefix, keyname, len_keyname+1);

    i++;

    // pop value and string of key
    lua_pop(L, 2);
  }

  // the last element
  cands[i] = NULL;

  // pop the last key and an input
  lua_pop(L, 2);

  return cands;
}

int is_key_string(lua_State *L) {
  // stack[top]     value
  // stack[top-1]   key

  // key is string?
  if(lua_type(L, -2) == LUA_TSTRING) {
    return 1;
  }

  return 0;
}

void xfree(void *obj) {
  if(obj) {
    free(obj);
  }
  return;
}

void xfree_array(char **array) {
  char **p = array;
  if(array) {
    while(*array) {
      xfree(*array);
      array++;
    }
    xfree(p);
  }
  return;
}
