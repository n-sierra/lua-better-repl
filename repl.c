#include <stdio.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <readline/readline.h>
#include <readline/history.h>

lua_State *g_state;
char **g_words;
int g_quit;

void err(lua_State *L);

int cf_quit(lua_State *L);
int cf_ls(lua_State *L);

char** lisc_completion(const char* text, int start, int end);
char* var_generator(const char* text, int state);

char** make_words(lua_State *L);
char** make_words_table(lua_State *L, const char *text, int start, int end);

void xfree(void *obj);

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
  rl_readline_name = "lisc";
  rl_basic_word_break_characters = " \t\n\"+-*/^%><=|;,{(#";
  rl_attempted_completion_function = lisc_completion;

  g_state = L;
  g_words = NULL;
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
    line = readline("lua>");

    // eval
    error_load = 0;
    if(luaL_loadstring(L, line)) {
      char *p;
      p = malloc(strlen(line)*sizeof(char)+sizeof("return "));
      sprintf(p, "return %s", line);
      if(luaL_loadstring(L, p)) {
        lua_pop(L, 1);
        error_load = 1;
      } else {
        lua_remove(L, lua_gettop(L)-1);
      }
      free(p);
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

  xfree(g_words);

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

  // print string keys in the table
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    // dupe key
    lua_pushvalue(L, -2);

    // only strings are accepted after '.' (dot)
    if(lua_type(L, -1) != LUA_TSTRING) {
      lua_pop(L, 2);
      continue;
    }

    // print key and type of value
    printf("%-10s %s\n",
        lua_typename(L, lua_type(L, -2)),
        lua_tostring(L, -1));

    // pop duped key and value
    lua_pop(L, 2);
  }

  lua_pop(L, 1);

  return 0;
}

char** lisc_completion(const char *text, int start, int end) {
  const char *name;
  int i, size;
  char **matches = NULL;
  lua_State *L = g_state;

  // update word list
  xfree(g_words);

  if(!strchr(text, '.')) {
    // global
    g_words = make_words(L);
  } else {
    // table
    g_words = make_words_table(L, text, start, end);
  }

  matches = rl_completion_matches(text, var_generator);

  // dont perform default filename completion
  rl_attempted_completion_over = 1;
  // nothing append after completion
  rl_completion_append_character = '\0';

  return matches;
}

char* var_generator(const char *text, int state) {
  static int index, len;
  char *name;
  lua_State *L = g_state;

  if(state == 0) {
    index = 0;
    len = strlen(text);
  }

  while(name = g_words[index]) {
    index++;
    // $text is prefix of $name ?
    if(strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return NULL;
}

char** make_words(lua_State *L) {
  const char *name;
  char **words;
  int size, i;

  lua_pushvalue(L, LUA_GLOBALSINDEX);

  // get the number of eitries in table
  size = 0;
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    size++;
    lua_pop(L, 1);
  }

  // allocate memory for matches
  words = (char**)malloc((size+1)*sizeof(char*));

  // put names of variables into matches
  i = 0;
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    lua_pushvalue(L, -2);
    if(lua_type(L, -1) != LUA_TSTRING) {
      lua_pop(L, 2);
      continue;
    }
    name = lua_tostring(L, -1);
    words[i] = strdup(name);
    i++;
    lua_pop(L, 2);
  }

  // the last element
  words[i] = NULL;

  // pop the last key
  lua_pop(L, 1);

  return words;
}

char** make_words_table(lua_State *L, const char *text, int start, int end) {
  char *table_name;
  const char *name;
  char **words;
  int size, i, len;
  char *p;

  table_name = strdup(text);
  p = strchr(table_name, '.');
  *p = '\0';
  len = strlen(table_name);

  lua_getglobal(L, table_name);

  if(lua_type(L, -1) != LUA_TTABLE) {
    // non-table or undefined
    words = (char**)malloc(sizeof(char*));
    words[0] = NULL;
    lua_pop(L, 1);
    return words;
  }

  // get the number of eitries in table
  size = 0;
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    size++;
    // dupe key
    lua_pushvalue(L, -2);
    lua_pop(L, 2);
  }

  // allocate memory for matches
  words = (char**)malloc((size+1)*sizeof(char*));

  // put names of variables into matches
  i = 0;
  lua_pushnil(L);
  while(lua_next(L, -2) != 0) {
    lua_pushvalue(L, -2);
    if(lua_type(L, -1) != LUA_TSTRING) {
      lua_pop(L, 2);
      continue;
    }
    name = lua_tostring(L, -1);
    // words[i] <- <table> . <name>
    words[i] = (char*)malloc((len+strlen(name)+2)*sizeof(char*));
    strncpy(words[i], table_name, len);
    words[i][len] = '.';
    strncpy(words[i]+len+1, name, strlen(name));
    words[i][len+strlen(name)+1] = '\0';
    i++;
    lua_pop(L, 2);
  }

  // the last element
  words[i] = NULL;

  // pop the last key
  lua_pop(L, 1);

  xfree(table_name);

  return words;
}

void xfree(void *obj) {
  if(obj != NULL) {
    free(obj);
  }
  return;
}

