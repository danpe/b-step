local root = assert(os.getenv("BSTEP_HOST_TEST_DIR"), "Set an isolated test resource directory")
assert(reaper.GetResourcePath() == root, "Wrong resource directory")
assert(reaper.CountTracks(0) == 0, "Start with an empty test project")
local log = assert(io.open(root .. "/host-test.log", "w"))
local function report(s) log:write(string.format("%.6f %s\n", reaper.time_precise(), s)); log:flush() end
local function setup()
  local ok, mode = reaper.GetAudioDeviceInfo("MODE")
  report("AUDIO " .. tostring(ok) .. " " .. tostring(mode))
  for _,k in ipairs({"IDENT_IN","IDENT_OUT","BSIZE","SRATE"}) do local _,v=reaper.GetAudioDeviceInfo(k);report(k.."="..tostring(v)) end
  reaper.SetMediaTrackInfo_Value(reaper.GetMasterTrack(0), "D_VOL", 0)
  for i=0,6 do
    reaper.InsertTrackAtIndex(i, false)
    local t=reaper.GetTrack(0,i)
    reaper.SetMediaTrackInfo_Value(t, "I_PERFFLAGS", 2)
    local fx=reaper.TrackFX_AddByName(t, "VST3i: B-Step", false, -1)
    assert(fx>=0, "B-Step not found")
    local _,name=reaper.TrackFX_GetFXName(t,fx)
    report("FX "..i.." "..name.." params="..reaper.TrackFX_GetNumParams(t,fx))
    reaper.TrackFX_SetParamNormalized(t,fx,8,0)
    for j=14,77 do reaper.TrackFX_SetParamNormalized(t,fx,j,j<30 and 0 or 1) end
    reaper.CreateNewMIDIItemInProj(t,0,20000,false)
  end
  local xa,xm=reaper.GetUnderrunTime(); report("INITIAL_UNDERRUN "..xa.." "..xm)
  reaper.GetSetRepeat(0)
  reaper.SetCurrentBPM(0,120,false)
  reaper.SetEditCurPos(120,false,false)
  reaper.Main_SaveProjectEx(0,root.."/host-test.RPP",0)
  local start=reaper.time_precise()
  local stages={
    {0,"start",function() reaper.OnPlayButtonEx(reaper.EnumProjects(-1,"")) end},
    {2,"bpm121",function() reaper.SetCurrentBPM(0,121,false) end},
    {3,"bpm80",function() reaper.SetCurrentBPM(0,80,false) end},
    {4,"bpm160",function() reaper.SetCurrentBPM(0,160,false) end},
    {5,"bpm120",function() reaper.SetCurrentBPM(0,120,false) end},
    {6,"bpm121.5",function() reaper.SetCurrentBPM(0,121.5,false) end},
    {7,"bpm120",function() reaper.SetCurrentBPM(0,120,false) end},
  }
  for i=0,39 do
    local bpm=60+(i*37)%128
    stages[#stages+1]={8+i*0.1,"sweep"..bpm,function() reaper.SetCurrentBPM(0,bpm,false) end}
  end
  stages[#stages+1]={13,"constant120",function() reaper.SetCurrentBPM(0,120,false) end}
  stages[#stages+1]={15,"seekforward",function() reaper.SetEditCurPos(300,false,true) end}
  stages[#stages+1]={17,"seekback",function() reaper.SetEditCurPos(5,false,true) end}
  stages[#stages+1]={19,"loop",function() reaper.GetSet_LoopTimeRange(true,true,0,2,false);reaper.GetSetRepeat(1);reaper.SetEditCurPos(0,false,true) end}
  stages[#stages+1]={24,"stop",function() reaper.OnStopButtonEx(reaper.EnumProjects(-1,"")) end}
  stages[#stages+1]={25,"stoppedtempo",function() reaper.SetCurrentBPM(0,145,false) end}
  stages[#stages+1]={26,"resume",function() reaper.OnPlayButtonEx(reaper.EnumProjects(-1,"")) end}
  stages[#stages+1]={28,"stopfinal",function() reaper.OnStopButtonEx(reaper.EnumProjects(-1,"")) end}
  stages[#stages+1]={30,"finish",function()
    local xa,xm=reaper.GetUnderrunTime(); report("FINAL_UNDERRUN "..xa.." "..xm)
    report("DONE");log:close()
    reaper.Main_SaveProjectEx(0,root.."/host-test-result.RPP",8)
    reaper.Main_OnCommand(40004,0)
  end}
  local index=1
  local function run()
    local t=reaper.time_precise()-start
    while stages[index] and t>=stages[index][1] do
      report(stages[index][2].." state="..reaper.GetPlayState().." pos="..reaper.GetPlayPosition())
      stages[index][3]();index=index+1
    end
    if stages[index] then reaper.defer(run) end
  end
  run()
end
local ready=reaper.time_precise()+2
local function wait_ready() if reaper.time_precise()<ready then reaper.defer(wait_ready) else setup() end end
wait_ready()
