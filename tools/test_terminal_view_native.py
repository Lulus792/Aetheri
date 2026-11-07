"""Run unchanged real workflows and bind their result to the NexaUI presenter."""
import hashlib,json,re,subprocess,sys
from pathlib import Path


def main():
    mode,*arguments=sys.argv[1:]
    scripts={'native':'test_terminal_ui.py','clipboard':'test_terminal_clipboard.py',
             'ime':'test_terminal_composition.py','files':'test_files_ui.py'}
    assert mode in scripts
    root=Path(__file__).resolve().parents[1]
    inputs=json.loads((root/'spec/ui-inputs.json').read_text())['terminal_font']
    fonts={}
    for item in (inputs,inputs['bold']):
        for key,expected in (('path',item['sha256']),('license_path',item['license_sha256'])):
            path=Path(item[key]);actual=hashlib.sha256(path.read_bytes()).hexdigest();assert actual==expected,path
            fonts[str(path)]=actual
    # Preserve the old driver's original output too, including failed attempts.
    result=subprocess.run([sys.executable,str(root/'tools'/scripts[mode]),*arguments],capture_output=True,text=True)
    if result.returncode:
        print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr);return result.returncode
    records=[]
    for line in result.stdout.splitlines():
        if not line.startswith('{'):continue
        try:record=json.loads(line)
        except json.JSONDecodeError:continue
        if isinstance(record,dict) and 'artifacts' in record:records.append(record)
    assert len(records)==1,records
    record=records[0];native=Path(record['artifacts']);data=json.loads((native/'result.json').read_text())
    (native/'original-driver.log').write_text(result.stdout+result.stderr)
    assert data['result']=='passed'
    terminal=Path(arguments[2] if mode=='files' else arguments[1]).resolve()
    assert terminal.name=='aetheri-terminal-nexaui' and data['binaries'][str(terminal)]==hashlib.sha256(terminal.read_bytes()).hexdigest()
    sdk=json.loads((root/'spec/nexaui-inputs.json').read_text())
    signature='PRESENTATION nexaui '+sdk['version']+' sdl-host rgba8'
    logs=[p for p in native.glob('*.log') if p.name!='original-driver.log' and signature in p.read_text(errors='replace')]
    assert logs and all(re.search(r'^NEXAUI_FRAME \d+ width=\d+ height=\d+ stride=\d+$',p.read_text(errors='replace'),re.M) for p in logs)
    symbols=subprocess.check_output(['nm','--defined-only',str(terminal)],text=True)
    assert 'ae_terminal_view_grid' in symbols and 'ae_terminal_session_enqueue' in symbols
    assert 'ae_test_push_event' not in symbols and '__wrap_write' not in symbols
    build=Path(arguments[-2] if mode=='files' else arguments[-1]).resolve()
    cache=(build/'CMakeCache.txt').read_text()
    prefix=Path(re.search(r'^AETHERI_NEXAUI_PREFIX:PATH=(.+)$',cache,re.M)[1])
    config=prefix/'lib/cmake/NexaUI/NexaUIConfig.cmake'
    assert 'set(NexaUI_SOURCE_REVISION "'+sdk['revision']+'")' in config.read_text()
    libraries={p.resolve() for p in (prefix/'lib').glob('libnexaui.so*')};assert len(libraries)==1
    library=libraries.pop()
    presentation={'version':sdk['version'],'revision':sdk['revision'],'library':str(library),
                  'sha256':hashlib.sha256(library.read_bytes()).hexdigest(),'fonts':fonts,
                  'actual_frame_logs':[p.name for p in logs],
                  'scope':'NexaUI core/cell-grid/raster with the original SDL host; no native accessibility transport or default-owner adoption'}
    (native/'nexaui-result.json').write_text(json.dumps(presentation,indent=2)+'\n')
    print(json.dumps({**record,'presentation':presentation}));return 0


if __name__=='__main__':raise SystemExit(main())
