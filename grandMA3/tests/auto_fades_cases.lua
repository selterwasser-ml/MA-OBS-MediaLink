-- Included by test_medialink.lua through its local adapter seam.
return function(A,c,main,cueobject,commands,selection_ok,variables,setfault)
    local function check_error(fn,text)
        local ok,why=pcall(fn);assert(not ok and tostring(why):find(text,1,true),tostring(why))
    end
    local function reset()
        if A.state.running then A.stop('Aus') end
        main.children={};main.current=nil
        A.clearvar('SENDER');A.clearvar('HEARTBEAT')
        variables.MAOBS_ML31_CONFIG=A.encode(c)
    end
    local function cue(no,index,note)
        local a=index~=nil and {{absolute_value=A.raw24(index)}} or nil
        return cueobject(3,no,{by_fixtures={[300]={VideoCrossfade=a},[77]={Dimmer={{absolute_value=123}}}}},'Keep name',note or 'OBS:1')
    end
    local function index(q) return assert(A.read_cue(c,q)).index end
    reset()
    local first=cue(1);local last=cue(2)
    local p=A.sync_fades(c);assert(#p.writes==2 and index(first)==10 and index(last)==20)
    local before=#commands;A.sync_fades(c);assert(#commands==before)
    local middle=cue(1.5);A.sync_fades(c)
    assert(index(middle)==15 and index(first)==10 and index(last)==20)
    local fade=middle[1].data.by_fixtures[300].VideoCrossfade
    fade.fade=4.5;fade.delay=1.25;fade[1].fade=3.5
    middle.Note='Other note';A.sync_fades(c)
    assert(not middle[1].data.by_fixtures[300].VideoCrossfade)
    assert(middle.name=='Keep name' and middle.Note=='Other note' and middle[1].data.by_fixtures[77].Dimmer[1].absolute_value==123)
    middle.Note='OBS:8';A.sync_fades(c);assert(index(middle)==15)
    -- Existing assignments survive a fresh runtime; no external ledger needed.
    local saved_state=_G.MA_OBS_MEDIALINK31;_G.MA_OBS_MEDIALINK31=nil
    local source=assert(io.open('source/medialink_core.lua','rb'));local text=source:read('*a');source:close()
    local fresh=assert(load(text,'@fresh core'))('MediaLink','Bedienung')
    before=#commands;fresh.sync_fades(c);assert(#commands==before)
    _G.MA_OBS_MEDIALINK31=saved_state
    -- Legacy references become direct values without touching pool objects.
    local legacy={name='Existing pool object'}
    first[1].data.by_fixtures[300].VideoCrossfade.abs_preset=legacy
    first[1].data.by_fixtures[300].VideoCrossfade.fade=7.75
    A.sync_fades(c);assert(index(first)==10 and legacy.name=='Existing pool object')
    assert(first[1].data.by_fixtures[300].VideoCrossfade.fade==7.75)
    -- Nine available integer slots can all be occupied without a rebuild.
    reset();first=cue(1,10);last=cue(2,20)
    for i=1,9 do cue(1+i/10) end
    p=A.sync_fades(c);assert(not p.rebuilt and #p.rows==11)
    for i,row in ipairs(p.rows) do assert(row.target==i+9) end
    cue(1.05);p=A.sync_fades(c);assert(p.rebuilt and #p.rows==12)
    for i,row in ipairs(p.rows) do assert(row.target==i*10) end
    -- Repeated insertion in the same tiny gap exhausts that gap earlier.
    reset();first=cue(1,10);last=cue(2,20)
    for _,n in ipairs({1.5,1.25,1.125}) do cue(n);assert(not A.sync_fades(c).rebuilt) end
    cue(1.1);assert(A.sync_fades(c).rebuilt)
    -- Copying/reordering values cannot leave duplicate/non-monotonic targets.
    reset();first=cue(1,20);last=cue(2,10);assert(A.sync_fades(c).rebuilt)
    assert(index(first)==10 and index(last)==20)
    local duplicate=cue(3,20);assert(A.sync_fades(c).rebuilt and index(duplicate)==30)
    local kept=first[1].data.by_fixtures[300].VideoCrossfade
    kept.fade={in_time=6,out_time=2};kept[1].delay=0.75
    duplicate.no=500;A.sync_fades(c)
    assert(kept.fade.in_time==6 and kept.fade.out_time==2 and kept[1].delay==0.75)
    -- Invalid OBS notes remain diagnostic errors, not cleanup requests.
    reset();first=cue(1,10,'OBS:0');before=#commands
    A.sync_fades(c);assert(#commands==before)
    local entries,issues=A.inventory(c);assert(#entries==0 and #issues==1)
    -- Do not silently mix tracking from another cue part into managed values.
    reset();first=cue(1,10)
    first.children[2]={data={by_fixtures={[300]={VideoCrossfade={{absolute_value=0}}}}}}
    before=#commands;check_error(function() A.sync_fades(c) end,'nur in Part 0');assert(#commands==before)
    -- Programmer protection is checked before mutating source cues.
    reset();first=cue(1)
    setfault('programmer',true);before=#commands
    check_error(function() A.sync_fades(c) end,'Programmer-Daten')
    assert(#commands==before);setfault('programmer',false)
    -- Capacity and initial compact allocation, including both raw endpoints.
    for _,n in ipairs({1,102,103,511,1023,1024}) do
        reset();for i=1,n do cue(i) end
        p=A.sync_fades(c);assert(#p.rows==n)
        local prior=-1
        for _,row in ipairs(p.rows) do
            local actual=index(row.cue);assert(actual>prior and actual<=1023);prior=actual
        end
        if n==1024 then assert(index(main.children[1])==0 and index(main.children[1024])==1023) end
        before=#commands;A.sync_fades(c);assert(#commands==before)
    end
    cue(1025);before=#commands
    check_error(function() A.sync_fades(c) end,'Mehr als 1024');assert(#commands==before)
    -- Model 600 deterministic edit cycles, including bursts and removals.
    reset();math.randomseed(43);local nextno=1
    for cycle=1,600 do
        if #main.children>5 and cycle%3==0 then
            local q=main.children[math.random(#main.children)];q.Note='No scene'
        else
            cue(nextno/1000);nextno=nextno+math.random(1,7)
        end
        p=A.sync_fades(c);local previous=-1
        for _,row in ipairs(p.rows) do assert(index(row.cue)>previous);previous=index(row.cue) end
        for _,q in ipairs(main.children) do
            if q.Note=='No scene' then assert(not q[1].data.by_fixtures[300].VideoCrossfade) end
        end
    end
    -- Changes precede metadata creation / dispatch in the actual timer.
    reset();first=cue(1);main.current=first
    A.start(c);A.tick(A.state.epoch);assert(A.state.running and index(first)==10)
    assert(A.state.map[A.cue_key(c,1000)].signature==A.read_cue(c,first).signature)
    first.Note='No scene'
    for i=1,10 do A.tick(A.state.epoch) end
    assert(not first[1].data.by_fixtures[300].VideoCrossfade)
    A.stop('Aus')
    -- Legacy saved settings are migrated before the timer consistency check.
    reset();variables.MAOBS_ML31_CONFIG=A.encode(c)..';pool=25;base=1000;step=10;count=10;mode=1'
    A.start(A.decode(variables.MAOBS_ML31_CONFIG));A.tick(A.state.epoch)
    assert(A.state.running and variables.MAOBS_ML31_CONFIG==A.encode(c));A.stop('Aus')
    -- A failed source Store must stop the writer, with no timer retries.
    reset();first=cue(1);main.current=first
    setfault('store',true);A.start(c);local epoch=A.state.epoch;A.tick(epoch)
    assert(not A.state.running);before=#commands
    for i=1,100 do A.tick(epoch) end
    assert(#commands==before);setfault('store',false)
    -- A command can report success but fail readback, including timing damage.
    for _,kind in ipairs({'readback','timing'}) do
        reset();first=cue(1)
        first[1].data.by_fixtures[300].VideoCrossfade={{absolute_value=123},fade=4.5}
        main.current=first
        local original=Cmd
        function Cmd(command)
            local result=original(command)
            if command:find('Store DataPool 1 Sequence 3 Cue ',1,true) then
                local data=first[1].data.by_fixtures[300].VideoCrossfade
                if kind=='readback' then data[1].absolute_value=123 else data.fade=0 end
            end
            return result
        end
        A.start(c);epoch=A.state.epoch;A.tick(epoch)
        assert(not A.state.running)
        assert(A.state.last_error:find(kind=='readback' and 'nicht bestätigt' or 'individuelle Zeit',1,true))
        before=#commands;for i=1,100 do A.tick(epoch) end;assert(#commands==before)
        Cmd=original
    end
    assert(selection_ok())
    for _,command in ipairs(commands) do
        assert(not command:find('Store DataPool 1 Preset',1,true))
        assert(not command:find('Delete DataPool 1 Preset',1,true))
    end
    reset()
    print('PASS: automatic steps/midpoints, nine slots/local gap exhaustion, cleanup/reuse, copy/move, legacy conversion/restart, timing/unrelated data, 1024 capacity/overflow, 600 edit cycles, migration, dispatch ordering, failed source Store stop and selection restoration')
end
