-- MA API adapter: models object handles, show data, selection/grid and programmer.
-- Does not simulate the MA scheduler, filter engine or physical DMX output.
MAOBS_TEST_MODE=true
-- Regression tests execute the exact core embedded in the importable file.
-- The public bootstrap is exercised separately below with test mode disabled.
local production_loadfile=loadfile
local file=assert(io.open('MA3/MA_OBS_MediaLink.lua','rb'))
local installed_source=file:read('*a');file:close()
local core_source=assert(installed_source:match('local CORE_SOURCE = %[%=%=%=%=%[\n(.-)%]%=%=%=%=%]'))
local function loadcore() return load(core_source,'@MA OBS MediaLink core','t',_ENV) end
local variables,commands,timers={}, {}, {}
local sources,groups,presets,sequences,programmer={},{},{},{},{}
local selection={{id=77,x=3,y=4,z=1},{id=78,x=9,y=2,z=0}}
local initial_selection=selection
local current_dp=1
local displayed={}
local fault
local function object(fields,addr)
    fields.address=addr
    function fields:Addr() return self.address end
    function fields:Children() return self.children or {} end
    function fields:Set(key,value) if not (self.refuse and self.refuse[key]) then self[key]=value end end
    return fields
end
local main=object({no=3,name='Main',children={}},'DataPool 1 Sequence 3')
function main:CurrentChild() return self.current end
sequences[3]=main
local dp={no=1,PresetPools={}}
for n=21,25 do dp.PresetPools[n]={} end
function DataPool() dp.no=current_dp;return dp end
function GlobalVars() return variables end
-- Model the actual MA boundary: a missing GetVar can return NO Lua values.
-- Empty-string writes are not a reliable deletion; SetVar uses numeric success.
local variable_fault,delete_fault
function GetVar(_,key)
    if variables[key]==nil then return end
    return variables[key]
end
function SetVar(_,key,value)
    if value=='' then return false end
    if variable_fault==key then return 0 end
    variables[key]=value;return 1
end
function DelVar(_,key)
    if delete_fault==key or variables[key]==nil then return 0 end
    variables[key]=nil;return 1
