"""Build the self-contained MA component; no external files are loaded on the desk."""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parent.parent
CORE = ROOT / 'source' / 'medialink_core.lua'
OUTPUT = ROOT / 'MA3' / 'MA_OBS_MediaLink.lua'

HEADER = '''-- MA OBS MediaLink v0.43 BETA | Copyright © 2026 Oliver Cassel
-- Für Farin und Bela.
-- Loading this component only returns an entry function. MA APIs run on invocation.
local plugin_arguments=table.pack(...)
local CORE_SOURCE = [====[
'''

FOOTER = ''']====]
local runtime
local function traceback(value)
    if type(debug)=='table' and type(debug.traceback)=='function' then
        return debug.traceback(tostring(value),2)
    end
    return tostring(value)
end
local function show_error(phase,detail)
    local report='MA OBS MediaLink 0.43: '..phase..'\\n'..tostring(detail)
    local printed=false
    if type(ErrPrintf)=='function' then printed=pcall(ErrPrintf,'%s',report) end
    if not printed and type(Printf)=='function' then pcall(Printf,'%s',report) end
    local first=tostring(detail):match('([^\\n]+)') or 'Unbekannter Fehler'
    if type(MessageBox)=='function' then
        pcall(MessageBox,{
            title='MA OBS MediaLink · v0.43 BETA – Startfehler',
            message='Das Plugin konnte nicht gestartet werden.\\n\\nSchritt: '..phase..'\\n'..first..
                '\\n\\nWeitere Details stehen im System Monitor.',
            commands={{value=0,name='Schließen'}}})
    end
end
local function main(...)
    local call_arguments=table.pack(...)
    local phase='Codeprüfung'
    local result=table.pack(xpcall(function()
        if not runtime then
            local chunk,why=load(CORE_SOURCE,'@MA OBS MediaLink v0.43 core','t',_ENV)
            if not chunk then error(why,0) end
            phase='Initialisierung'
            local entry=chunk(table.unpack(plugin_arguments,1,plugin_arguments.n))
            assert(type(entry)=='function','Der Plugin-Kern hat keine Startfunktion zurückgegeben.')
            runtime=entry
        end
        phase='Betrieb'
        return runtime(table.unpack(call_arguments,1,call_arguments.n))
    end,traceback))
    if not result[1] then show_error(phase,result[2]);return end
    return table.unpack(result,2,result.n)
end
return main
'''

def build():
    core = CORE.read_text(encoding='utf-8')
    assert ']====]' not in core
    if not core.endswith('\n'):
        core += '\n'
    return (HEADER + core + FOOTER).encode('utf-8')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    data = build()
    if args.check:
        assert OUTPUT.read_bytes() == data, 'Importable Lua component is out of date'
        print('PASS: embedded core exactly matches maintained source and bootstrap')
    else:
        OUTPUT.write_bytes(data)
        print(f'Built {OUTPUT.name}: {len(data)} bytes')
