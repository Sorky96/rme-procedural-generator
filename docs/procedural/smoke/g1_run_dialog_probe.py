"""Run a NEW private normal-entry RME observer; native UI commands are manual/computer-use.

No source map files are opened. Curated assets and public menu config are copied to private output.
Sequence: invalid seed, Preview, Validate, change seed to1728, restore1729/Preview, Cancel;
reopen from Map menu, Preview, Place, then real GUI Undo and Redo. The observer exits normally.
--roundtrip --roundtrip-side 100|500 --roundtrip-timing: Preview/Validate, then the dialog's Place button,
Edit > Undo, Edit > Redo and File > Save from the menu bar (not the toolbar); close RME manually after
ROUNDTRIP_TIMING_PASS. Writes run/roundtrip-timing.csv with one row per operation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--matrix',action='store_true',help='500x500, selected holes, input and stale Place refusals')
parser.add_argument('--selection-only',action='store_true',help='Only the sparse-selection GUI regression; not the full matrix')
parser.add_argument('--resources',action='store_true',help='Three Preview/Validate/Cancel cycles per size, process memory and GUI heartbeat baseline')
parser.add_argument('--cancel-preview',action='store_true',help='Cancel an active 500x500 Preview, retry with seed 1728, then measure three completed 500x500 cycles')
parser.add_argument('--resource-side',type=int,choices=(500,1000,2000),help='Run one resource cycle at this size; combine with --cancel-preview for in-flight Cancel and retry')
parser.add_argument('--refusals',action='store_true',help='Native multifloor, busy, inert live, snapshot allocation and no-client refusals')
parser.add_argument('--roundtrip',action='store_true',help='Place, Undo, Redo, save and reopen a generated OTBM 5 map')
parser.add_argument('--roundtrip-side',type=int,choices=(100,500),default=100)
parser.add_argument('--roundtrip-timing',action='store_true',help='Round-trip side (100 or 500): per-operation CSV for Place/Undo/Redo/Save; no reference preview, terrain scan or reopen')
parser.add_argument('--invalidation',action='store_true',help='Edit input and close the window during active Preview jobs')
parser.add_argument('--focus-trace',action='store_true',help='Capture a local call stack and window enabled-state for SetFocus errors (invalidation or refusals probe)')
for name in ('output','profile_data','config'):
    parser.add_argument(name,type=lambda value:Path(value).resolve())
args=parser.parse_args()
args.matrix=args.matrix or args.selection_only
args.resources=args.resources or args.cancel_preview or args.resource_side is not None
if args.resources and args.matrix:
    parser.error('--resources cannot be combined with --matrix or --selection-only')
if args.refusals and (args.resources or args.matrix):
    parser.error('--refusals runs independently')
if args.roundtrip and (args.resources or args.matrix or args.refusals):
    parser.error('--roundtrip runs independently')
if args.roundtrip_side!=100 and not args.roundtrip:
    parser.error('--roundtrip-side requires --roundtrip')
if args.roundtrip_timing and not args.roundtrip:
    parser.error('--roundtrip-timing requires --roundtrip')
if args.invalidation and (args.resources or args.matrix or args.refusals or args.roundtrip):
    parser.error('--invalidation runs independently')
if args.focus_trace and not (args.invalidation or args.refusals or args.roundtrip):
    parser.error('--focus-trace requires --invalidation, --refusals or --roundtrip')
repo=Path(__file__).resolve().parents[3]
if not args.output.is_relative_to(repo/'local-data/procedural-smoke'):
    raise ValueError('Output must be a NEW private procedural-smoke directory')
args.output.mkdir(parents=True,exist_ok=False)
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
sources=[repo/name for name in ('source/ui/dialogs/terrain_generator_dialog.h',
    'source/ui/dialogs/terrain_generator_dialog.cpp','source/ui/main_menubar.h',
    'source/ui/main_menubar.cpp','source/ui/menubar/menubar_action_manager.cpp',
    'source/procedural/CMakeLists.txt','data/menubar.xml','tests/test_procedural_dialog.cpp',
    'docs/procedural/smoke/g1_run_dialog_probe.py','docs/procedural/smoke/g1_build_input_probe.py')]
probe=repo/'tests'/('test_procedural_dialog_invalidation.cpp' if args.invalidation else
    'test_procedural_dialog_roundtrip.cpp' if args.roundtrip else
    'test_procedural_dialog_refusals.cpp' if args.refusals else
    'test_procedural_dialog_resources.cpp' if args.resources else
    'test_procedural_dialog_matrix.cpp' if args.matrix else 'test_procedural_dialog.cpp')
if probe not in sources:
    sources.append(probe)
if args.focus_trace:
    sources.append(repo/'tests/procedural_focus_trace.h')
source_hashes={str(path.relative_to(repo)):sha(path) for path in sources}
build_command=[sys.executable,str(Path(__file__).with_name('g1_build_input_probe.py')),
    str(args.output),'--source',str(probe),'--application-entry']
if args.focus_trace:
    build_command.append('--symbols')
subprocess.run(build_command,cwd=repo,check=True)
run=args.output/'run';data=run/'data';data.mkdir(parents=True)
source_files=[args.profile_data/'clients.toml',args.profile_data/'menubar.xml',args.config,repo/'data/menubar.xml']
source_files += [p for p in (args.profile_data/'1098').rglob('*') if p.is_file()]
original={str(path):sha(path) for path in source_files}
shutil.copytree(args.profile_data/'1098',data/'1098')
shutil.copy2(args.profile_data/'clients.toml',data/'clients.toml')
# Exercise the production menu, without changing the accepted curated profile's menu.
shutil.copy2(repo/'data/menubar.xml',data/'menubar.xml')
config=args.config.read_text(encoding='utf-8')
config=re.sub(r'^recent_edited_map_path = .*?$',"recent_edited_map_path = ''",config,flags=re.M)
config=re.sub(r'^recent_files = \[[\s\S]*?^\]','recent_files = []',config,flags=re.M)
settings={'data_directory':"'"+data.as_posix()+"/'",'use_custom_data_directory':'1',
    'indirectory_installation':'1','only_one_instance':'false','welcome_dialog':'false',
    'use_updater':'false','goto_website_on_boot':'false'}
for key,value in settings.items():
    config,count=re.subn(r'^'+key+r' = .*?$',key+' = '+value,config,flags=re.M)
    assert count==1,key
(run/'config.toml').write_text(config,encoding='utf-8')
(run/'.g1-dialog-probe').write_text('Private synthetic editor only',encoding='utf-8')
if args.selection_only:
    (run/'.g1-dialog-selection-only').write_text('Sparse selection regression only',encoding='utf-8')
if args.resources:
    (run/'.g1-dialog-resources').write_text('Process resource baseline only',encoding='utf-8')
if args.cancel_preview:
    (run/'.g1-dialog-cancel-preview').write_text('Interrupt and retry one private Preview',encoding='utf-8')
if args.resource_side:
    (run/'resource-side.txt').write_text(str(args.resource_side),encoding='utf-8')
if args.refusals:
    (run/'.g1-dialog-refusals').write_text('Refusal fixtures on one private synthetic editor',encoding='utf-8')
if args.roundtrip:
    (run/'.g1-dialog-roundtrip').write_text('Synthetic generator OTBM 5 round-trip',encoding='utf-8')
    if args.roundtrip_side==500:
        (run/'.g1-dialog-roundtrip-500').touch()
    if args.roundtrip_timing:
        (run/'.g1-dialog-roundtrip-timing').write_text('Place/Undo/Redo/Save timing only; no reopen',encoding='utf-8')
if args.invalidation:
    (run/'.g1-dialog-invalidation').write_text('In-flight edit and window close on a synthetic editor',encoding='utf-8')
if args.focus_trace:
    (run/'.g1-focus-trace').touch()
shutil.copytree(repo/'build-ninja/assets',args.output/'assets')
env=os.environ.copy();env['PATH']=str(repo/'build-ninja')+os.pathsep+env.get('PATH','')
if args.focus_trace:
    env['_NT_SYMBOL_PATH']=';'.join(map(str,(args.output,repo/'build-ninja/vcpkg_installed/x64-windows/bin')))
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=1
with (args.output/'stdout.log').open('w',encoding='utf-8') as out,(args.output/'stderr.log').open('w',encoding='utf-8') as err:
    process=subprocess.Popen([str(args.output/'snapshot_probe.exe')],cwd=run,env=env,stdout=out,stderr=err,startupinfo=startup)
    (args.output/'probe-pid.txt').write_text(str(process.pid))
    code=process.wait(timeout=1800 if args.matrix or args.resources else 900)  # Timeout leaves this SAME private child.
report=(run/'dialog-probe.log').read_text(encoding='utf-8')
markers=['DIALOG_FIXTURE_READY','DIALOG_INVALID_SEED_PASS','DIALOG_PREVIEW_PASS','DIALOG_VALIDATE_PASS',
    'DIALOG_INPUT_INVALIDATION_PASS','DIALOG_CANCEL_PASS','DIALOG_GUI_COMMIT_PASS','DIALOG_GUI_UNDO_PASS',
    'DIALOG_GUI_REDO_PASS','DIALOG_PASS']
if args.matrix:
    markers=['MATRIX_500_READY','MATRIX_EMPTY_SELECTION_REFUSED','MATRIX_INVALID_BRUSH_REFUSED',
        'MATRIX_ZERO_WEIGHTS_REFUSED','MATRIX_BOUNDS_REFUSED','MATRIX_500_PREVIEW_PASS','MATRIX_500_COMMIT_PASS',
        'MATRIX_500_UNDO_PASS','MATRIX_500_REDO_PASS','MATRIX_SELECTION_READY','MATRIX_SELECTION_PREVIEW_PASS',
        'MATRIX_STALE_MEMBERSHIP_INJECTED','MATRIX_STALE_PLACE_REFUSED','MATRIX_SELECTION_COMMIT_PASS',
        'MATRIX_SELECTION_UNDO_PASS','MATRIX_SELECTION_REDO_PASS','MATRIX_HISTORY_GUIDANCE_PASS',
        'MATRIX_HISTORY_RETRY_PASS','MATRIX_SELECTION_PRESENTATION_PASS','MATRIX_PASS']
if args.selection_only:
    markers=['MATRIX_SELECTION_READY','MATRIX_SELECTION_PREVIEW_PASS','MATRIX_SELECTION_PRESENTATION_PASS',
        'MATRIX_STALE_MEMBERSHIP_INJECTED','MATRIX_STALE_PLACE_REFUSED','MATRIX_SELECTION_COMMIT_PASS',
        'MATRIX_SELECTION_UNDO_PASS','MATRIX_SELECTION_REDO_PASS','MATRIX_PASS']
if args.resources:
    markers=['RESOURCE_HOST','RESOURCE_PASS']
    markers += ['RESOURCE_CANCEL_PASS\t'+str(cycle) for cycle in range(1,2 if args.resource_side else 4 if args.cancel_preview else 7)]
if args.cancel_preview:
    markers += ['RESOURCE_IN_FLIGHT_CANCEL_PASS','RESOURCE_CANCEL_RETRY_PASS']
if args.refusals:
    markers=['REFUSAL_MULTIFLOOR_READY','REFUSAL_PASS']
    markers += ['REFUSAL_CASE_PASS\t'+str(case) for case in range(6)]
if args.roundtrip:
    markers=['ROUNDTRIP_'+name for name in ('FIXTURE_READY','PLACE_PASS','UNDO_PASS','REDO_PASS',
        'SAVED','CLOSED','REOPEN_PASS','PASS')]
if args.roundtrip_timing:
    markers=['ROUNDTRIP_'+name for name in ('TIMING_QUANTIZATION','FIXTURE_READY','PLACE_PASS','UNDO_PASS',
        'REDO_PASS','SAVED','TIMING_PASS')]
if args.invalidation:
    markers=['INVALIDATION_'+name for name in ('FIXTURE_READY','EDIT_IN_FLIGHT','EDIT_PASS','EDIT_RETRY_PASS',
        'CLOSE_READY','CLOSE_IN_FLIGHT','CLOSE_PASS','REOPEN_PASS','PASS')]
unchanged=all(sha(Path(path))==value for path,value in original.items())
current=all(sha(repo/path)==value for path,value in source_hashes.items())
stderr_empty=(args.output/'stderr.log').stat().st_size==0
passed=code==0 and unchanged and current and stderr_empty and 'FAILED\t' not in report and all(
    any(line==marker or line.startswith(marker+'\t') for line in report.splitlines()) for marker in markers)
if args.roundtrip_timing:
    operations=['place','undo','redo','save']
    lines=report.splitlines()
    # Markers and timing rows must all appear, once each, in the expected order.
    hits=[[i for i,line in enumerate(lines) if line==m or line.startswith(m+'\t')] for m in markers]
    positions=[found[0] for found in hits if len(found)==1]
    markers_ordered=len(positions)==len(markers) and positions==sorted(positions)
    report_rows=[line.split('\t')[1:3] for line in lines if line.startswith('ROUNDTRIP_TIMING_ROW\t')]
    header=('order,operation,side,elapsed_observed_ms,observer_tick_ms,max_heartbeat_gap_ms,baseline_ws,peak_ws,end_ws,'
        'baseline_private,peak_private,end_private,samples,max_sample_gap_ms')
    csv_path=run/'roundtrip-timing.csv'
    csv_lines=csv_path.read_text(encoding='utf-8').splitlines() if csv_path.is_file() else []
    def complete_row(index,line):
        fields=line.split(',')
        if len(fields)!=14 or fields[:3]!=[str(index+1),operations[index],str(args.roundtrip_side)] or fields[4]!='20':
            return False
        try:
            numbers=[float(value) for value in fields[3:]]
        except ValueError:
            return False
        elapsed,_,heartbeat,base_ws,peak_ws,end_ws,base_pv,peak_pv,end_pv,samples,_=numbers
        return (elapsed>0 and heartbeat>=0 and samples>=1 and min(base_ws,end_ws,base_pv,end_pv)>0 and
            peak_ws>=max(base_ws,end_ws) and peak_pv>=max(base_pv,end_pv))
    timing_complete=(markers_ordered and len(csv_lines)==5 and csv_lines[0]==header and
        report_rows==[[str(i+1),op] for i,op in enumerate(operations)] and
        all(complete_row(i,line) for i,line in enumerate(csv_lines[1:])))
    # Only the already recognized wxWidgets SetFocus warning (0x00000057) is tolerated in stderr.
    stderr_lines=[line for line in (args.output/'stderr.log').read_text(encoding='utf-8',errors='replace').splitlines() if line.strip()]
    focus_warning_only=bool(stderr_lines) and all('SetFocus' in line and '0x00000057' in line for line in stderr_lines)
    passed=(code==0 and unchanged and current and (stderr_empty or focus_warning_only) and
        'FAILED\t' not in report and timing_complete)
result={'passed':passed,'exit_code':code,'matrix':args.matrix,'selection_only':args.selection_only,'resources':args.resources,'input_assets_unchanged':unchanged,'current_sources':current,
    'stderr_empty':stderr_empty,'sources':source_hashes,'source_assets':original,'report':report,
    'scope':('Process memory / GUI heartbeat baseline: Preview, Validate, Cancel after completion; not cancellation during generation or full G1' if args.resources else
        'Sparse-selection GUI regression only: presentation, stale refusal, Place/Undo/Redo; not full matrix or G1' if args.selection_only else
        'Native GUI matrix: 500x500 and five-member sparse selection, input/stale refusals and Place/Undo/Redo; not full G1' if args.matrix else
        'Native dialog at100x100: real menu/Preview/Validate/Cancel/Place/Undo/Redo, minimap only; not G1 E2E or sprite preview')}
if args.resources:
    result['resource_csv_sha256']=sha(run/'resources.csv')
    result['resource_side']=args.resource_side
    result['cancel_preview']=args.cancel_preview
if args.cancel_preview:
    result['scope']='Native 500x500 in-flight Cancel and retry, plus three completed resource cycles'
if args.resource_side:
    result['scope']=f'Native {args.resource_side}x{args.resource_side} resource cycle' + (' with in-flight Cancel and retry' if args.cancel_preview else '')
if args.refusals:
    result['refusals']=True
    result['scope']='Native GUI refusal cases and Preview recovery on a synthetic map'
if args.roundtrip:
    result['roundtrip']=True
    result['roundtrip_side']=args.roundtrip_side
    result['scope']='Native Place/Undo/Redo and OTBM 5 save/close/reopen on one synthetic map'
if args.roundtrip_timing:
    result['roundtrip_timing']=True
    result['timing_complete']=timing_complete
    result['stderr_focus_warning_only']=focus_warning_only
    if passed and focus_warning_only:
        result['status']='PARTIAL_G1_GUI_TIMING_FOCUS_WARNING'
    if csv_path.is_file():
        result['timing_csv_sha256']=sha(csv_path)
    side=args.roundtrip_side
    result['scope']=(f'Native {side}x{side} Place/Undo/Redo/Save timing: elapsed runs from the real command event to the first 20 ms '
        'observer tick seeing the history/dirty transition (quantized; Windows timer ~15.6 ms), 5 ms memory sampler; '
        'tile counts and fixture only, no terrain payload comparison, close or reopen')
if args.invalidation:
    result['invalidation']=True
    result['scope']='Native edit and window close during Preview, plus retry and reopening'
(args.output/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({key:value for key,value in result.items() if key not in ('source_assets','sources')},indent=2))
sys.exit(0 if passed else 1)