end
function SerialNumber() return 'TEST-STATION' end
local test_clock=0
function Time() test_clock=test_clock+0.1;return test_clock end
function Printf() end
function ErrPrintf() end
function Timer(fn,interval,count) assert((interval==0.1 or interval==1) and count==1);timers[#timers+1]=fn end
function SelectionCount() return #selection end
function MessageBox(t) displayed[#displayed+1]=t;return {success=true,result=0} end
local channelmap={}
local function addfixture(id,attrs)
    local f=object({no=id},'Fixture '..id);f.channels={}
    for i,name in ipairs(attrs) do
        local index=id*10+i
        local ch={INDEX=index+1,SUBATTRIBUTE=name,index=index,id=id,attribute=name}
        f.channels[#f.channels+1]=ch;channelmap[index]=ch
    end
    sources[id]=f;return f
end
addfixture(300,{'VideoCrossfade'})
addfixture(301,{'VideoSceneNumber','Dimmer','VideoTransitionToken'})
programmer[777]={sentinel='other fixture values'}
function GetUIChannels(f,handles)
    local result={};for _,ch in ipairs(f.channels) do result[#result+1]=handles and ch or ch.index end;return result
end
local rt={}
local patch_base=5632
function GetRTChannels(f)
    local result={}
    local offsets={VideoCrossfade=0,VideoSceneNumber=3,Dimmer=5,VideoTransitionToken=8}
    for _,ch in ipairs(f.channels) do
        local coarse=patch_base+offsets[ch.attribute]
        rt[ch.index]={ui_index_first=ch.index,patch={coarse=coarse,fine=coarse+1,
            ultra=(ch.attribute=='Dimmer' or ch.attribute=='VideoCrossfade') and coarse+2 or 0}}
        result[#result+1]=ch.index
    end
    return result
end
function GetRTChannel(index) return rt[index] end
function GetProgPhaser(index) return programmer[index] end
function GetPresetData(o) return o.data or {by_fixtures={}} end
local function cueobject(seq,no,data,name,note)
    local part=object({data=data},'DataPool 1 Sequence '..seq..' Cue '..no..' Part 0')
    local cue=object({no=no*1000,name=name or '',Note=note,[1]=part,children={part}},'DataPool 1 Sequence '..seq..' Cue '..no)
    local s=sequences[seq]
    if not s then s=object({no=seq,name='',children={}},'DataPool 1 Sequence '..seq);sequences[seq]=s end
    s.children[#s.children+1]=cue
    return cue
end
local function findcue(seq,no)
    local s=sequences[seq];if s then for _,q in ipairs(s.children) do if q.no==no*1000 then return q end end end
end
function ObjectList(text)
    local id=text:match('^Fixture (%d+)$');if id then return sources[tonumber(id)] and {sources[tonumber(id)]} or {} end
    local d,seq,cue=text:match('^DataPool (%d+) Sequence (%d+) Cue ([%d%.]+)$')
    if d then assert(tonumber(d)==1);local c=findcue(tonumber(seq),tonumber(cue));return c and {c} or {} end
    d,seq=text:match('^DataPool (%d+) Sequence (%d+)$')
    if d then assert(tonumber(d)==1);local s=sequences[tonumber(seq)];return s and {s} or {} end
    local pool,slot
    d,pool,slot=text:match('^DataPool (%d+) Preset (%d+)%.(%d+)$')
    if d then assert(tonumber(d)==1);local p=dp.PresetPools[tonumber(pool)][tonumber(slot)];return p and {p} or {} end
    local group;d,group=text:match('^DataPool (%d+) Group (%d+)$')
    if d then local g=groups[tonumber(group)];return g and {g} or {} end
    error('Unsupported lookup: '..text)
end
local function selected_data()
    local by={}
    for _,selected in ipairs(selection) do
        local f=sources[selected.id]
        if f then for _,ch in ipairs(f.channels) do
            if programmer[ch.index] then
                by[f.no]=by[f.no] or {};by[f.no][ch.attribute]=programmer[ch.index]
            end
        end end
    end
    return {by_fixtures=by}
end
function Cmd(text)
    assert(not text:find('ClearAll',1,true),'Global clear forbidden')
    commands[#commands+1]=text
    if fault and text:find(fault,1,true) then return 'Failed injected error' end
    if text=='ClearSelection' then selection={};return 'OK' end
    local id=text:match('^Fixture (%d+)$')
    if id then selection={{id=tonumber(id),x=0,y=0,z=0}};return 'OK' end
    id=text:match('^Off Fixture (%d+)$')
    if id then for _,ch in ipairs(sources[tonumber(id)].channels) do programmer[ch.index]=nil end;return 'OK' end
    local group=text:match('^Store DataPool 1 Group (%d+) ')
    if group then
        assert(not groups[tonumber(group)]);groups[tonumber(group)]={selection=selection};return 'OK'
    end
    group=text:match('^Group (%d+)$');if group then selection=assert(groups[tonumber(group)]).selection;return 'OK' end
    group=text:match('^Delete DataPool 1 Group (%d+) ');if group then groups[tonumber(group)]=nil;return 'OK' end
    local attribute,bits,value=text:match('^Attribute "([^"]+)" At Absolute Decimal(%d+) (%d+)$')
    if attribute then
        assert(#selection==1)
        for _,ch in ipairs(sources[selection[1].id].channels) do
            if ch.attribute==attribute then
                local raw=tonumber(value)
                if bits=='16' then raw=math.floor(raw*16777215/65535+0.5) end
                programmer[ch.index]={{absolute_value=raw}};return 'OK'
            end
        end
        error('Missing attribute')
    end
    local pool,slot=text:match('^Store DataPool 1 Preset (%d+)%.(%d+) ')
    if pool then
        pool,slot=tonumber(pool),tonumber(slot);assert(not dp.PresetPools[pool][slot],'Overwrite forbidden')
        assert(text:find('/ActiveForSelected',1,true) and text:find('/Selective',1,true))
        dp.PresetPools[pool][slot]=object({no=slot,name='',data=selected_data()},'DataPool 1 Preset '..pool..'.'..slot);return 'OK'
    end
    local label;pool,slot,label=text:match('^Label DataPool 1 Preset (%d+)%.(%d+) "(.*)"$')
    if pool then dp.PresetPools[tonumber(pool)][tonumber(slot)].name=label;return 'OK' end
    local seq,no=text:match('^Store DataPool 1 Sequence (%d+) Cue (%d+) ')
    if seq then
        assert(text:find('/ActiveForSelected',1,true));assert(not findcue(tonumber(seq),tonumber(no)))
        cueobject(tonumber(seq),tonumber(no),selected_data());return 'OK'
    end
    seq,no,label=text:match('^Label DataPool 1 Sequence (%d+) Cue (%d+) "(.*)"$')
    if seq then findcue(tonumber(seq),tonumber(no)).name=label:gsub(',','');return 'OK' end
    seq,label=text:match('^Label DataPool 1 Sequence (%d+) "(.*)"$')
    if seq then sequences[tonumber(seq)].name=label;return 'OK' end
    seq,no=text:match('^Goto DataPool 1 Sequence (%d+) Cue (%d+) Fade 0$')
    if seq then assert(findcue(tonumber(seq),tonumber(no)));return 'OK' end
    error('Unsupported command: '..text)
end
local A=assert(loadcore())('MA OBS MediaLink','Bedienung')
local c=A.default_config();c.configured=1;c.base=101
local function expect_error(fn,pattern)
    local ok,e=pcall(fn);assert(not ok and tostring(e):find(pattern,1,true),tostring(e))
end
local function count_go()
    local n=0;for _,cmd in ipairs(commands) do if cmd:match('^Goto ') then n=n+1 end end;return n
end
local function run_generation(plan)
    local thread=coroutine.create(function() return A.generate(c,plan) end)
    local ok,result
    repeat ok,result=coroutine.resume(thread);assert(ok,result) until coroutine.status(thread)=='dead'
    return result
end
assert(A.raw24(0)==0 and A.raw24(1023)==16777215)
for i=0,1023 do
    local r=A.raw24(i);assert(math.floor(r*1023/16777215+0.5)==i)
    if i>0 then assert(r>A.raw24(i-1)) end
end
assert(A.encode(A.decode(A.encode(c)))==A.encode(c));A.preflight(c)
-- Regression: RT addresses start at zero. Old v0.31 rejects raw 5632 at line 136.
for _,raw in ipairs({0,1,502,512,5632,5633,6134}) do
    patch_base=raw
    local label=A.verify_patch(c)
    assert(label=='sACN U'..(math.floor(raw/512)+1)..' · Startadresse '..(raw%512+1),label)
end
for _,raw in ipairs({503,504,511,6135,6143}) do
    patch_base=raw
    expect_error(function() A.verify_patch(c) end,'Universumsgrenze')
end
patch_base=5632
-- DataPool 1 stays valid; reserved Preview DataPool 128 does not.
local reserved={};for k,v in pairs(c) do reserved[k]=v end;reserved.dp=128
expect_error(function() A.validate(reserved) end,'Preview')
local migrated=A.decode(A.encode(c):gsub(';mode=1',''))
assert(migrated.mode==1 and migrated.dp==1 and migrated.base==101)
assert(A.clean_error('[string "MA OBS MediaLink"]:136: Patchfehler\nstack traceback:\n technical')=='Patchfehler')

local invalid=A.default_config();invalid.fade=invalid.controller;expect_error(function() A.validate(invalid) end,'verschieden')
invalid=A.default_config();invalid.pool=20;expect_error(function() A.validate(invalid) end,'All-Presetpool')
-- Sparse append + immutable raw positions, ten NEW presets per click.
local plan=A.plan_presets(c,'append');assert(#plan.items==10 and plan.items[1].slot==101 and plan.items[10].slot==191)
assert(run_generation(plan)==10)
assert(selection==initial_selection and selection[1].x==3 and programmer[777].sentinel)
assert(next(groups)==nil and programmer[3001]==nil)
plan=A.plan_presets(c,'append');assert(plan.items[1].slot==201 and plan.items[10].slot==291)
assert(run_generation(plan)==10)
-- Foreign occupancy is reported and never overwritten; gaps explicitly fill slots.
local foreign=object({no=102,name='Foreign'},'DataPool 1 Preset 25.102');dp.PresetPools[25][102]=foreign
local gaps=function(t)local r={};for k,v in pairs(t) do r[k]=v end;return r end
gaps=gaps(c);gaps.step=1;gaps.count=2
plan=A.plan_presets(gaps,'gaps');assert(#plan.conflicts==1 and plan.items[1].slot==103 and plan.items[2].slot==104)
assert(dp.PresetPools[25][102]==foreign)
-- Gap mode must fill intermediate slots even while append spacing remains 10.
local actual_gaps=A.plan_presets(c,'gaps')
assert(actual_gaps.items[1].slot==103 and actual_gaps.items[2].slot==104,
    'Gap mode still uses append spacing and grows the range instead of filling holes')
-- Logical bound: only 103 sparse (stride 10) positions exist in 1024 slots.
local bound={};for k,v in pairs(c) do bound[k]=v end;bound.count=1024
plan=A.plan_presets(bound,'append');assert(#plan.items==83 and plan.items[#plan.items].index==1020)
-- All five All pools supported; root pool selection isn't changed by the plugin.
for pool=21,25 do
    local other={};for k,v in pairs(c) do other[k]=v end;other.pool=pool
    assert(#A.plan_presets(other,'append').items==10)
end
current_dp=2;expect_error(function() A.preflight(c) end,'Datenpool');current_dp=1
-- Race after preview must not replace the now occupied target.
local raced=A.plan_presets(c,'append');local slot=raced.items[1].slot
dp.PresetPools[25][slot]=foreign
expect_error(function() A.generate(c,raced) end,'inzwischen belegt');dp.PresetPools[25][slot]=nil
-- Failures restore selection/programmer and remove the temporary group.
fault='Store DataPool 1 Preset';local retry=A.plan_presets(c,'append')
expect_error(function() A.generate(c,{items={retry.items[1]}}) end,'Befehl fehlgeschlagen');fault=nil
assert(selection==initial_selection and next(groups)==nil and programmer[777].sentinel and programmer[3001]==nil)
programmer[3011]={{absolute_value=12}}
expect_error(function() A.isolated(c,301,function()error('must not run')end) end,'Programmer-Daten');programmer[3011]=nil
-- Main cue read must use the chosen sequence, not global selection.
local function maincue(no,slot,scene)
    local p=dp.PresetPools[25][slot]
    local value=p.data.by_fixtures[300].VideoCrossfade[1].absolute_value
    return cueobject(3,no,{by_fixtures={[300]={VideoCrossfade={abs_preset=p,{absolute_value=value}}}}},'',scene)
end
local q1=maincue(1,101,'OBS:6');local q2=maincue(2,111,'OBS:6')
local e1=assert(A.read_cue(c,q1));assert(e1.index==0 and e1.scene==6)
q1.Note='OBS: 006';assert(A.read_cue(c,q1));q1.Note='OBS:6'
local bad=maincue(3,121,'OBS:0');local e,why=A.read_cue(c,bad);assert(not e and why)
bad.Note=''
-- Regression from screenshot: first Start with no HEARTBEAT must not call tonumber().
assert(select('#',GetVar(nil,'MAOBS_ML31_HEARTBEAT'))==0)
A.start(c);assert(A.state.running);A.stop('Aus')
assert(variables.MAOBS_ML31_SENDER==nil and variables.MAOBS_ML31_LAST==0)
-- Metadata revision creation only adds cues; unchanged revisions reused.
local r1=A.revision(c,e1);assert(r1.control==1001 and r1.token==1)
assert(A.find_revision(c,e1).control==1001)
local changed={};for k,v in pairs(e1) do changed[k]=v end
changed.scene=7;changed.signature='1000,7,0'
local r2=A.revision(c,changed);assert(r2.control==1002 and r2.token==2)
assert(math.floor(findcue(900,1001)[1].data.by_fixtures[301].VideoSceneNumber[1].absolute_value/256)==6)
-- Fixed-sequence dispatch, changes, removal, start/stop/epoch, foreign lock.
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c));DelVar(nil,'MAOBS_ML31_SENDER')
main.current=q1;A.start(c);local epoch=A.state.epoch;A.tick(epoch);assert(count_go()==1)
A.tick(epoch);assert(count_go()==1)
main.current=q2;A.tick(epoch);assert(count_go()==2)
local before=#commands;A.start(c);assert(#commands==before) -- duplicate Start is inert
A.stop('Aus');assert(GetVar(nil,'MAOBS_ML31_LAST')==0)
A.tick(epoch);assert(count_go()==2)
SetVar(nil,'MAOBS_ML31_SENDER','other-station');SetVar(nil,'MAOBS_ML31_HEARTBEAT',os.time());expect_error(function() A.start(c) end,'Sender reserviert')
DelVar(nil,'MAOBS_ML31_SENDER');A.start(c);epoch=A.state.epoch
SetVar(nil,'MAOBS_ML31_SENDER','other-station');A.tick(epoch);assert(not A.state.running and count_go()==2)
assert(GetVar(nil,'MAOBS_ML31_SENDER')=='other-station');DelVar(nil,'MAOBS_ML31_SENDER')
-- Startup policy only runs through startup entry. Stop records last status.
for _,policy in ipairs({1,2,3}) do
    c.startup=policy;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c));SetVar(nil,'MAOBS_ML31_LAST',0)
    assert(A.startup()==(policy==1));if A.state.running then A.stop() end
end
c.startup=3;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c));SetVar(nil,'MAOBS_ML31_LAST',1)
assert(A.startup() and A.state.running);A.stop()
-- Foreign sequence never claimed silently.
sequences[900].name='Foreign show sequence';sequences[900].Note=nil
expect_error(function() A.start(c) end,'Steuersequenz ist belegt')
-- Explicit adoption rejects hidden commands and foreign fixture data.
findcue(900,1001).Command='Go+ Sequence 88'
expect_error(function() A.adoption(c) end,'nicht übernommen')
findcue(900,1001).Command=nil
-- Stale saved sender reservation does not permanently block startup.
SetVar(nil,'MAOBS_ML31_SENDER','stale');SetVar(nil,'MAOBS_ML31_HEARTBEAT',os.time()-20)
sequences[900].name='MA OBS MediaLink [DP1 S3 F300 C301]'
A.start(c);assert(A.state.running);A.stop()
-- Fixed-sequence capability absence refuses startup rather than selecting a sequence.
local current_child=main.CurrentChild;main.CurrentChild=false
expect_error(function() A.preflight(c) end,'CurrentChild');main.CurrentChild=current_child
-- Unpatching/incompatible resolution fails closed before any writer.
local rt_original=GetRTChannel
function GetRTChannel(index) local r=rt_original(index);r.patch.fine=0;return r end
expect_error(function() A.preflight(c) end,'Fine-Adresse');GetRTChannel=rt_original
-- Headless startup doesn't open a dialog and retries when show config isn't ready.
local previous_boxes=#displayed
c.startup=1;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c));A.main(nil,'startup')
assert(A.state.running and #displayed==previous_boxes);A.stop()
SetVar(nil,'MAOBS_ML31_CONFIG',nil);local timer_count=#timers;A.main(nil,'startup')
assert(not A.state.running and #timers==timer_count+1 and #displayed==previous_boxes)
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c));timers[#timers]()
assert(A.state.running);A.stop()
-- About matches OBS branding, copyright, dedication and beta note.
local found=false
function MessageBox(t)
    displayed[#displayed+1]=t
    if not found then found=true;return {success=true,result=6} end
    return {success=true,result=0}
end
A.main();if A.state.running then A.stop() end
local all='';for _,t in ipairs(displayed) do all=all..t.message end
assert(all:find('Copyright © 2026 Oliver Cassel',1,true) and all:find('Für Farin und Bela.',1,true))
assert(all:find('Diese Beta-Version befindet sich in Entwicklung.',1,true))
print('PASS: actual Lua 5.4 plugin – sparse generation/append/gaps/bounds/conflicts, immutable raw mapping, programmer/selection restoration, fixed-sequence cue reading, metadata revisions/reuse, dispatch/duplicate stop guards, sender lock, three startup policies, About')

-- Actual UI handlers: no hardware/patch preflight on Save; all preset fields together.
local function answer_defaults(t,result)
    local answer={success=true,result=result,inputs={},selectors={}}
    for _,field in ipairs(t.inputs or {}) do answer.inputs[field.name]=field.value end
    for _,selector in ipairs(t.selectors or {}) do answer.selectors[selector.name]=selector.selectedValue end
    return answer
end
local rt_api=GetRTChannel
function GetRTChannel() error('Injected patch read failure') end
local before_commands=#commands
local setup_seen=false
function MessageBox(t)
    displayed[#displayed+1]=t
    if t.inputs then
        setup_seen=true
        for _,field in ipairs(t.inputs) do
            assert(not field.name:find('Presetposition',1,true) and not field.name:find('All-Presetpool',1,true))
        end
        return answer_defaults(t,1)
    end
    return {success=true,result=0}
end
local saved=A.setup(c)
assert(setup_seen and saved.configured==1 and saved.dp==1 and #commands==before_commands)
assert(GetVar(nil,'MAOBS_ML31_CONFIG')==A.encode(saved))
local presets_seen=false
function MessageBox(t)
    displayed[#displayed+1]=t
    if t.inputs then
        presets_seen=true
        local answer=answer_defaults(t,2)
        assert(answer.inputs['01 Erste Presetposition'] and answer.inputs['02 Anzahl zusätzlich'] and answer.inputs['03 Abstand beim Anhängen'])
        assert(t.selectors[1].values['All 1']==21 and t.selectors[1].values['All 5']==25)
        answer.inputs['01 Erste Presetposition']='201'
        answer.inputs['02 Anzahl zusätzlich']='15'
        answer.inputs['03 Abstand beim Anhängen']='5'
        answer.selectors['04 All-Presetpool']=22
        answer.selectors['05 Vorgehen']=2
        return answer
    end
    return {success=true,result=0}
end
saved=A.presets_ui(saved)
assert(presets_seen and saved.base==201 and saved.pool==22 and saved.count==15 and saved.step==5 and saved.mode==2)
assert(#commands==before_commands and GetVar(nil,'MAOBS_ML31_CONFIG')==A.encode(saved))
function MessageBox(t) return {success=true,result=0} end
local config_before=GetVar(nil,'MAOBS_ML31_CONFIG')
assert(A.presets_ui(saved)==saved and GetVar(nil,'MAOBS_ML31_CONFIG')==config_before)
-- Error from Preview is short in modal; full trace retained for diagnostic log.
local stage=0;local friendly
function MessageBox(t)
    displayed[#displayed+1]=t
    if stage==0 then stage=1;return {success=true,result=3} end
    if t.inputs then return answer_defaults(t,1) end
    if t.title:find('Hinweis',1,true) then friendly=t.message;return {success=true,result=0} end
    return {success=true,result=0}
end
A.main()
assert(friendly and friendly:find('Injected patch read failure',1,true))
assert(not friendly:find('stack traceback',1,true) and not friendly:find('.lua:',1,true))
assert(A.state.last_error:find('stack traceback',1,true))
GetRTChannel=rt_api
print('PASS: zero-based RT universe starts/end boundaries, Preview DP rejection, DP1 support, v0.31 config migration, combined preset settings, Save without patch, cancel without writes, concise UI errors with technical log')

-- v0.33: exercise exactly the zero-return/empty-write MA behavior from screenshots.
assert(select('#',GetVar(nil,'NO_SUCH_VAR'))==0)
assert(select('#',A.readvar('NO_SUCH_VAR'))==1 and A.readvar('NO_SUCH_VAR')==nil)
assert(not pcall(function() return tonumber(GetVar(nil,'NO_SUCH_VAR')) end))
assert(tonumber(A.readvar('NO_SUCH_VAR'))==nil)
DelVar(nil,'MAOBS_ML31_LAST');DelVar(nil,'MAOBS_ML31_HEARTBEAT');DelVar(nil,'MAOBS_ML31_SENDER')
c.startup=3;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c))
assert(A.startup()==false and not A.state.running) -- missing last status means off
A.start(c);assert(A.state.running)
DelVar(nil,'MAOBS_ML31_HEARTBEAT')
A.tick(A.state.epoch);assert(variables.MAOBS_ML31_HEARTBEAT==os.time())
A.stop('Aus')
assert(variables.MAOBS_ML31_SENDER==nil and variables.MAOBS_ML31_HEARTBEAT==nil)
assert(variables.MAOBS_ML31_LAST==0)
A.clearvar('SENDER');A.clearvar('SENDER') -- repeated absent deletion is harmless
-- Check both actual MA boolean/numeric failure results rather than Lua truthiness of 0.
variable_fault='MAOBS_ML31_TEST'
expect_error(function() A.writevar('TEST',123) end,'nicht gespeichert')
variable_fault=nil
A.writevar('TEST',123);assert(variables.MAOBS_ML31_TEST==123)
expect_error(function() A.writevar('TEST','') end,'DelVar')
-- Partial acquisition must release only this attempted sender and retain last state.
variable_fault='MAOBS_ML31_HEARTBEAT'
expect_error(function() A.start(c) end,'nicht gespeichert')
variable_fault=nil
assert(not A.state.running and variables.MAOBS_ML31_SENDER==nil and variables.MAOBS_ML31_LAST==0)
-- Failure to replace a stale foreign owner must not clear that owner.
SetVar(nil,'MAOBS_ML31_SENDER','stale-foreign');SetVar(nil,'MAOBS_ML31_HEARTBEAT',os.time()-20)
variable_fault='MAOBS_ML31_SENDER'
expect_error(function() A.start(c) end,'nicht gespeichert')
variable_fault=nil
assert(variables.MAOBS_ML31_SENDER=='stale-foreign' and not A.state.running)
A.clearvar('SENDER');A.clearvar('HEARTBEAT')
-- Timer failure after registration invalidates the callback and rolls back LAST.
local original_timer=Timer
function Timer(fn,interval,count) original_timer(fn,interval,count);error('Timer registration failed') end
expect_error(function() A.start(c) end,'Timer registration failed')
Timer=original_timer
assert(not A.state.running and variables.MAOBS_ML31_SENDER==nil and variables.MAOBS_ML31_LAST==0)
local before_callback=#commands;timers[#timers]();assert(#commands==before_callback)
-- A refused deletion remains an explicit error; the runtime is still stopped.
A.start(c);delete_fault='MAOBS_ML31_SENDER'
expect_error(function() A.stop('Aus') end,'nicht freigegeben')
assert(not A.state.running and A.state.status=='Aus' and variables.MAOBS_ML31_LAST==0 and variables.MAOBS_ML31_SENDER)
delete_fault=nil;A.clearvar('SENDER');A.clearvar('HEARTBEAT')
-- The actual Diagnose -> Sender freigeben path uses DelVar, never empty SetVar.
SetVar(nil,'MAOBS_ML31_SENDER','foreign-to-release');SetVar(nil,'MAOBS_ML31_HEARTBEAT',os.time())
local release_dialogs=0
function MessageBox(t)
    displayed[#displayed+1]=t;release_dialogs=release_dialogs+1
    return {success=true,result=1}
end
A.diagnosis(c)
assert(release_dialogs==2 and variables.MAOBS_ML31_SENDER==nil and variables.MAOBS_ML31_HEARTBEAT==nil)
-- Numeric variables may be read back as numeric strings without losing verification.
local normal_getvar=GetVar
function GetVar(scope,key)
    local value=normal_getvar(scope,key)
    if value==nil then return end
    if type(value)=='number' then return tostring(value) end
    return value
end
A.writevar('HEARTBEAT',42);assert(A.readvar('HEARTBEAT')=='42');A.clearvar('HEARTBEAT')
GetVar=normal_getvar
-- Upgrade invalidates callbacks and releases its previous local sender via DelVar.
A.start(c);local old_epoch=A.state.epoch;A.state.version='0.32'
local upgraded=assert(loadcore())('MA OBS MediaLink','Bedienung')
assert(not upgraded.state.running and upgraded.state.epoch>old_epoch and upgraded.state.version=='0.42')
assert(variables.MAOBS_ML31_SENDER==nil and variables.MAOBS_ML31_LAST==1)
local before_upgrade_tick=#commands;A.tick(old_epoch);assert(#commands==before_upgrade_tick)
print('PASS: v0.33 zero-return GetVar normalization, first Start, missing LAST/TOKEN/HEARTBEAT, idempotent DelVar release, numeric SetVar result/readback, failed claim/timer rollback, stopped runtime on delete refusal, actual diagnosis release, compatible upgrade')

-- v0.34: actual MA phasers use raw24 for Decimal16 values, too.
for value=0,65535 do
    local raw=math.floor(value*16777215/65535+0.5)
    assert(A.dmx16(raw)==value)
end
assert(A.dmx16(-1)==nil and A.dmx16(16777216)==nil)
local sig,tok=A.decode_revision_label('ML31 1000.0,6.0,0.0,12.0')
assert(sig=='1000,6,0' and tok==12)
-- Different textual handle addresses and int/float spellings are the same cue.
local before_identity=A.read_cue(c,q1)
local previous_addr=q1.address;q1.address='Renamed cue display address';q1.no=1000.0
local after_identity=A.read_cue(c,q1)
assert(before_identity.key==after_identity.key and before_identity.signature==after_identity.signature)
q1.address=previous_addr;q1.no=1000
local function stores()
    local n=0;for _,text in ipairs(commands) do if text:match('^Store DataPool 1 Sequence ') then n=n+1 end end
    return n
end
local normal_cmd=Cmd
local broken=maincue(20,101,'OBS:6');main.current=broken
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c))
local attempted_before=stores()
function Cmd(text)
    local result=normal_cmd(text)
    local seq,no=text:match('^Store DataPool 1 Sequence (%d+) Cue (%d+) ')
    if seq then
        -- Write succeeds but the subsequent data readback is inconsistent.
        findcue(tonumber(seq),tonumber(no))[1].data.by_fixtures[301].VideoSceneNumber[1].absolute_value=0
    end
    return result
end
A.start(c);local broken_epoch=A.state.epoch;A.tick(broken_epoch)
assert(not A.state.running and stores()==attempted_before+1)
assert(A.state.last_error:find('Steuer-Cue-Prüfung fehlgeschlagen',1,true))
assert(A.state.last_error:find('Rohwerte',1,true))
for i=1,1300 do A.tick(broken_epoch) end
assert(stores()==attempted_before+1)
-- Even an explicit restart in the same failed configuration cannot make another
-- cue for that same signature. It stops with a retained write-attempt guard.
for i=1,3 do
    A.start(c);A.tick(A.state.epoch);assert(not A.state.running)
end
assert(stores()==attempted_before+1 and A.state.last_error:find('Schreibsperre:',1,true))
Cmd=normal_cmd
-- A different, explicitly chosen control sequence has its own attempt scope.
local alternative={};for k,v in pairs(c) do alternative[k]=v end;alternative.control=901
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(alternative))
A.start(alternative);A.tick(A.state.epoch);assert(A.state.running and sequences[901])
A.stop('Aus')
broken.Note=''
-- A healthy unchanged setup stays bounded over 3000 timer passes, including
-- restart/recovery from labels, name changes and interleaved numeric spellings.
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(c));main.current=q1
A.start(c);local healthy_epoch=A.state.epoch
for i=1,3000 do
    q1.no=i%2==0 and 1000.0 or 1000
    q1.address='display address '..i
    A.tick(healthy_epoch)
    assert(A.state.running)
end
assert(stores()==attempted_before+2) -- only the failed attempt + alternative seq
A.stop('Aus');q1.address=previous_addr;q1.no=1000
-- Re-entry while Store runs cannot start a second writer.
local nextcue=maincue(21,111,'OBS:7');main.current=nextcue
local reentered=false
function Cmd(text)
    if text:match('^Store DataPool 1 Sequence ') and not reentered then
        reentered=true;A.tick(A.state.epoch)
    end
    return normal_cmd(text)
end
local before_reentry=stores()
A.start(c);A.tick(A.state.epoch)
assert(reentered and A.state.running and stores()==before_reentry+1)
A.stop('Aus');Cmd=normal_cmd
-- A repeated invocation of the same Timer callback can advance the chain once.
A.start(c);local callback=timers[#timers];local timer_before=#timers
callback();assert(#timers==timer_before+1)
callback();assert(#timers==timer_before+1)
A.stop('Aus')
-- Use the monotonic MA clock, not a potentially rounded Timer interval, to gate
-- work. Too-early callbacks may reschedule but must not perform new operations.
local normal_time=Time;local frozen_time=10000
function Time() return frozen_time end
A.start(c);A.tick(A.state.epoch);local commands_at_freeze=#commands
for i=1,300 do A.tick(A.state.epoch) end
assert(#commands==commands_at_freeze)
frozen_time=frozen_time+0.1;A.tick(A.state.epoch)
assert(A.state.running)
A.stop('Aus');Time=normal_time
print('PASS: v0.34 raw24/Decimal16 conversion, canonical cue identity and legacy labels, failed verification capped at one Store across 1300 callbacks and restarts, separate control-sequence scope, stable 3000-pass run, re-entry guard, single-use Timer callback, monotonic time gate')

-- v0.35: MA strips commas from Cue labels, as shown by the user's actual log.
local safe_name=A.revision_label('4000,6,492000',1053)
assert(safe_name=='ML35 C4000 S6 R492000 T1053')
assert(not safe_name:find('[^%w ]'))
local parsed_signature,parsed_token=A.decode_revision_label(safe_name)
assert(parsed_signature=='4000,6,492000' and parsed_token==1053)
-- Reconstruct the precise v0.34 label from known cue data, never by guessing
-- where concatenated digits split. The frame below matches the reported case.
local recovery_cfg={};for k,v in pairs(c) do recovery_cfg[k]=v end;recovery_cfg.control=902
local recovery_main=maincue(4,131,'OBS:6')
local recovery_entry=assert(A.read_cue(recovery_cfg,recovery_main))
assert(recovery_entry.signature=='4000,6,492000')
local function scaled16(value) return math.floor(value*16777215/65535+0.5) end
local recovery_data={by_fixtures={[301]={
    VideoSceneNumber={{absolute_value=scaled16(6)}},
    Dimmer={{absolute_value=492000}},
    VideoTransitionToken={{absolute_value=scaled16(1053)}}}}}
local legacy_name='ML31 400064920001053'
local legacy_cue=cueobject(902,1002,recovery_data,legacy_name)
sequences[902].name='MA OBS MediaLink [DP1 S3 F300 C301]'
local recovered=assert(A.find_revision(recovery_cfg,recovery_entry))
assert(recovered.control==1002 and recovered.token==1053 and recovered.done)
-- Incorrect data, labels, extra values or commands cannot be claimed as ours.
local scene_value=recovery_data.by_fixtures[301].VideoSceneNumber[1]
local stored_scene=scene_value.absolute_value
scene_value.absolute_value=0;assert(A.matching_revision(recovery_cfg,legacy_cue,recovery_entry)==nil)
scene_value.absolute_value=stored_scene
for _,bad_name in ipairs({'ML31 500064920001053','ML31 400064920001054','Cue 1002','ML31 40006492000'}) do
    legacy_cue.name=bad_name;assert(A.matching_revision(recovery_cfg,legacy_cue,recovery_entry)==nil)
end
legacy_cue.name=legacy_name
legacy_cue.Command='Go+ Sequence 77';assert(A.matching_revision(recovery_cfg,legacy_cue,recovery_entry)==nil);legacy_cue.Command=nil
recovery_data.by_fixtures[777]={Dimmer={{absolute_value=123}}}
assert(A.matching_revision(recovery_cfg,legacy_cue,recovery_entry)==nil);recovery_data.by_fixtures[777]=nil
-- The loaded v0.34 process retains a failed write guard. It may recover that
-- exact, fully verified legacy cue without issuing another Store or relabel.
local recovery_key='DataPool 1 Sequence 902|MA OBS MediaLink [DP1 S3 F300 C301]|4000,6,492000'
A.state.write_attempts[recovery_key]={control=1002,token=1053,signature=recovery_entry.signature,done=false}
local before_recover=stores();local commands_before_recover=#commands
local reused=A.revision(recovery_cfg,recovery_entry)
assert(reused.control==1002 and stores()==before_recover and #commands==commands_before_recover)
-- Normal upgrade + first dispatch uses the existing cue, with no new Store.
local all_main_children=main.children;main.children={recovery_main};main.current=recovery_main
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(recovery_cfg))
A.state.version='0.34'
local B=assert(loadcore())('MA OBS MediaLink','Bedienung')
assert(B.state.version=='0.42')
B.start(recovery_cfg);B.tick(B.state.epoch)
assert(B.state.running and B.state.map[recovery_entry.key].control==1002)
assert(stores()==before_recover and legacy_cue.name==legacy_name)
B.stop('Aus')
-- Also recover after a fresh Lua runtime, where no in-memory attempt exists.
local previous_runtime=_G.MA_OBS_MEDIALINK31;_G.MA_OBS_MEDIALINK31=nil
local C=assert(loadcore())('MA OBS MediaLink','Bedienung')
C.start(recovery_cfg);C.tick(C.state.epoch)
assert(C.state.running and C.state.map[recovery_entry.key].control==1002 and stores()==before_recover)
C.stop('Aus');_G.MA_OBS_MEDIALINK31=previous_runtime;main.children=all_main_children
for _,command in ipairs(commands) do assert(not command:match('^Delete DataPool %d+ Sequence ')) end
print('PASS: v0.35 real comma-stripping behavior, alphanumeric labels, exact recovery of ML31 400064920001053 with data validation, rejection of unrelated/modified data, retained-guard recovery, upgrade and fresh-runtime reuse without Store/relabel/delete')

-- v0.36: gap mode fills the existing span at unit spacing, even with step 10.
local gap_cfg={};for k,v in pairs(c) do gap_cfg[k]=v end
gap_cfg.count=1024;gap_cfg.step=10
local bounded=A.plan_presets(gap_cfg,'gaps')
assert(bounded.step==1 and bounded.highest==190 and bounded.finish==190)
assert(#bounded.items==170 and #bounded.conflicts==1)
for _,item in ipairs(bounded.items) do
    assert(item.slot>=101 and item.slot<=291 and item.raw==A.raw24(item.index))
    assert((item.slot-101)%10~=0 and item.slot~=102)
end
local protected_presets={}
for slot,preset in pairs(dp.PresetPools[25]) do protected_presets[slot]=preset end
local small_gap={};for k,v in pairs(gap_cfg) do small_gap[k]=v end;small_gap.count=5
local first_gaps=A.plan_presets(small_gap,'gaps')
assert(first_gaps.items[1].slot==103 and first_gaps.items[5].slot==107)
assert(A.generate(small_gap,first_gaps)==5)
local following=A.plan_presets(small_gap,'gaps')
assert(following.items[1].slot==108) -- repeated invocation continues with real holes
local remaining=A.plan_presets(gap_cfg,'gaps');assert(#remaining.items==165)
local thread=coroutine.create(function() return A.generate(gap_cfg,remaining) end)
local ok,result
repeat ok,result=coroutine.resume(thread);assert(ok,result) until coroutine.status(thread)=='dead'
assert(result==165)
for slot,preset in pairs(protected_presets) do assert(dp.PresetPools[25][slot]==preset) end
local no_gaps=A.plan_presets(gap_cfg,'gaps')
assert(#no_gaps.items==0 and no_gaps.reason:find('Keine freien Zwischenplätze',1,true))
assert(dp.PresetPools[25][292]==nil) -- never silently append from gap mode
local append=A.plan_presets(c,'append')
assert(append.step==10 and append.items[1].slot==301) -- append raster unchanged
-- Empty target and foreign occupancy beyond the managed span do not enlarge it.
local empty_cfg={};for k,v in pairs(gap_cfg) do empty_cfg[k]=v end;empty_cfg.pool=21
local empty=A.plan_presets(empty_cfg,'gaps')
assert(#empty.items==0 and empty.reason:find('Noch keine passenden',1,true))
dp.PresetPools[25][1001]=foreign
assert(A.plan_presets(gap_cfg,'gaps').finish==190)
dp.PresetPools[25][1001]=nil
-- Version readout is optional and read-only, also useful for MA editor issues.
function BuildDetails() return {BigVersion='2.5.0.0',HostType='onPC'} end
assert(A.ma_build_version()=='2.5.0.0')
BuildDetails=nil;assert(A.ma_build_version()=='unbekannt')
print('PASS: v0.36 gaps ignore append spacing, exact 170-hole span, repeated partial/full generation, no overwrite/range growth, empty targets, stable append raster, optional MA build readout')

-- v0.39: visible names are no longer the ownership key. Old identities remain valid.
local tc=A.default_config();tc.configured=1;tc.base=101
if A.state.running then A.stop('Aus') end
A.clearvar('SENDER');A.clearvar('HEARTBEAT')
local target=sequences[900]
local user_note='Eigene Notiz mit "Zitat", Komma und\nzweiter Zeile'
target.name='MA OBS MediaLink [DP1 S3 F300 C301]';target.Note=user_note
local count_before=#target.children;local stores_before=stores()
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));A.start(tc)
assert(target.name=='MediaLink – Nicht löschen')
assert(target.Note==user_note..'\n'..A.control_tag(tc))
assert(#target.children==count_before and stores()==stores_before)
A.stop('Aus')
local owned_note=target.Note
target.name='Eigener Anzeigename';assert(A.control_owned(tc,target))
A.start(tc);assert(target.name=='MediaLink – Nicht löschen' and target.Note==owned_note);A.stop('Aus')
target.Note=user_note -- matching visible name alone must not claim a sequence
expect_error(function() A.start(tc) end,'Steuersequenz ist belegt')
target.name='MA OBS MediaLink [DP1 S3 F300 C301]'
target.Note='[MAOBS_ML31 DP1 S99 F300 C301]'
expect_error(function() A.start(tc) end,'Steuersequenz ist belegt')
target.Note=user_note;target.refuse={Note=true}
expect_error(function() A.start(tc) end,'Kennung der Steuersequenz')
assert(not A.state.running and A.readvar('SENDER')==nil and A.readvar('LAST')==0)
assert(target.Note==user_note and target.name=='MA OBS MediaLink [DP1 S3 F300 C301]')
target.refuse=nil;target.Note=owned_note;target.name='MediaLink – Nicht löschen'
assert(stores()==stores_before and #target.children==count_before)
target.Note=user_note..'\n[MAOBS_ML31 DP1 S99 F300 C301]'
assert(not A.control_owned(tc,target));A.label_control(tc,true)
assert(A.control_owned(tc,target) and target.Note==owned_note and #target.children==count_before)
-- CRLF notes take the normalization path without modifying the loop variable.
local crlf_note=user_note..'\r\n'..A.control_tag(tc)..'\r\n'
target.Note=crlf_note;assert(A.control_owned(tc,target))
A.label_control(tc);assert(target.Note==crlf_note and #target.children==count_before and stores()==stores_before)
target.Note=user_note..'\r\n[MAOBS_ML31 DP1 S99 F300 C301]\r\n'
assert(not A.control_owned(tc,target));target.Note=owned_note
print('PASS: v0.39 legacy ownership migration, exact human label, preserved notes/cues, renamed owned sequences, foreign identity rejection, failed metadata write rollback, unchanged Store guard keys')

-- The ordinary UI entry now applies the saved policy, but reopening never stops a sender.
function MessageBox(t) displayed[#displayed+1]=t;return {success=true,result=0} end
Enums={AlignmentH={Left=0},AlignmentV={Top=0}}
for _,policy in ipairs({1,2,3}) do
    tc.startup=policy;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));SetVar(nil,'MAOBS_ML31_LAST',0)
    A.main();assert(A.state.running==(policy==1));if A.state.running then A.stop('Aus') end
end
tc.startup=3;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));SetVar(nil,'MAOBS_ML31_LAST',1)
A.main();assert(A.state.running)
local last_epoch=A.state.epoch;local previous_sender=A.readvar('SENDER');local previous_events=A.state.events
tc.startup=2;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));A.state.config.startup=2
A.main();assert(A.state.running and A.state.epoch>last_epoch and A.readvar('SENDER')==previous_sender)
assert(A.state.events==previous_events)
-- Reopening cannot strand the timer chain if an older tick is still running.
A.state.in_tick=true;local next_timer=timers[#timers];local queued=#timers
next_timer();assert(#timers==queued+1 and A.state.running);A.state.in_tick=false
A.stop('Aus')
-- Auto-start failure leaves the operating window accessible.
SetVar(nil,'MAOBS_ML31_SENDER','another-station');SetVar(nil,'MAOBS_ML31_HEARTBEAT',os.time())
tc.startup=1;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));local boxes_before=#displayed
A.main();assert(not A.state.running and #displayed>boxes_before and not A.state.ui)
assert(displayed[#displayed].title:find('Betrieb',1,true))
A.clearvar('SENDER');A.clearvar('HEARTBEAT')
-- Simulated save/reload: new GlobalVars handle, remembered own sender, LAST stays on.
tc.startup=3;SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));SetVar(nil,'MAOBS_ML31_LAST',1)
A.main();assert(A.state.running);local epoch_before_reload=A.state.epoch
local old_vars=variables;variables={};for key,value in pairs(old_vars) do variables[key]=value end
A.main();assert(A.state.running and A.state.vars==variables and A.readvar('LAST')==1)
assert(A.state.epoch>epoch_before_reload and A.readvar('SENDER')~=previous_sender)
local before_old_tick=#commands;A.tick(epoch_before_reload);assert(#commands==before_old_tick)
A.stop('Aus')
print('PASS: v0.39 ordinary open policies, running sender preserved/rearmed, reachable UI on failed start, saved-state restore after context replacement, old callbacks inert')

-- Native MA UI adapter: real dialog builder, asynchronous signals and cancellation.
local native_fault
local function ui_node(kind,parent)
    local node={class=kind,parent=parent,children={},valid=true,layout={}}
    setmetatable(node,{__index=function(self,key)
        if type(key)=='number' then
            self.layout[key]=self.layout[key] or setmetatable({}, {__index=function(t,k) local v={};rawset(t,k,v);return v end})
            return self.layout[key]
        end
    end})
    function node:Append(child_kind)
        if native_fault==child_kind then error('Injected native UI error') end
        local child=ui_node(child_kind,self);self.children[#self.children+1]=child;return child
    end
    function node:WaitInit() end
    function node:Close() self.valid=false;for _,child in ipairs(self.children) do child:Close() end end
    return node
end
local overlay=ui_node('Overlay');local focus=ui_node('Display');focus.ScreenOverlay=overlay;focus.W=1280;focus.H=900
function GetFocusDisplay() return focus end
function IsObjectValid(node) return node and node.valid end
function FindBestFocus() end
local entered={};local input_calls=0
function TextInput(_,current) input_calls=input_calls+1;return table.remove(entered,1) or current end
local handlers,component={},{}
local U=assert(loadcore())('MA OBS MediaLink','Bedienung',handlers,component)
local function active_base()
    for i=#overlay.children,1,-1 do if overlay.children[i].valid then return overlay.children[i] end end
end
local function find_ui(root,kind,text)
    if root.class==kind and root.Text==text then return root end
    for _,child in ipairs(root.children) do local found=find_ui(child,kind,text);if found then return found end end
end
local function check_text_boxes(base)
    local function visit(node)
        if node.class=='UIObject' or node.class=='Button' then
            assert(node.Font=='Regular18' or node.Font=='Medium20','Unexpected font: '..tostring(node.Font))
            local lines=0
            for line in (node.Text..'\n'):gmatch('(.-)\n') do
                local count=assert(utf8.len(line),'Invalid UTF-8 in displayed text')
                local columns=math.max(10,math.floor((node.W-20)/(node.Font=='Medium20' and 11 or 9)))
                assert(count<=columns,'Text exceeds planned width: '..line)
                lines=lines+1
            end
            local needed=lines*(node.Font=='Medium20' and 28 or 24)+16
            assert(node.H>=needed,'Text field too short: '..node.Text)
            assert(node.TextalignmentV=='Center','Missing vertical breathing room')
            assert(node.Y+node.H<=base.H-48,'Text field exceeds dialog frame')
        end
        for _,child in ipairs(node.children) do visit(child) end
    end
    visit(base)
end
local function click(root,text)
    local button=assert(find_ui(root,'Button',text),'Button not found: '..text)
    assert(button.PluginComponent==component and handlers[button.Clicked])
    handlers[button.Clicked](button);return button
end
local function begin_ui(fn)
    local co=coroutine.create(fn);local ok,why=coroutine.resume(co);assert(ok,why)
    assert(coroutine.status(co)=='suspended' and active_base(),'Native dialog unexpectedly fell back')
    check_text_boxes(active_base())
    return co,active_base()
end
local function finish_ui(co)
    local ok,why=coroutine.resume(co);assert(ok,why)
    assert(coroutine.status(co)=='dead' and not active_base() and next(handlers)==nil)
end
tc.startup=2;tc.count=3;tc.step=10;tc.mode=2
SetVar(nil,'MAOBS_ML31_CONFIG',A.encode(tc));local saves_before=stores()
local co,base=begin_ui(function() tc=U.presets_ui(tc) end)
assert(base.W==1080 and base.H<focus.H)
local pool_label=assert(find_ui(base,'UIObject','All-Presetpool'))
assert(pool_label.Font=='Regular18','Unexpected field-label font: '..tostring(pool_label.Font))
local step=assert(find_ui(base,'Button','10'));assert(step.Enabled=='No')
local called=input_calls;click(base,'10');assert(input_calls==called) -- disabled even if a signal leaks through
click(base,'Hinten ergänzen');assert(step.Enabled=='Yes')
entered={'7'};click(base,'10');assert(step.Text=='7')
click(base,'Lücken ergänzen');assert(step.Enabled=='No')
click(base,'All 4');click(base,'Speichern');finish_ui(co)
assert(tc.pool==24 and tc.mode==2 and tc.step==7 and stores()==saves_before)
-- Setup labels and policies use the same numeric persistence format.
co,base=begin_ui(function() tc=U.setup(tc) end)
assert(find_ui(base,'UIObject','Datenpool'))
assert(not find_ui(base,'UIObject','Datenpool (nicht All-Pool)'))
click(base,'Bridge starten');click(base,'Speichern');finish_ui(co);assert(tc.startup==1)
-- Closing an operating window leaves the sender on; Stop keeps it off within that window.
co,base=begin_ui(function() U.main() end)
assert(U.state.running and base.W==1120)
assert(find_ui(base,'Button','Steuersequenz'))
assert(find_ui(base,'UIObject','Vor Änderungen an Einrichtung oder Presets die Bridge stoppen.').Font=='Medium20')
click(base,'Stoppen')
local ok,why=coroutine.resume(co);assert(ok,why);base=assert(active_base());assert(not U.state.running)
assert(find_ui(base,'Button','Starten'));click(base,'Schließen');finish_ui(co)
co,base=begin_ui(function() U.main() end);assert(U.state.running)
click(base,'Schließen');finish_ui(co);assert(U.state.running);U.stop('Aus')
-- Escape/external close cancels drafts without writing configuration.
local saved_config=U.readvar('CONFIG')
co,base=begin_ui(function() U.presets_ui(tc) end);base:Close();finish_ui(co)
assert(U.readvar('CONFIG')==saved_config)
-- Context change while a form is open also cancels every pending edit.
co,base=begin_ui(function() U.setup(tc) end)
local old=variables;variables={};for key,value in pairs(old) do variables[key]=value end
finish_ui(co);assert(U.readvar('CONFIG')==saved_config)
-- A narrow web display wraps complete button labels into two command rows.
focus.W=960;focus.H=768;tc.startup=2;SetVar(nil,'MAOBS_ML31_CONFIG',U.encode(tc));U.state.status='Aus'
co,base=begin_ui(function() U.main() end)
assert(base.W==920 and base.H<focus.H and find_ui(base,'Button','Steuersequenz'))
click(base,'Schließen');finish_ui(co)
-- Native construction failure closes only its own overlay, then uses MessageBox.
focus.W=1280;focus.H=900;native_fault='TitleBar';local fallback_before=#displayed
U.setup(tc);native_fault=nil
assert(not active_base() and next(handlers)==nil and #displayed==fallback_before+1)
assert(displayed[#displayed].message_align_h==Enums.AlignmentH.Left)
assert(not displayed[#displayed].message:find('nicht All-Pool',1,true))
print('PASS: v0.39 native layouts, widths, complete command labels, emphasized hint, five All pools, dynamic gap spacing disable, input editing, preserved sender on close, Stop behavior, Escape/context cancel, callback cleanup, isolated fallback')

-- v0.40: exercise the public import entry, without the core's test export.
-- A MA API failure during component evaluation used to prevent `return main`.
-- No MA API may run while the replacement component itself is being imported.
do
    local env=setmetatable({}, {__index=_G})
    env._G=env;env.MAOBS_TEST_MODE=false
    local blocked=true;local api_calls=0
    local diagnostics,dialogs={},{}
    local own_vars={}
    env.MA_OBS_MEDIALINK31={running=false,epoch=7,version='0.39',events=0,status='Aus'}
    local old_state=env.MA_OBS_MEDIALINK31
    local function api_gate()
        api_calls=api_calls+1
        assert(not blocked,'Injected MA API unavailable during component import')
    end
    env.GlobalVars=function() api_gate();return own_vars end
    env.GetVar=function(v,k) api_gate();return v[k] end
    env.SetVar=function(v,k,value) api_gate();v[k]=value;return true end
    env.DelVar=function(v,k) api_gate();v[k]=nil;return true end
    env.ErrPrintf=function(format,value) diagnostics[#diagnostics+1]=string.format(format,value) end
    env.Printf=env.ErrPrintf
    env.MessageBox=function(t) dialogs[#dialogs+1]=t;return {success=true,result=0} end
    env.GetFocusDisplay=false -- first check uses only the standard dialog
    local public={}
    for _,name in ipairs({'Bedienung','Autostart'}) do
        public[name]=assert(production_loadfile('MA3/MA_OBS_MediaLink.lua','t',env))('MA OBS MediaLink',name,{},component)
        assert(type(public[name])=='function')
    end
    assert(api_calls==0 and env.MA_OBS_MEDIALINK31==old_state and old_state.epoch==7)
    public.Bedienung()
    assert(#dialogs==1 and dialogs[1].message:find('Initialisierung',1,true))
    assert(diagnostics[1]:find('Injected MA API unavailable',1,true))
    assert(diagnostics[1]:find('stack traceback',1,true))
    -- The failed factory is retryable after the MA API becomes available.
    blocked=false;public.Bedienung()
    assert(old_state.version=='0.42' and dialogs[#dialogs].title:find('Betrieb',1,true))
    assert(not old_state.running and not old_state.ui)
    -- Already initialized runtime failures must not escape the entry either.
    blocked=true;public.Bedienung()
    assert(dialogs[#dialogs].message:find('Schritt: Betrieb',1,true))
    blocked=false
    -- Compiler failures inside the embedded core have a visible error path.
    local real_load=env.load
    env.load=function() return nil,'Injected core syntax failure' end
    local broken=assert(production_loadfile('MA3/MA_OBS_MediaLink.lua','t',env))('MA OBS MediaLink','Bedienung',{},component)
    assert(type(broken)=='function');broken()
    assert(dialogs[#dialogs].message:find('Codeprüfung',1,true))
    assert(diagnostics[#diagnostics]:find('Injected core syntax failure',1,true))
    env.load=real_load
    -- The actual public entry can yield through a native UI, and component
    -- handles/signals still reach the core's callbacks unchanged.
    env.GetFocusDisplay=GetFocusDisplay
    local public_signals,public_component={},{}
    local operating=assert(production_loadfile('MA3/MA_OBS_MediaLink.lua','t',env))('MA OBS MediaLink','Bedienung',public_signals,public_component)
    local thread=coroutine.create(function() operating(focus) end)
    local success,reason=coroutine.resume(thread);assert(success,reason)
    local window=assert(active_base());assert(coroutine.status(thread)=='suspended')
    local close=assert(find_ui(window,'Button','Schließen'))
    assert(close.PluginComponent==public_component and public_signals[close.Clicked])
    public_signals[close.Clicked](close)
    success,reason=coroutine.resume(thread);assert(success,reason)
    assert(coroutine.status(thread)=='dead' and not active_base() and next(public_signals)==nil)
    assert(not old_state.running and not old_state.ui)
end
print('PASS: v0.40 public import always returns main without MA API access, protected core compiler/initialization/runtime failures, full diagnostic traceback, initialization retry, native UI yields and unchanged component signals')

-- v0.42: sizes depend on text lines, not one fixed height per form row.
-- Every normal native UI above also passes the font/UTF-8/width/height checks.
for _,display_width in ipairs({1280,960,800}) do
    focus.W=display_width;focus.H=900
    co,base=begin_ui(function() U.setup(tc) end)
    assert(find_ui(base,'UIObject','Auszulesende Sequenz').Font=='Regular18')
    assert(find_ui(base,'UIObject','Beim Öffnen des Plugins').Font=='Regular18')
    click(base,'Abbrechen');finish_ui(co)
    co,base=begin_ui(function() U.presets_ui(tc) end)
    click(base,'Abbrechen');finish_ui(co)
end
co,base=begin_ui(function()
    U.box('Textprüfung','Änderungen · Öffnen · Für Farin und Bela.\n\n'..string.rep('Ü',90),{ui_rows={},ui_width=720})
end)
click(base,'Schließen');finish_ui(co)
print('PASS: v0.42 normal field fonts, all native text boxes have line-based height/padding and valid UTF-8, complete labels/choices at 1280/960/800 display widths, safe wrapping of long words with umlauts')
