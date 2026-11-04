-- MA OBS MediaLink v0.42 BETA | Copyright © 2026 Oliver Cassel
-- Für Farin und Bela.
-- One pool plugin, two entry components (UI / startup), one shared runtime.
-- OBS transport remains the tested 3+7 channel sACN layout.
local _, componentName, signals, component_handle = ...
local VERSION='0.42'
local CONTROL_LABEL='MediaLink – Nicht löschen'
local ui_display
local PREFIX='MAOBS_ML31_'
local RAWMAX=16777215
local S -- runtime initialized below; also owns the per-signature write ledger
local function safe(fn) local ok,v=pcall(fn);if ok then return v end end
local function prop(o,k) return o and safe(function() return o[k] end) end
local function address(o) return o and safe(function() return o:Addr() end) end
local function integer(v,lo,hi)
    v=tonumber(v);return v and v==math.floor(v) and v>=lo and v<=hi and v or nil
end
local function raw24(i) return math.floor(i*RAWMAX/1023+0.5) end
local function copy(t) local r={};for k,v in pairs(t) do r[k]=v end;return r end
local function log(s) Printf('MA OBS MediaLink '..VERSION..': '..tostring(s)) end
local function fail(s) ErrPrintf('MA OBS MediaLink '..VERSION..': '..tostring(s)) end
local function clean_error(value)
    local first=tostring(value or 'Unbekannter Fehler'):match('([^\n]+)') or 'Unbekannter Fehler'
    -- A Lua assert prefixes its message with source:line. Keep that in the log,
    -- not in the user-facing explanation. Do not expose the traceback in a modal.
    return (first:gsub('^.-:%d+:%s*',''))
end
local function report_error(value)
    local state=rawget(_G,'MA_OBS_MEDIALINK31')
    if state then state.last_error=tostring(value) end
    fail(value)
    return clean_error(value)
end
local function cmd(s)
    local ok,r=pcall(Cmd,s)
    if not ok then error('Befehl fehlgeschlagen: '..s..' / '..tostring(r),0) end
    if type(r)=='string' then
        local low=r:lower()
        if low:find('failed',1,true) or low:find('error',1,true) or low:find('illegal',1,true) then
            error('Befehl fehlgeschlagen: '..s..' / '..r,0)
        end
    end
end
local function objects(s)
    local result=ObjectList(s)
    assert(type(result)=='table','Objektabfrage fehlgeschlagen: '..s)
    return result
