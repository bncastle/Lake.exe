--This is  a Lakefile
--To build, use: Lake.exe -f <name of lakefile> <target>
--Note: If this file is simply named lakefile or lakefile.lua, then you can omit -f <name of lakefile>
--
local LUAC_CMD = '..'..DIRSEP..'tools'..DIRSEP..'luac.exe -s -o lake.luc lake'
local OBJ_DIR = 'obj'
local LOC = './'
local INCDIR = '../inc'
local LIBDIR = '../lib'

local libraries = 'lua52_static lfs_static'

--Building for windows? Add the _static suffix
if(CC == 'gcc') then libraries = 'lua52 lfs' end

--This loads a config file to see if we need to override some options
--dofile('../config.lua') 

rc = wresource.group {src='resources', odir = OBJ_DIR}
trgt = c.program({LOC .. 'lake', src='main', deps = rc, static= true, odir = OBJ_DIR, incdir = INCDIR, libdir = LIBDIR, libs= libraries})
action('build', function()
	--do pre-build here
	print('Compiling Lake into Lake.luc...')
	os.execute(LUAC_CMD)
	print("done!")	
end)

--target('g', p)

default{'build', trgt}