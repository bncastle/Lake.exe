#include <stdio.h>
#include <Windows.h>
#include <signal.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "resource.h"


#include "lfs.h"

static const char *progname = "Lake";

static lua_State *globalL = NULL;

static void l_message(const char *pname, const char *msg)
{
	if (pname) luai_writestringerror("%s: ", pname);
	luai_writestringerror("%s\n", msg);
}


static int getargs(lua_State *L, char **argv, int n) 
{
	int script_args;
	int total_args = 0;
	int i = 0;

	//Count the total number of args on the command line
	while (argv[total_args]) total_args++;

	//Number of args going to the scripts
	script_args = total_args - (n + 1);

	luaL_checkstack(L, script_args + 3, "too many arguments to script");

	//Push the args onto the Lua stack
	for (i = n + 1; i < total_args; i++)
		lua_pushstring(L, argv[i]);

	lua_createtable(L, script_args, n + 1);
	for (i = 0; i < total_args; i++) {
		lua_pushstring(L, argv[i]);
		lua_rawseti(L, -2, i - n);
	}

	return script_args;
}


static void lstop(lua_State *L, lua_Debug *ar)
{
	(void)ar;  /* unused arg. */
	lua_sethook(L, NULL, 0, 0);
	luaL_error(L, "interrupted!");
}

static void laction(int i) 
{
	signal(i, SIG_DFL); /* if another SIGINT happens before lstop,
						terminate process (default action) */
	lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static int report(lua_State *L, int status)
{
	if (status != LUA_OK && !lua_isnil(L, -1))
	{
		const char *msg = lua_tostring(L, -1);
		if (msg == NULL) msg = "(error object is not a string)";
		l_message(progname, msg);
		lua_pop(L, 1);
		/* force a complete garbage collection in case of errors */
		lua_gc(L, LUA_GCCOLLECT, 0);
	}
	return status;

}

/* the next function is called unprotected, so it must avoid errors */
static void finalreport(lua_State *L, int status) 
{
	if (status != LUA_OK) {
		const char *msg = (lua_type(L, -1) == LUA_TSTRING) ? lua_tostring(L, -1)
			: NULL;
		if (msg == NULL) msg = "(error object is not a string)";
		l_message(progname, msg);
		lua_pop(L, 1);
	}
}

static int traceback(lua_State *L)
{
	const char *msg = lua_tostring(L, 1);
	if (msg)
		luaL_traceback(L, L, msg, 1);
	else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
		if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
			lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

static int docall(lua_State *L, int narg, int nres) 
{
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
	lua_pushcfunction(L, traceback);  /* push traceback function */
	lua_insert(L, base);  /* put it under chunk and args */
	globalL = L;  /* to be available to 'laction' */
	signal(SIGINT, laction);
	status = lua_pcall(L, narg, nres, base);
	signal(SIGINT, SIG_DFL);
	lua_remove(L, base);  /* remove traceback function */
	return status;
}

static int handle_script(lua_State *L, char **argv, int n) 
{
	int status = 0;
	int narg = getargs(L, argv, n);  /* collect arguments */
	lua_setglobal(L, "arg");

	//Grab our compiled lua file from our resources
	HRSRC hScript = FindResourceA(NULL, MAKEINTRESOURCE(IDR_lake_luc1), "lake_luc");
	if (hScript == NULL)
	{
		char buffer[512];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, buffer, 512, NULL);
		printf("Error: %s\n", buffer);
		status = LUA_ERRERR;
	}
	else
	{
		//Get our embedded pre-compile lake script from the resources
		HGLOBAL rsLoaded = LoadResource(NULL, hScript);
		LPVOID ptr = LockResource(rsLoaded);
		//Get the size in bytes of the resource
		DWORD scriptSize = SizeofResource(NULL, hScript);

		//Load up the file
		//NOTE: When you get a new version of lake, run the following:
		//luac.exe -s -o lake.luc
		//

		//status = luaL_loadfile(L, "..\\Lake\\lake");
		//status = luaL_loadfile(L, "lake");
		//if (status)
		//{
		//	fprintf(stderr, "%s", lua_tostring(L, -1));
		//	//return EXIT_FAILURE;
		//}

		status = luaL_loadbuffer(L, (const char *)ptr, scriptSize, "_lake_");
		if (status)
		{
			printf("ERROR!\n");
			status = LUA_ERRERR;
		}

		//Put the chunk we just loaded under the args?
		lua_insert(L, -(narg + 1));
		if (status == LUA_OK)
			status = docall(L, narg, LUA_MULTRET);
		else
			lua_pop(L, narg);
	}

	return report(L, status);
}

static int pmain(lua_State *L)
{
	//int argc = (int)lua_tointeger(L, 1);
	char **argv = (char **)lua_touserdata(L, 2);

	luaL_checkversion(L);

	//Stop collector during initialization
	lua_gc(L, LUA_GCSTOP, 0);

	//Open standard libraries
	luaL_openlibs(L);

	//TODO: Open Other libs here:

	//We use the require function here instead of opening the
	//library directly as this will register the return value
	//from the luaopen_lfs call with require's internal database of loaded modules.
	//Thus subsequent calls to require from a script will return this table
	luaL_requiref(L, "lfs", luaopen_lfs, 0);

	//Restart the GC
	lua_gc(L, LUA_GCRESTART, 0);

	//This script gets ALL the args, so we set the num passed to the C program to 0
	if (handle_script(L, argv, 0) != LUA_OK) return 0;

	//Signal no errors
	lua_pushboolean(L, 1);
	return 1;
}

int main(int argc, char *argv[])
{
	//printf("(C) 2014 Pixelbyte Studios LLC\n");
	//printf("https://github.com/stevedonovan/Lake\n");

	int status, result;

	//Try to create the Lua state
	lua_State *L = luaL_newstate();

	if (L == NULL)
	{
		l_message(argv[0], "Cannot create Lua state: not enough memory");
		return EXIT_FAILURE;
	}

	// call 'pmain' in protected mode
	lua_pushcfunction(L, &pmain);
	lua_pushinteger(L, argc);  /* 1st argument */
	lua_pushlightuserdata(L, argv); /* 2nd argument */
	status = lua_pcall(L, 2, 1, 0);
	result = lua_toboolean(L, -1);  /* get result */
	finalreport(L, status);
	lua_close(L);
	return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}