end
local function one(s)
    local result=objects(s);assert(#result<=1,'Mehrdeutige Adresse: '..s);return result[1]
end
local function gv() return GlobalVars() end
local function readvar(key)
    -- Some MA API builds return zero values for an absent variable. Assigning
    -- first turns that into exactly one nil, so tonumber(readvar(...)) is safe.
    local value=GetVar(gv(),PREFIX..key)
    return value
end
local function clearvar(key)
    if readvar(key)==nil then return end -- DelVar(false) for already absent is OK
    DelVar(gv(),PREFIX..key)
    assert(readvar(key)==nil,'Variable konnte nicht freigegeben werden: '..key)
end
local function writevar(key,value)
    assert(value~=nil and value~='','Leere Werte müssen mit DelVar freigegeben werden: '..key)
    local result=SetVar(gv(),PREFIX..key,value)
    local actual=readvar(key)
    local matches=actual==value or (type(value)=='number' and tonumber(actual)==value)
    assert(result~=false and result~=0 and matches,'Einstellung konnte nicht gespeichert werden: '..key)
end
local config_fields={'dp','main','fade','controller','pool','base','control','step','count','startup','mode'}
local function default_config()
    return {dp=1,main=3,fade=300,controller=301,pool=25,base=1000,control=900,
        step=10,count=10,startup=3,mode=1,configured=0}
end
local function encode(c)
    local parts={'v31'}
    for _,key in ipairs(config_fields) do parts[#parts+1]=key..'='..tostring(c[key]) end
    parts[#parts+1]='configured='..tostring(c.configured or 0)
    return table.concat(parts,';')
end
local function decode(text)
    local c=default_config()
    if text==nil then return c end
    assert(type(text)=='string' and text:match('^v31;'),'Unbekanntes Einstellungsformat')
    for key,value in text:gmatch('([a-z]+)=(%d+)') do
        if c[key]~=nil then c[key]=tonumber(value) end
    end
    return c
end
local function validate(c)
    for _,key in ipairs({'dp','main','fade','controller','base','control'}) do
        assert(integer(c[key],1,999999),key..': ganze Zahl zwischen 1 und 999999 erforderlich')
    end
    assert(c.dp~=128,'Datenpool 128 ist für Preview reserviert. Einen Show-Datenpool wählen.')
    assert(integer(c.mode,1,2),'Preset-Vorgehen ungültig')
    assert(integer(c.pool,21,25),'All-Presetpool muss 21, 22, 23, 24 oder 25 sein')
    assert(integer(c.step,1,1024),'Abstand muss 1 bis 1024 sein')
    assert(integer(c.count,1,1024),'Anzahl muss 1 bis 1024 sein')
    assert(integer(c.startup,1,3),'Startoption ungültig')
    assert(c.base+1023<=999999,'Startposition lässt keinen Bereich von 1024 Plätzen zu')
    assert(c.main~=c.control,'Lese- und Steuersequenz müssen verschieden sein')
    assert(c.fade~=c.controller,'Fade- und Steuer-Fixture müssen verschieden sein')
    return c
end
local function sequence(c,n) return one('DataPool '..c.dp..' Sequence '..n) end
local function preset(c,slot) return one('DataPool '..c.dp..' Preset '..c.pool..'.'..slot) end
local function seqaddr(c) return 'DataPool '..c.dp..' Sequence '..c.control end
local function presetaddr(c,slot) return 'DataPool '..c.dp..' Preset '..c.pool..'.'..slot end
local function ownership(c)
    return 'MA OBS MediaLink [DP'..c.dp..' S'..c.main..' F'..c.fade..' C'..c.controller..']'
end
local function control_tag(c)
    return '[MAOBS_ML31 DP'..c.dp..' S'..c.main..' F'..c.fade..' C'..c.controller..']'
end
local function control_owned(c,target)
    local note=tostring(prop(target,'Note') or '')
    local marked=false
    for raw_line in (note..'\n'):gmatch('(.-)\n') do
        local line=raw_line:gsub('\r$','')
        if line:match('^%[MAOBS_ML31 ') then
            if line~=control_tag(c) then return false end
            marked=true
        end
    end
    return marked or prop(target,'name')==ownership(c)
end
local function current_pool(c)
    local dp=DataPool()
    assert(dp and tonumber(prop(dp,'no'))==c.dp,
        'Aktiven Datenpool auf '..c.dp..' stellen. Der gewählte Datenpool wird nicht automatisch gewechselt.')
    return dp
end
local function fixture(id,attributes)
    local handle=one('Fixture '..id);assert(handle,'Fixture '..id..' fehlt')
    local channels=GetUIChannels(handle,true)
    assert(type(channels)=='table','UI-Kanäle von Fixture '..id..' nicht lesbar')
    local result={}
    for _,channel in pairs(channels) do
        local name=tostring(prop(channel,'SUBATTRIBUTE') or ''):lower()
        for _,attr in ipairs(attributes) do
            if name==attr:lower() then
                local index=tonumber(prop(channel,'INDEX'))
                assert(index and index>=1,'UI-Kanalindex nicht lesbar')
                result[attr]=index-1
            end
        end
    end
    for _,attr in ipairs(attributes) do assert(result[attr], 'Fixture '..id..': Attribut '..attr..' fehlt') end
    return handle,result,channels
end
local function verify_patch(c)
    local _,fade=fixture(c.fade,{'VideoCrossfade'})
    local _,control=fixture(c.controller,{'VideoSceneNumber','Dimmer','VideoTransitionToken'})
    local wanted={}
    for name,index in pairs(fade) do wanted[index]={name=name,width=3,offset=0} end
    for name,index in pairs(control) do
        wanted[index]={name=name,width=name=='Dimmer' and 3 or 2,
            offset=name=='VideoSceneNumber' and 3 or name=='Dimmer' and 5 or 8}
    end
    local base
    for _,id in ipairs({c.fade,c.controller}) do
        local indices=GetRTChannels(one('Fixture '..id),false)
        assert(type(indices)=='table','RT-Kanäle nicht lesbar')
        for _,index in pairs(indices) do
            local channel=GetRTChannel(index)
            local rule=channel and wanted[channel.ui_index_first]
            assert(rule,'Fixture '..id..' enthält zusätzliche oder nicht zuordenbare Kanäle')
            local patch=channel.patch
            local coarse=patch and integer(patch.coarse,0,32768*512-1)
            assert(coarse,'Fixture '..id..' ist nicht passend auf DMX gepatcht')
            base=base or coarse-rule.offset
            assert(coarse==base+rule.offset,'DMX-Reihenfolge muss Fade(3), Szene(2), Endwert(3), Token(2) sein')
            assert(tonumber(patch.fine)==coarse+1,'Fine-Adresse/Auflösung falsch: '..rule.name)
            if rule.width==3 then assert(tonumber(patch.ultra)==coarse+2,'Ultra-Adresse fehlt: '..rule.name)
            else assert(not tonumber(patch.ultra) or tonumber(patch.ultra)<=0,'Steuerattribut muss 16 Bit haben: '..rule.name) end
            rule.checked=true
        end
    end
    for _,rule in pairs(wanted) do assert(rule.checked,'RT-Kanal fehlt: '..rule.name) end
    -- RT patch addresses are offsets starting at zero. DMX U1.001 is raw 0;
    -- U12.001 is raw 5632. Convert only when presenting the address to the user.
    assert(base and base>=0 and math.floor(base/512)==math.floor((base+9)/512),
        'Der Zehn-Kanal-Block überschreitet eine Universumsgrenze. '..
        'Die OBS-Startadresse muss zwischen 1 und 503 liegen.')
    return 'sACN U'..(math.floor(base/512)+1)..' · Startadresse '..(base%512+1)
end
local function preflight(c)
    validate(c);current_pool(c)
    for _,name in ipairs({'GetPresetData','GetProgPhaser','GetUIChannels','GetRTChannels','GetRTChannel','Timer','SelectionCount','Time'}) do
        assert(type(_G[name])=='function','MA-Funktion fehlt: '..name)
    end
    local main=sequence(c,c.main);assert(main,'Lesesequenz '..c.main..' fehlt')
    -- Deliberately no GetCurrentCue()/SelectedSequence() fallback.
    local ok=pcall(function() return main:CurrentChild() end)
    assert(ok,'CurrentChild der festen Lesesequenz ist auf dieser MA-Version nicht verfügbar')
    fixture(c.fade,{'VideoCrossfade'})
    fixture(c.controller,{'VideoSceneNumber','Dimmer','VideoTransitionToken'})
    local dp=DataPool();assert(dp.PresetPools[c.pool],'All-Presetpool fehlt')
    verify_patch(c)
    return main
end
local function attrdata(object,id,attribute)
    local d=GetPresetData(object,false,true)
    local f=type(d)=='table' and type(d.by_fixtures)=='table' and
        (d.by_fixtures[id] or d.by_fixtures[tostring(id)])
    return type(f)=='table' and (f[attribute] or f[attribute:lower()]) or nil,d
end
local function rawvalue(a)
    return type(a)=='table' and a[1] and tonumber(a[1].absolute_value) or nil
end
local function preset_index(c,object)
    if not object then return nil end
    local a=attrdata(object,c.fade,'VideoCrossfade');local raw=rawvalue(a)
    if not integer(raw,0,RAWMAX) then return nil end
    local i=math.floor(raw*1023/RAWMAX+0.5)
    if raw24(i)~=raw then return nil end
    -- Slots define immutable logical positions, including deliberately empty gaps.
    local slot=tonumber(prop(object,'no'))
    if slot~=c.base+i then return nil end
    local label=tostring(prop(object,'name') or '')
    if label~=string.format('OBS %04d',i) and label~=string.format('OBS ML %04d',i) then return nil end
    return i,raw
end
local function parse_scene(note)
    note=tostring(note or '')
    local token=note:match('[Oo][Bb][Ss]%s*:%s*(%d+)')
    if not token then return nil,note:lower():find('obs%s*:') and 'Ungültige OBS-Note' or nil end
    local n=integer(token,1,65535)
    if not n then return nil,'OBS-Szenennummer muss 1 bis 65535 sein' end
    if note:match('[Oo][Bb][Ss]%s*:%s*%d+%.%d') then return nil,'OBS-Szenennummer muss ganzzahlig sein' end
    return n
end
local function cue_key(c,number)
    return string.format('DP%d:S%d:C%d',c.dp,c.main,number)
end
local function cue_signature(number,scene,raw)
    return string.format('%d,%d,%d',number,scene,raw)
end
local function revision_label(signature,token)
    local number,scene,raw=signature:match('^(%d+),(%d+),(%d+)$')
    assert(number and scene and raw,'Ungültige interne Cue-Zuordnung')
    -- MA removes commas in Cue names. Use only ASCII letters, digits and spaces.
    return string.format('ML35 C%d S%d R%d T%d',tonumber(number),tonumber(scene),tonumber(raw),token)
end
local function decode_revision_label(label)
    local number,scene,raw,token=tostring(label or ''):match('^ML35 C(%d+) S(%d+) R(%d+) T(%d+)$')
    if not number then
        -- Read old unsanitized v0.31-v0.34 labels, including .0 numeric spellings.
        number,scene,raw,token=tostring(label or ''):match('^ML31 ([%d%.]+),([%d%.]+),([%d%.]+),([%d%.]+)$')
    end
    number=integer(number,1,999999999);scene=integer(scene,1,65535)
    raw=integer(raw,0,RAWMAX);token=integer(token,1,65535)
    if not number or not scene or not raw or not token then return nil end
    return cue_signature(number,scene,raw),token
end
local function read_cue(c,cue)
    local scene,why=parse_scene(prop(cue,'Note'))
    if not scene then return nil,why end
    local number=integer(prop(cue,'no'),1,999999999)
    if not number then return nil,'Cue-Nummer nicht lesbar' end
    local part=cue[1];if not part then return nil,'Cue Part 0 fehlt' end
    local a=attrdata(part,c.fade,'VideoCrossfade')
    local ref=type(a)=='table' and a.abs_preset
    if not ref then return nil,'Fade-Preset muss ausdrücklich in Part 0 gespeichert sein; Tracking wird nicht aufgelöst' end
    local slot=tonumber(prop(ref,'no'))
    if not slot or slot<c.base or slot>c.base+1023 then return nil,'Fade-Preset liegt außerhalb des eingerichteten Bereichs' end
    local expected=preset(c,slot)
    if not expected or address(ref)~=address(expected) then return nil,'Fade-Preset liegt nicht im eingerichteten Pool' end
    local index,raw=preset_index(c,ref)
    if not index then return nil,'Fade-Preset hat keinen passenden Namen oder 24-Bit-Wert' end
    if rawvalue(a)~=raw then return nil,'Gespeicherter Cue-Wert stimmt nicht mit dem Fade-Preset überein' end
    local key=cue_key(c,number)
    return {key=key,number=number/1000,scene=scene,index=index,raw=raw,
        signature=cue_signature(number,scene,raw)}
end
local function inventory(c)
    local main=sequence(c,c.main);assert(main,'Lesesequenz fehlt')
    local items,problems={},{}
    for _,cue in ipairs(main:Children()) do
        if tonumber(prop(cue,'no')) and tonumber(prop(cue,'no'))>0 then
            local ok,item,why=pcall(read_cue,c,cue)
            if ok and item then items[#items+1]=item
            elseif not ok or why then
                problems[#problems+1]='Cue '..tostring((tonumber(prop(cue,'no')) or 0)/1000)..': '..clean_error(ok and why or item)
            end
        end
    end
    table.sort(items,function(a,b) return a.number<b.number end)
    return items,problems
end
-- No global programmer clearing. Only the dedicated target fixture is touched.
-- Selection/grid are retained using a fresh temporary Group, removed afterwards.
local function isolated(c,id,fn)
    current_pool(c);verify_patch(c)
    local handle=one('Fixture '..id);assert(handle,'Fixture fehlt')
    local channels=GetUIChannels(handle,false);assert(type(channels)=='table','Programmer-Prüfung nicht möglich')
    for _,index in pairs(channels) do
        local p=GetProgPhaser(index,false)
        assert(p==nil or (type(p)=='table' and next(p)==nil),
            'Fixture '..id..' enthält Programmer-Daten; Bearbeitung wartet bis diese freigegeben sind')
    end
    local group
    if SelectionCount()>0 then
        for n=9999,9000,-1 do if not one('DataPool '..c.dp..' Group '..n) then group=n;break end end
        assert(group,'Kein temporärer Gruppenplatz 9000..9999 frei')
        cmd('Store DataPool '..c.dp..' Group '..group..' /NoConfirmation')
        assert(one('DataPool '..c.dp..' Group '..group),'Auswahl konnte nicht gesichert werden')
    end
    local ok,why=xpcall(function()
        cmd('ClearSelection');cmd('Fixture '..id)
        assert(SelectionCount()==1,'Steuer-Fixture nicht einzeln selektierbar (World/Filter prüfen)')
        fn()
    end,debug.traceback)
    local clean,cleanup=xpcall(function()
        cmd('Off Fixture '..id)
        cmd('ClearSelection')
        if group then
            cmd('Group '..group)
            cmd('Delete DataPool '..c.dp..' Group '..group..' /NoConfirmation')
        end
    end,debug.traceback)
    assert(clean,'Auswahl-Wiederherstellung fehlgeschlagen; Gruppe '..tostring(group)..' behalten: '..tostring(cleanup))
    assert(ok,why)
end
local function plan_presets(c,mode)
    validate(c)
    local highest=-1
    for i=0,1023 do
        local p=preset(c,c.base+i)
        if p and preset_index(c,p)~=nil then highest=i end
    end
    local gaps=mode=='gaps'
    -- Filling holes is independent of the append raster. Never silently grow
    -- the range when there are no holes (or no managed presets to begin with).
    local step=gaps and 1 or c.step
    local start=gaps and 0 or math.floor(highest/c.step+1)*c.step
    local finish=gaps and highest or 1023
    local result={items={},conflicts={},existing=0,start=start,mode=mode,
        step=step,highest=highest,finish=finish}
    for i=start,finish,step do
        local slot=c.base+i;local p=preset(c,slot)
        if p then
            if preset_index(c,p)~=nil then result.existing=result.existing+1
            else result.conflicts[#result.conflicts+1]=slot end
        elseif #result.items<c.count then
            result.items[#result.items+1]={slot=slot,index=i,raw=raw24(i)}
        end
        if #result.items==c.count then break end
    end
    if #result.items==0 then
        if gaps and highest<0 then
            result.reason='Noch keine passenden MediaLink-Presets im Zielbereich. Zuerst mit Hinten ergänzen einen Bereich anlegen.'
        elseif gaps then
            result.reason='Keine freien Zwischenplätze im bereits angelegten Bereich. Zum Vergrößern Hinten ergänzen wählen.'
        else result.reason='Keine freien Rasterpositionen mehr im Bereich der 1024 logischen Positionen.' end
    end
    return result
end
local function generate(c,plan)
    assert(#plan.items>0,'Keine freien Positionen im 1024er-Bereich')
    local count=0
    for _,item in ipairs(plan.items) do
        assert(not preset(c,item.slot),'Position inzwischen belegt: '..item.slot)
        isolated(c,c.fade,function()
            -- Non-zero priming preserves the tested zero-position workaround.
            if item.raw==0 then cmd('Attribute "VideoCrossfade" At Absolute Decimal24 16400') end
            cmd('Attribute "VideoCrossfade" At Absolute Decimal24 '..item.raw)
            assert(not preset(c,item.slot),'Position inzwischen belegt')
            cmd('Store '..presetaddr(c,item.slot)..' /Selective /ActiveForSelected /KeepActivation /NoConfirmation /InputFilter "No"')
            cmd('Label '..presetaddr(c,item.slot)..' "'..string.format('OBS ML %04d',item.index)..'"')
            assert(preset_index(c,preset(c,item.slot))==item.index,'Preset-Verifikation fehlgeschlagen: '..item.slot)
        end)
        count=count+1
        if count%8==0 then coroutine.yield(0.03) end
    end
    return count
end
local function sequence_allowed(c)
    local control=sequence(c,c.control)
    if control then assert(control_owned(c,control),
        'Steuersequenz ist belegt. Eine freie Nummer wählen oder die Sequenz unter Steuersequenz prüfen und übernehmen.') end
    return control
end
local function metadata_scope(c,cue)
    if not cue or not cue[1] then return false,'Cue Part 0 fehlt' end
    local parts=cue:Children()
    if #parts~=1 then return false,'Weitere Cue-Parts vorhanden' end
    for _,object in ipairs({cue,cue[1]}) do
        for _,name in ipairs({'Command','CueCommand'}) do
            local value=prop(object,name)
            if value and tostring(value)~='' then return false,'Cue-Commands vorhanden' end
        end
    end
    local d=GetPresetData(cue[1],false,true)
    if type(d)~='table' or type(d.by_fixtures)~='table' then return false,'Cue-Daten nicht prüfbar' end
    for id,attrs in pairs(d.by_fixtures) do
        if tonumber(id)~=c.controller then return false,'Weitere Fixtures vorhanden' end
        for name in pairs(attrs) do
            local n=tostring(name):lower()
            if n~='videoscenenumber' and n~='dimmer' and n~='videotransitiontoken' then return false,'Weitere Attribute vorhanden' end
        end
    end
    return true
end
local function control_integrity(c)
    local target=sequence_allowed(c)
    if target then for _,cue in ipairs(target:Children()) do
        if (tonumber(prop(cue,'no')) or 0)>0 then
            local ok,why=metadata_scope(c,cue)
            assert(ok,'Steuersequenz enthält fremde Daten: '..tostring(why))
        end
    end end
end
local function label_control(c,explicit)
    local target=sequence(c,c.control);assert(target,'Steuersequenz fehlt')
    assert(explicit or control_owned(c,target),'Steuersequenz gehört zu einer anderen Einrichtung')
    for _,cue in ipairs(target:Children()) do
        if (tonumber(prop(cue,'no')) or 0)>0 then
            local ok,why=metadata_scope(c,cue);assert(ok,'Steuersequenz enthält fremde Daten: '..tostring(why))
        end
    end
    local note=tostring(prop(target,'Note') or '')
    if explicit then
        local kept={}
        for line in (note..'\n'):gmatch('(.-)\n') do
            if not line:gsub('\r$',''):match('^%[MAOBS_ML31 DP%d+ S%d+ F%d+ C%d+%]$') then kept[#kept+1]=line end
        end
        note=table.concat(kept,'\n')
    end
    local marked=false
    for line in (note..'\n'):gmatch('(.-)\n') do
        if line:gsub('\r$','')==control_tag(c) then marked=true end
    end
    if not marked then
        local updated=note..(note~='' and '\n' or '')..control_tag(c)
        target:Set('Note',updated) -- documented void return; verify by reading back
        assert(prop(target,'Note')==updated,'Kennung der Steuersequenz konnte nicht gespeichert werden')
    end
    assert(control_owned(c,target),'Die Notiz der Steuersequenz enthält eine widersprüchliche MediaLink-Kennung')
    if prop(target,'name')~=CONTROL_LABEL then
        target:Set('name',CONTROL_LABEL)
        assert(prop(target,'name')==CONTROL_LABEL,'Steuersequenz konnte nicht umbenannt werden')
    end
    return target
end
local function dmx16(value)
    -- GetPresetData absolute_value is the internal 24-bit raw value, even
    -- when the user entered Decimal16. The two transmitted bytes are its
    -- high 16 bits. Treating Decimal16 6 as raw24 6 gave a false mismatch.
    value=integer(value,0,RAWMAX)
    return value and math.floor(value/256) or nil
end
local function metadata_ok(c,cue,entry,token)
    local scoped,why=metadata_scope(c,cue)
    if not scoped then return false,'Steuer-Cue nicht prüfbar: '..tostring(why) end
    local a=attrdata(cue[1],c.controller,'VideoSceneNumber')
    local b=attrdata(cue[1],c.controller,'Dimmer')
    local d=attrdata(cue[1],c.controller,'VideoTransitionToken')
    local scene_raw,endpoint_raw,token_raw=rawvalue(a),rawvalue(b),rawvalue(d)
    local scene,actual_token=dmx16(scene_raw),dmx16(token_raw)
    if scene==entry.scene and endpoint_raw==entry.raw and actual_token==token then return true end
    return false,string.format('Steuer-Cue-Prüfung fehlgeschlagen: Szene %s/%d, Endwert %s/%d, Token %s/%d (Ist/Soll; Rohwerte %s/%s/%s).',
        tostring(scene),entry.scene,tostring(endpoint_raw),entry.raw,tostring(actual_token),token,
        tostring(scene_raw),tostring(endpoint_raw),tostring(token_raw))
end
local function matching_revision(c,cue,entry)
    if not cue then return nil end
    local label=tostring(prop(cue,'name') or '')
    local signature,token=decode_revision_label(label)
    if not signature then
        -- v0.34 may have stored valid values then failed because MA stripped
        -- the commas. Never guess boundaries in concatenated numbers. Decode
        -- the actual token, validate ALL data, and reconstruct the exact name
        -- for this known main Cue instead.
        if not label:match('^ML31 %d+$') then return nil end
        local token_data=cue[1] and attrdata(cue[1],c.controller,'VideoTransitionToken')
        token=dmx16(rawvalue(token_data))
        if not integer(token,1,65535) then return nil end
        local legacy='ML31 '..entry.signature:gsub(',','')..string.format('%d',token)
        if label~=legacy then return nil end
        signature=entry.signature
    end
    local no=integer(prop(cue,'no'),1,999999999)
    if signature~=entry.signature or not no or no%1000~=0 or
        not metadata_ok(c,cue,entry,token) then return nil end
    return {control=math.floor(no/1000),token=token,signature=signature,done=true}
end
local function control_cue(c,n) return one(seqaddr(c)..' Cue '..n) end
local function next_control(c)
    control_integrity(c)
    local control=sequence_allowed(c);local highest=1000
    if control then for _,cue in ipairs(control:Children()) do
        local n=tonumber(prop(cue,'no'));if n and n>0 then highest=math.max(highest,math.ceil(n/1000)) end
    end end
    assert(highest<999999,'Steuersequenz ist voll')
    return highest+1
end
local function revision(c,entry)
    -- At most one Store attempt per identity/signature in this loaded runtime.
    -- A failed Store/readback must never turn into a new Cue every timer tick.
    S.write_attempts=S.write_attempts or {}
    local key=seqaddr(c)..'|'..ownership(c)..'|'..entry.signature
    local prior=S.write_attempts[key]
    if prior then
        local recovered=matching_revision(c,control_cue(c,prior.control),entry)
        if recovered then S.write_attempts[key]=recovered;return recovered end
        error('Schreibsperre: Cue '..prior.control..' wurde für diese Zuordnung bereits angelegt oder versucht. '..
            'Keine weitere Cue wird erstellt. Diagnose prüfen oder eine freie Steuersequenz wählen.',0)
    end
    local n=next_control(c)
    local result
    isolated(c,c.controller,function()
        local token=(tonumber(readvar('TOKEN')) or 0)%65535+1
        writevar('TOKEN',token)
        cmd('Attribute "VideoSceneNumber" At Absolute Decimal16 '..entry.scene)
        cmd('Attribute "Dimmer" At Absolute Decimal24 '..entry.raw)
        cmd('Attribute "VideoTransitionToken" At Absolute Decimal16 '..token)
        assert(not control_cue(c,n),'Steuer-Cue inzwischen belegt')
        local new_sequence=sequence_allowed(c)==nil
        result={control=n,token=token,signature=entry.signature,done=false}
        S.write_attempts[key]=result -- commit the attempt BEFORE the mutating command
        if S.running then S.writes_this_tick=(S.writes_this_tick or 0)+1 end
        cmd('Store '..seqaddr(c)..' Cue '..n..' /ActiveForSelected /NoConfirmation')
        local verified,why=metadata_ok(c,control_cue(c,n),entry,token)
        assert(verified,why)
        local label=revision_label(entry.signature,token)
        cmd('Label '..seqaddr(c)..' Cue '..n..' "'..label..'"')
        label_control(c,new_sequence)
        assert(prop(control_cue(c,n),'name')==label and control_owned(c,sequence(c,c.control)),
            'Steuer-Cue-Beschriftung konnte nicht bestätigt werden')
    end)
    result.done=true -- only after Store, verification, labels AND cleanup succeeded
    return result
end
local function find_revision(c,entry)
    local control=sequence_allowed(c);if not control then return nil end
    local found
    for _,cue in ipairs(control:Children()) do
        local candidate=matching_revision(c,cue,entry)
        if candidate and (not found or found.control<candidate.control) then found=candidate end
    end
    return found
end
S=_G.MA_OBS_MEDIALINK31
if not S then
    S={running=false,busy=false,epoch=0,events=0,map={},issues={},status='Aus',version=VERSION}
    _G.MA_OBS_MEDIALINK31=S
end
if S.version~=VERSION then
    S.running=false;S.epoch=(S.epoch or 0)+1;S.boot_epoch=(S.boot_epoch or 0)+1
    if S.vars==gv() and readvar('SENDER')==S.owner then
        clearvar('SENDER');clearvar('HEARTBEAT')
    end
    S.version=VERSION;S.ui=false;S.status='Aktualisiert · Verknüpfung neu starten'
end
local function stop(reason,remember)
    S.running=false;S.epoch=S.epoch+1;S.boot_epoch=(S.boot_epoch or 0)+1
    S.status=reason or 'Aus'
    if remember~=false then writevar('LAST',0) end
    if S.vars==gv() and readvar('SENDER')==S.owner then clearvar('SENDER');clearvar('HEARTBEAT') end
    log(S.status)
end
local function owns()
    return S.running and S.vars==gv() and readvar('SENDER')==S.owner
end
local function status_error(text)
    local short=clean_error(text)
    if S.status~=short then report_error(text) end
    S.status=short
end
local function sync_one(c,items)
    for _,entry in ipairs(items) do
        local mapped=S.map[entry.key]
        if not mapped or mapped.signature~=entry.signature then
            S.map[entry.key]=find_revision(c,entry) or revision(c,entry)
            -- At most one write per timer pass; caller schedules the next pass.
            break
        end
    end
    -- Keep entries while work remains; dispatch independently revalidates current cue.
    for key in pairs(S.map) do
        local exists=false;for _,entry in ipairs(items) do if entry.key==key then exists=true;break end end
        if not exists then S.map[key]=nil end
    end
end
local function dispatch(c)
    local main=sequence(c,c.main);assert(main,'Lesesequenz wurde entfernt')
    local cue=main:CurrentChild()
    if not cue then S.previous=nil;return end
    local number=integer(prop(cue,'no'),1,999999999)
    if not number then S.previous=nil;return end
    local key=cue_key(c,number)
    if S.previous==key then return end
    local entry,why=read_cue(c,cue)
    if not entry then
        if why then status_error('Aktuelle Cue: '..why) else S.previous=key end
        return
    end
    local mapped=S.map[entry.key]
    if not mapped or mapped.signature~=entry.signature then
        mapped=find_revision(c,entry) or revision(c,entry);S.map[entry.key]=mapped
    end
    assert(owns(),'Sendersperre verloren')
    local verified,why=metadata_ok(c,control_cue(c,mapped.control),entry,mapped.token)
    assert(verified,why)
    cmd('Goto '..seqaddr(c)..' Cue '..mapped.control..' Fade 0')
    S.previous=key;S.events=S.events+1
    S.status='Aktiv · Cue '..entry.number..' → OBS:'..entry.scene
    log(S.status)
end
local tick
local function schedule(epoch)
    local fired=false
    Timer(function()
        -- A Timer callback can only advance this chain once, even if called
        -- repeatedly or after a newer epoch has already started.
        if fired then return end
        fired=true
        if S.running and S.epoch==epoch then
            if S.in_tick then schedule(epoch) else tick(epoch) end
        end
    end,0.10,1)
end
tick=function(epoch)
    if not S.running or S.epoch~=epoch or S.in_tick then return end
    if S.vars~=gv() then stop('Show-Kontext gewechselt',false);return end
    if not owns() then stop('Andere Station hat die Sendersperre übernommen',false);return end
    S.in_tick=true;S.writes_this_tick=0
    local ok,why=xpcall(function()
        local now=Time()
        assert(type(now)=='number','MA-Laufzeituhr nicht lesbar')
        -- Do not depend on whether a MA build rounds the fractional Timer wait.
        if S.last_poll and now-S.last_poll<0.095 then return end
        S.last_poll=now
        if S.busy then return end
        assert(encode(S.config)==readvar('CONFIG'),'Konfiguration auf anderer Station geändert; neu starten')
        current_pool(S.config);sequence_allowed(S.config)
        if tonumber(readvar('HEARTBEAT'))~=os.time() then writevar('HEARTBEAT',os.time()) end
        dispatch(S.config)
        S.ticks=(S.ticks or 0)+1
        if S.ticks%10==0 then
            verify_patch(S.config);control_integrity(S.config)
            S.items,S.issues=inventory(S.config)
        end
        if S.items and S.writes_this_tick==0 then sync_one(S.config,S.items) end
    end,debug.traceback)
    S.in_tick=false
    if not ok then
        if S.writes_this_tick>0 or tostring(why):find('Schreibsperre:',1,true) then
            local stopped,cleanup=pcall(stop,'Angehalten · Steuer-Cue-Schreibfehler',false)
            if not stopped then why=tostring(why)..'\nFreigabe: '..tostring(cleanup) end
            report_error(why)
            return -- no reschedule, no retry, no further Store commands
        end
        status_error(tostring(why))
    end
    if S.running and S.epoch==epoch then schedule(epoch) end
end
local function start(c)
    if S.running then return end
    assert(c.configured==1,'Zuerst Einrichtung speichern')
    preflight(c);control_integrity(c)
    local owner=tostring(readvar('SENDER') or '')
    local heartbeat=tonumber(readvar('HEARTBEAT')) or 0
    assert(owner=='' or os.time()-heartbeat>8,
        'Bereits ein Sender reserviert: '..owner..'. Unter Diagnose gezielt freigeben.')
    local previous_last=readvar('LAST')
    S.owner=tostring(safe(SerialNumber) or 'Station')..':'..tostring({}):gsub('table: ','')
    S.vars=gv();S.config=copy(c);S.epoch=S.epoch+1
    S.previous=nil;S.map={};S.items=nil;S.issues={};S.ticks=9;S.events=0
    S.in_tick=false;S.last_poll=nil;S.writes_this_tick=0
    local ok,why=xpcall(function()
        writevar('SENDER',S.owner)
        writevar('HEARTBEAT',os.time())
        writevar('LAST',1)
        if sequence(c,c.control) then label_control(c) end
        S.running=true
        schedule(S.epoch)
    end,debug.traceback)
    if not ok then
        S.running=false;S.epoch=S.epoch+1
        local cleaned,cleanup=pcall(function()
            if S.vars==gv() and readvar('SENDER')==S.owner then
                clearvar('SENDER');clearvar('HEARTBEAT')
            end
            if readvar('LAST')~=previous_last then
                if previous_last==nil then clearvar('LAST') else writevar('LAST',previous_last) end
            end
        end)
        S.status='Start fehlgeschlagen'
        if not cleaned then why=tostring(why)..'\nAufräumen fehlgeschlagen: '..tostring(cleanup) end
        error(why,0)
    end
    S.status='Aktiv · warte auf gültige OBS-Cue'

end
local function startup(c)
    c=c or validate(decode(readvar('CONFIG')))
    if S.running then return true end
    local wanted=c.startup==1 or (c.startup==3 and tonumber(readvar('LAST'))==1)
    if wanted then start(c) end
    return wanted
end
local function boot(attempt,epoch)
    if epoch~=(S.boot_epoch or 0) or S.running then return end
    local ok,why=pcall(function()
        assert(readvar('CONFIG') and decode(readvar('CONFIG')).configured==1,
            'Gespeicherte Einrichtung noch nicht verfügbar')
        startup()
    end)
    if ok then return end
    if attempt<20 then
        Timer(function() boot(attempt+1,epoch) end,1,1)
    else status_error('Autostart wartet: '..tostring(why)) end
end
local ui_serial=0
local function text_length(text)
    local count=type(utf8)=='table' and utf8.len(text)
    return count or #text
end
local function wrapped(text,columns)
    local lines={}
    for paragraph in (tostring(text)..'\n'):gmatch('(.-)\n') do
        local line=''
        for raw_word in paragraph:gmatch('%S+') do
            local word=raw_word
            if text_length(word)>columns then
                if line~='' then lines[#lines+1]=line;line='' end
                while text_length(word)>columns do
                    local edge=type(utf8)=='table' and utf8.offset(word,columns+1) or columns+1
                    lines[#lines+1]=word:sub(1,edge-1);word=word:sub(edge)
                end
                line=word
            elseif #line>0 and text_length(line)+text_length(word)+1>columns then lines[#lines+1]=line;line=word
            else line=line..(line~='' and ' ' or '')..word end
        end
        lines[#lines+1]=line
    end
    return table.concat(lines,'\n'),#lines
end
local function native_dialog(t)
    if not t.ui_rows or type(signals)~='table' or not component_handle or
        type(GetFocusDisplay)~='function' or type(IsObjectValid)~='function' or type(TextInput)~='function' then return nil end
    local display=ui_display
    if not display or not safe(function() return IsObjectValid(display) and display.ScreenOverlay end) then display=GetFocusDisplay() end
    local overlay=display and prop(display,'ScreenOverlay');if not overlay then return nil end
    local available_w=tonumber(prop(display,'W')) or 1280
    local available_h=tonumber(prop(display,'H')) or 900
    local width=math.min(t.ui_width or 1080,available_w-40)
    if width<680 then return nil end
    local inner=width-40
    local function fitted(text,w,strong)
        local value,lines=wrapped(text,math.max(10,math.floor((w-20)/(strong and 11 or 9))))
        return value,lines*(strong and 28 or 24)+16
    end
    local segments={};local y=10
    for paragraph in (t.message..'\n\n'):gmatch('(.-)\n\n') do
        if paragraph~='' then
            local strong=t.ui_status and #segments==0
            local text,h=fitted(paragraph,inner,strong)
            if strong then h=math.max(h,72) end
            segments[#segments+1]={text=text,y=y,h=h,strong=strong}
            y=y+h+10
        end
    end
    if t.ui_hint then
        local text,h=fitted(t.ui_hint,inner,true)
        segments[#segments+1]={text=text,y=y+4,h=h,strong=true}
        y=y+h+18
    end
    local inputs,selectors={},{}
    for _,v in ipairs(t.inputs or {}) do inputs[v.name]=v end
    for _,v in ipairs(t.selectors or {}) do selectors[v.name]=v end
    local label_w=math.floor(inner*.36)
    local field_x=20+math.floor(inner*.37);local field_w=inner-math.floor(inner*.37)
    local form={}
    for _,row in ipairs(t.ui_rows) do
        -- gsub returns text AND replacement count: assign first, never pass
        -- both results into create's text/font argument pair.
        local name=row.name:gsub('^%d+%s+','')
        local label,h=fitted(name,label_w,false)
        local layout={row=row,label=label,y=y,h=math.max(48,h),choices={}}
        local selector=selectors[row.name]
        assert(inputs[row.name] or selector,'Dialogfeld fehlt: '..tostring(row.name))
        if selector then
            for name,value in pairs(selector.values) do layout.choices[#layout.choices+1]={name=name,value=value} end
            table.sort(layout.choices,function(a,b) return a.value<b.value end)
            for _,choice in ipairs(layout.choices) do
                local text,needed=fitted(choice.name,math.floor(field_w/#layout.choices)-4,false)
                choice.text=text;layout.h=math.max(layout.h,needed)
            end
        end
        form[#form+1]=layout;y=y+layout.h+8
    end
    local per_row=(width<980 and #t.commands>4) and 4 or #t.commands
    local command_layout={}
    for first=1,#t.commands,per_row do
        local count=math.min(per_row,#t.commands-first+1)
        local w=math.floor(inner/count);local row_h=52
        for offset=0,count-1 do
            local command=t.commands[first+offset]
            local text,h=fitted(command.name,w-4,false);row_h=math.max(row_h,h)
            command_layout[#command_layout+1]={command=command,text=text,x=20+offset*w,y=y,w=w-4}
        end
        for index=#command_layout-count+1,#command_layout do command_layout[index].h=row_h end
        y=y+row_h+8
    end
    local height=48+y+16
    if height>available_h-35 then return nil end
    local result={success=false,result=0,inputs={},selectors={}}
    for _,input in ipairs(t.inputs or {}) do result.inputs[input.name]=input.value end
    for _,selector in ipairs(t.selectors or {}) do result.selectors[selector.name]=selector.selectedValue end
    local base;local callbacks={};local done=false
    local context=gv()
    ui_serial=ui_serial+1
    local prefix='ML39_'..ui_serial..'_'
    local ok,problem=xpcall(function()
        base=overlay:Append('BaseInput')
        base.W=width;base.H=height;base.Columns=1;base.Rows=2
        base[1][1].SizePolicy='Fixed';base[1][1].Size=48
        base[1][2].SizePolicy='Stretch'
        base.AutoClose='No';base.CloseOnEscape='Yes'
        local title=base:Append('TitleBar');title.Columns=2;title.Rows=1;title.Anchors='0,0'
        title[2][2].SizePolicy='Fixed';title[2][2].Size=48
        local heading=title:Append('TitleButton');heading.Anchors='0,0';heading.Text=t.title
        heading.TextalignmentH='Left';heading.Texture='corner1'
        local close=title:Append('TitleButton');close.Anchors='1,0';close.Text='×';close.Texture='corner2'
        local frame=base:Append('DialogFrame');frame.Anchors={left=0,right=0,top=1,bottom=1}
        frame.Columns=1;frame.Rows=1
        frame[1][1].SizePolicy='Stretch'
        local function create(kind,x,top,w,h,text,font)
            local item=frame:Append(kind);item.X=x;item.Y=top;item.W=w;item.H=h
            assert(font==nil or type(font)=='string','Ungültige Schriftart im Dialog')
            item.Text=text;item.Font=font or 'Regular18';item.TextalignmentH='Left';item.TextalignmentV='Center'
            if kind=='UIObject' then item.BackColor='Global.Transparent'
            else item.TextalignmentV='Center';item.HasHover='Yes';item.WantsNumericRedirect='Yes' end
            return item
        end
        local function connect(button,fn)
            local key=prefix..(#callbacks+1);callbacks[#callbacks+1]=key
            signals[key]=function(...)
                if done or not IsObjectValid(base) or gv()~=context then return end
                local called,why=pcall(fn,...)
                if not called then report_error(why) end
            end
            button.PluginComponent=component_handle;button.Clicked=key
        end
        connect(close,function() done=true end)
        local status_label
        for index,segment in ipairs(segments) do
            local label=create('UIObject',20,segment.y,inner,segment.h,segment.text,segment.strong and 'Medium20' or 'Regular18')
            if index==1 and t.ui_status then status_label=label end
        end
        local widgets={};local redraw
        for _,layout in ipairs(form) do
            local row=layout.row;local top=layout.y
            local input=inputs[row.name];local selector=selectors[row.name]
            local label=create('UIObject',20,top,label_w,layout.h,layout.label,'Regular18')
            local x=field_x
            local entry={row=row,label=label,buttons={}};widgets[#widgets+1]=entry
            if input then
                local button=create('Button',x,top,field_w,layout.h,tostring(input.value))
                entry.buttons[1]=button
                connect(button,function()
                    if row.enabled_when and not row.enabled_when(result) then return end
                    local value=TextInput(row.name:gsub('^%d+%s+',''),result.inputs[row.name])
                    if value~=nil then result.inputs[row.name]=tostring(value);redraw() end
                end)
            else
                for j,choice in ipairs(layout.choices) do
                    local w=math.floor(field_w/#layout.choices)
                    local button=create('Button',x+(j-1)*w,top,w-4,layout.h,choice.text)
                    button.TextalignmentH='Center'
                    entry.buttons[#entry.buttons+1]={handle=button,value=choice.value}
                    connect(button,function() result.selectors[row.name]=choice.value;redraw() end)
                end
            end
        end
        redraw=function()
            for _,entry in ipairs(widgets) do
                local row=entry.row;local enabled=not row.enabled_when or row.enabled_when(result)
                entry.label.Enabled=enabled and 'Yes' or 'No'
                if inputs[row.name] then
                    entry.buttons[1].Enabled=enabled and 'Yes' or 'No'
                    entry.buttons[1].Text=result.inputs[row.name]
                else for _,button in ipairs(entry.buttons) do
                    button.handle.State=result.selectors[row.name]==button.value and 1 or 0
                    button.handle.Enabled=enabled and 'Yes' or 'No'
                end end
            end
        end
        redraw()
        local first_command
        for index,layout in ipairs(command_layout) do
            local command=layout.command
            local button=create('Button',layout.x,layout.y,layout.w,layout.h,layout.text)
            button.TextalignmentH='Center'
            if index==1 then first_command=button end
            connect(button,function() result.success=true;result.result=command.value;done=true end)
        end
        base:WaitInit()
        if type(FindBestFocus)=='function' then pcall(FindBestFocus,base) end
        while not done and IsObjectValid(base) and gv()==context do
            if status_label then status_label.Text=fitted(t.ui_status(),inner,true) end
            if t.ui_action_name and first_command then first_command.Text=t.ui_action_name() end
            coroutine.yield(0.1)
        end
    end,debug.traceback)
    for _,key in ipairs(callbacks) do signals[key]=nil end
    if base and IsObjectValid(base) then pcall(function() base:Close() end) end
    if not ok then log('Dialog: '..tostring(problem));return nil end
    return result
end
local function box(title,message,extra)
    local context=gv()
    local t={title='MA OBS MediaLink · v'..VERSION..' BETA – '..title,message=message,
        commands={{value=0,name='Schließen'}},autoCloseOnInput=false}
    for k,v in pairs(extra or {}) do t[k]=v end
    local custom=native_dialog(t)
    if custom then return custom end
    local fallback={title=t.title,message=t.message,commands=t.commands,inputs=t.inputs,selectors=t.selectors,autoCloseOnInput=false}
    if t.ui_hint then fallback.message=fallback.message..'\n\n'..t.ui_hint end
    if Enums and Enums.AlignmentH and Enums.AlignmentV then
        fallback.message_align_h=Enums.AlignmentH.Left;fallback.message_align_v=Enums.AlignmentV.Top
    end
    local result=MessageBox(fallback)
    if gv()~=context then return {success=false,result=0} end
    return result
end
local function require_stopped()
    assert(not S.running,'Die Bridge vor Änderungen stoppen.')
end
local function adoption(c)
    require_stopped();preflight(c)
    local target=sequence(c,c.control)
    if not target then box('Steuersequenz','Die Nummer ist frei. Beim ersten Abgleich wird die Steuersequenz automatisch angelegt.');return end
    if control_owned(c,target) then
        box('Steuersequenz','Sequenz '..c.control..' gehört zur aktuellen Einrichtung.\n\nSie wird beim nächsten Start als „'..CONTROL_LABEL..'“ angezeigt.');return
    end
    local count=0
    for _,cue in ipairs(target:Children()) do
        if (tonumber(prop(cue,'no')) or 0)>0 then
            local valid,why=metadata_scope(c,cue)
            assert(valid,'Steuersequenz wird nicht übernommen: '..tostring(why))
            count=count+1
        end
    end
    local answer=box('Steuersequenz','Sequenz '..c.control..' enthält '..count..
        ' geprüfte Cues.\n\nDie Sequenz wird für MediaLink übernommen und „'..CONTROL_LABEL..'“ genannt. '..
        'Vorhandene Cues bleiben erhalten.',
        {commands={{value=1,name='Übernehmen'},{value=0,name='Abbrechen'}}})
    if answer.success and answer.result==1 then
        local owner=tostring(readvar('SENDER') or '')
        assert(owner=='' or os.time()-(tonumber(readvar('HEARTBEAT')) or 0)>8,
            'Die Bridge auf der sendenden Station vor der Übernahme stoppen.')
        label_control(c,true)
    end
end
local function setup(c)
    require_stopped()
    local fields={
        {'01 Datenpool','dp'},{'02 Auszulesende Sequenz','main'},
        {'03 Fade-Fixture','fade'},{'04 Steuer-Fixture','controller'},{'05 Steuersequenz','control'}}
    local inputs={};for _,f in ipairs(fields) do inputs[#inputs+1]={name=f[1],value=tostring(c[f[2]]),whitefilter='0123456789',maxTextLength=6} end
    local answer=box('Einrichtung',
        'Die auszulesende Sequenz enthält die OBS-Cues und wird unabhängig von der ausgewählten Sequenz verwendet.\n\n'..
        'Fade-Fixture und Steuer-Fixture müssen passend gepatcht sein. Presets werden im Bereich Presets eingerichtet.',
        {ui_rows={{name=fields[1][1]},{name=fields[2][1]},{name=fields[3][1]},
            {name=fields[4][1]},{name=fields[5][1]},{name='06 Beim Öffnen des Plugins'}},
         inputs=inputs,selectors={{name='06 Beim Öffnen des Plugins',type=1,selectedValue=c.startup,
            values={['Bridge starten']=1,['Nur Bedienfenster öffnen']=2,['Letzten Zustand wiederherstellen']=3}}},
         commands={{value=1,name='Speichern'},{value=0,name='Abbrechen'}}})
    if not answer.success or answer.result~=1 then return c end
    local nextc=copy(c)
    for _,f in ipairs(fields) do nextc[f[2]]=tonumber(answer.inputs[f[1]]) end
    nextc.startup=answer.selectors['06 Beim Öffnen des Plugins'];nextc.configured=1
    validate(nextc);writevar('CONFIG',encode(nextc));S.config=copy(nextc)
    box('Einrichtung','Die Einstellungen wurden gespeichert.\n\n'..
        'Vor dem Start unter Prüfen / Diagnose die Fixtures und den Patch kontrollieren.\n\n'..
        'Eine bereits belegte Sequenz lässt sich unter Steuersequenz prüfen und übernehmen.')
    return nextc
end
local function presets_ui(c)
    require_stopped()
    local answer=box('Presets',
        'Die Anzahl bestimmt, wie viele neue Presets zusätzlich erstellt werden. Vorhandene Presets bleiben erhalten.\n\n'..
        'Hinten ergänzen: Neue Presets werden im gewählten Abstand angehängt.\n\n'..
        'Lücken ergänzen: Freie Positionen bis zum letzten vorhandenen MediaLink-Preset werden gefüllt. Der Bereich wird nicht erweitert.\n\n'..
        'Eine Änderung der Startposition verschiebt keine vorhandenen Presets.\n\n'..
        'Speichern sichert die Einstellungen. Vorschau zeigt die geplanten Presets.',
        {ui_rows={{name='04 All-Presetpool'},{name='01 Erste Presetposition'},{name='02 Anzahl zusätzlich'},
            {name='05 Vorgehen'},{name='03 Abstand beim Anhängen',enabled_when=function(r) return r.selectors['05 Vorgehen']~=2 end}},
         inputs={{name='01 Erste Presetposition',value=tostring(c.base),whitefilter='0123456789',maxTextLength=6},
            {name='02 Anzahl zusätzlich',value=tostring(c.count),whitefilter='0123456789',maxTextLength=4},
            {name='03 Abstand beim Anhängen',value=tostring(c.step),whitefilter='0123456789',maxTextLength=4}},
         selectors={{name='04 All-Presetpool',type=0,selectedValue=c.pool,
            values={['All 1']=21,['All 2']=22,['All 3']=23,['All 4']=24,['All 5']=25}},
            {name='05 Vorgehen',type=1,selectedValue=c.mode,values={['Hinten ergänzen']=1,['Lücken ergänzen']=2}}},
         commands={{value=1,name='Vorschau'},{value=2,name='Speichern'},{value=0,name='Abbrechen'}}})
    if not answer.success or answer.result==0 then return c end
    local nextc=copy(c)
    nextc.base=tonumber(answer.inputs['01 Erste Presetposition'])
    nextc.count=tonumber(answer.inputs['02 Anzahl zusätzlich']);nextc.step=tonumber(answer.inputs['03 Abstand beim Anhängen'])
    nextc.pool=tonumber(answer.selectors['04 All-Presetpool']);nextc.mode=tonumber(answer.selectors['05 Vorgehen'])
    validate(nextc)
    if answer.result==2 then
        writevar('CONFIG',encode(nextc));S.config=copy(nextc)
        box('Presets','Einstellungen gespeichert. Es wurden keine Presets erzeugt oder verändert.')
        return nextc
    end
    preflight(nextc)
    local mode=nextc.mode==2 and 'gaps' or 'append'
    local plan=plan_presets(nextc,mode)
    local lines={'Ziel: All '..(nextc.pool-20)..' · Startposition '..nextc.base,
        'Neu möglich: '..#plan.items..' / angefordert: '..nextc.count,
        'Belegte fremde Plätze übersprungen: '..#plan.conflicts}
    if mode=='gaps' and plan.highest>=0 then
        lines[#lines+1]='Lückenbereich: '..nextc.pool..'.'..nextc.base..' bis '..nextc.pool..'.'..(nextc.base+plan.highest)
    elseif mode~='gaps' then lines[#lines+1]='Abstand beim Anhängen: '..nextc.step end
    if plan.reason then lines[#lines+1]=plan.reason end
    for i,item in ipairs(plan.items) do
        log('VORSCHAU '..presetaddr(nextc,item.slot)..' = '..item.raw)
        if i<=12 then lines[#lines+1]=nextc.pool..'.'..item.slot..' → OBS ML '..string.format('%04d',item.index) end
    end
    for i,slot in ipairs(plan.conflicts) do
        log('BELEGT '..presetaddr(nextc,slot)..' – bleibt unverändert')
        if i<=8 then lines[#lines+1]='Belegt: '..nextc.pool..'.'..slot end
    end
    if #plan.items>12 or #plan.conflicts>8 then lines[#lines+1]='… vollständige Liste im System Monitor' end
    if #plan.items==0 then box('Preset-Vorschau',table.concat(lines,'\n'));return c end
    local yes=box('Preset-Vorschau',table.concat(lines,'\n'),
        {commands={{value=1,name='Jetzt erzeugen'},{value=0,name='Abbrechen'}}})
    if yes.success and yes.result==1 then
        S.busy=true
        local ok,n=xpcall(function() return generate(nextc,plan) end,debug.traceback)
        S.busy=false
        if not ok then error(n,0) end
        writevar('CONFIG',encode(nextc));S.config=copy(nextc)
        box('Presets',n..' neue Presets erzeugt. Vorhandene Presets bleiben erhalten.')
        return nextc
    end
    return c
end
local function ma_build_version()
    local build=safe(BuildDetails)
    return type(build)=='table' and tostring(build.BigVersion or 'unbekannt') or 'unbekannt'
end
local function api_diagnostics(c)
    log('grandMA3-Version: '..ma_build_version())
    local function describe(value,depth)
        if type(value)~='table' then return tostring(value) end
        if depth==0 then return '{…}' end
        local parts={};local count=0
        for k,v in pairs(value) do
            count=count+1;if count>16 then parts[#parts+1]='…';break end
            parts[#parts+1]=tostring(k)..'='..describe(v,depth-1)
        end
        return '{'..table.concat(parts,', ')..'}'
    end
    for _,id in ipairs({c.fade,c.controller}) do
        local ok,why=pcall(function()
            local f=one('Fixture '..id);if not f then return end
            log('API Fixture '..id)
            for _,ch in pairs(GetUIChannels(f,true)) do
                log('UI INDEX='..tostring(prop(ch,'INDEX'))..' SUBATTRIBUTE='..tostring(prop(ch,'SUBATTRIBUTE')))
                local index=tonumber(prop(ch,'INDEX'))
                if index then log('Programmer '..(index-1)..': '..describe(GetProgPhaser(index-1,false),3)) end
            end
            for _,index in pairs(GetRTChannels(f,false)) do
                local rt=GetRTChannel(index)
                log('RT '..index..': ui_index_first='..tostring(rt and rt.ui_index_first)..' patch='..describe(rt and rt.patch,2))
            end
        end)
        if not ok then log('API-Diagnose: '..tostring(why)) end
    end
end
local function diagnosis(c)
    api_diagnostics(c)
    local ok,result=pcall(preflight,c)
    local lines={'Status\n'..S.status..'\ngrandMA3: '..ma_build_version(),
        'Einrichtung\n'..(ok and 'Grundprüfung erfolgreich' or clean_error(result))..
        '\nDatenpool: '..c.dp..' · Lesesequenz: '..c.main..' · Steuersequenz: '..c.control..
        '\nPatch: '..tostring(safe(function() return verify_patch(c) end) or 'nicht gültig'),
        'Sender\n'..tostring(readvar('SENDER') or 'Nicht aktiv')..'\nGesendete Cue-Wechsel: '..S.events}
    if S.last_error then lines[#lines+1]='Letzter Fehler\n'..clean_error(S.last_error)..'\nWeitere Details stehen im System Monitor.' end
    if ok then
        local items,issues=inventory(c)
        local cues={'OBS-Cues\nGültig: '..#items..' · Probleme: '..#issues}
        for i=1,math.min(#issues,12) do cues[#cues+1]=issues[i] end
        lines[#lines+1]=table.concat(cues,'\n')
        for _,issue in ipairs(issues) do log(issue) end
    end
    lines[#lines+1]='Cue-Einrichtung\nCue-Note: OBS:1 oder OBS:12, ohne Leerzeichen und führende Nullen. '..
        'Die Nummer bezeichnet die Position in der OBS-Szenenliste.\nDas Fade-Preset in Cue Part 0 speichern.'
    lines[#lines+1]='Betriebshinweise\nEin erneutes Goto auf dieselbe aktive Cue löst keinen neuen Szenenaufruf aus. '..
        'Bei einem Schreibfehler wird die Bridge angehalten.'
    local answer=box('Diagnose',table.concat(lines,'\n\n'),{commands={{value=1,name='Sender freigeben'},{value=0,name='Schließen'}}})
    if answer.success and answer.result==1 then
        local yes=box('Sender freigeben','Eine andere Station kann danach übernehmen. Ein laufender MediaLink-Sender verliert seine Sperre und beendet sich beim nächsten Timerlauf.',
            {commands={{value=1,name='Freigeben'},{value=0,name='Abbrechen'}}})
        if yes.success and yes.result==1 then if S.running then stop('Aus') end;clearvar('SENDER');clearvar('HEARTBEAT') end
    end
end
local function about()
    box('Über','MA OBS MediaLink\nVersion '..VERSION..' · BETA\nSzenen, Video und Audio aus grandMA3 steuern.\n\n'..
        'Copyright © 2026 Oliver Cassel\n\nFür Farin und Bela.\n\nDiese Beta-Version befindet sich in Entwicklung.')
end
local function main(display_handle,argument)
    if S.vars and S.vars~=gv() then
        local remembered_owner=S.owner and readvar('SENDER')==S.owner
        stop('Aus · Show-Kontext gewechselt',false);S.ui=false;S.ui_owner=nil
        if remembered_owner then clearvar('SENDER');clearvar('HEARTBEAT') end
    elseif S.running and (readvar('SENDER')~=S.owner or readvar('CONFIG')~=encode(S.config)) then
        stop('Aus · Einrichtung oder Sender geändert',false)
    end
    if S.ui and componentName~='Autostart' and tostring(argument or ''):lower()~='startup' then return end
    local dialog_owner
    local ok,why=xpcall(function()
        if componentName=='Autostart' or tostring(argument or ''):lower()=='startup' then
            S.boot_epoch=(S.boot_epoch or 0)+1;boot(1,S.boot_epoch);return
        end
        local c=validate(decode(readvar('CONFIG')))
        ui_display=display_handle
        dialog_owner={};S.ui=true;S.ui_owner=dialog_owner
        local dialog_context=gv()
        if S.running then
            -- Refresh one timer chain without resetting cue identity or mapping.
            -- Older chains are invalidated even if a show load dropped its timers.
            S.epoch=S.epoch+1;S.last_poll=nil;schedule(S.epoch)
        elseif c.configured==1 then
            local started,why=xpcall(function() startup(c) end,debug.traceback)
            if not started then status_error('Start nicht möglich: '..tostring(why)) end
        end
        while true do
            local result=box('Betrieb',S.status..'\n\nLesesequenz: '..c.main..' · Datenpool: '..c.dp..
                '\nSteuersequenz: '..c.control,
                {ui_rows={},ui_width=1120,ui_status=function() return S.status end,
                 ui_action_name=function() return S.running and 'Stoppen' or 'Starten' end,
                 ui_hint='Vor Änderungen an Einrichtung oder Presets die Bridge stoppen.',
                 commands={{value=1,name=S.running and 'Stoppen' or 'Starten'},
                    {value=2,name='Einrichtung'},{value=3,name='Presets'},
                    {value=4,name='Steuersequenz'},{value=5,name='Prüfen / Diagnose'},
                    {value=6,name='Über'},{value=0,name='Schließen'}}})
            if gv()~=dialog_context or not result.success or result.result==0 then break end
            local action_ok,problem=xpcall(function()
                if result.result==1 then if S.running then stop('Aus') else start(c) end
                elseif result.result==2 then c=setup(c)
                elseif result.result==3 then c=presets_ui(c)
                elseif result.result==4 then adoption(c)
                elseif result.result==5 then diagnosis(c)
                elseif result.result==6 then about() end
            end,debug.traceback)
            if not action_ok then box('Hinweis',report_error(problem)..'\n\nTechnische Details stehen im System Monitor.') end
        end
    end,debug.traceback)
    if dialog_owner and S.ui_owner==dialog_owner then S.ui=false;S.ui_owner=nil end
    if not ok then box('Hinweis',report_error(why)..'\n\nTechnische Details stehen im System Monitor.') end
end
-- Export a narrow seam for the Lua test adapter (not enabled in a MA show).
if rawget(_G,'MAOBS_TEST_MODE') then
    return {raw24=raw24,validate=validate,default_config=default_config,encode=encode,decode=decode,
        plan_presets=plan_presets,generate=generate,read_cue=read_cue,inventory=inventory,
        preflight=preflight,verify_patch=verify_patch,revision=revision,find_revision=find_revision,isolated=isolated,
        start=start,stop=stop,tick=tick,startup=startup,main=main,state=S,adoption=adoption,
        setup=setup,presets_ui=presets_ui,clean_error=clean_error,
        readvar=readvar,writevar=writevar,clearvar=clearvar,diagnosis=diagnosis,
        dmx16=dmx16,cue_key=cue_key,decode_revision_label=decode_revision_label,
        revision_label=revision_label,matching_revision=matching_revision,ma_build_version=ma_build_version,
        control_owned=control_owned,control_tag=control_tag,label_control=label_control,
        native_dialog=native_dialog,box=box}
end
return main
