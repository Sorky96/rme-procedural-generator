"""Independently verify current objects/sources for a completed native dialog probe.

The earlier core probe remains historical evidence; it is not rerun or relabelled as GUI coverage.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--matrix',action='store_true')
parser.add_argument('--selection-only',action='store_true')
parser.add_argument('--protected-selection',action='store_true',
                    help='Selection-only run with UID 32101 protection markers, on the staged Preview revision')
parser.add_argument('--resources',action='store_true')
parser.add_argument('--cancel-preview',action='store_true')
parser.add_argument('--refusals',action='store_true')
parser.add_argument('--roundtrip',action='store_true')
parser.add_argument('--roundtrip-timing',action='store_true',
                    help='100x100 or 500x500 Place/Undo/Redo/Save timing run; no reopen, so no OTBM comparison')
parser.add_argument('--archived-harness-root',type=lambda value:Path(value).resolve(),
                    help='Timing only: source root holding the exact recorded runner and observer bytes of an older run')
parser.add_argument('--invalidation',action='store_true')
parser.add_argument('run',type=lambda value:Path(value).resolve())
args=parser.parse_args()
if args.archived_harness_root and not args.roundtrip_timing:
    parser.error('--archived-harness-root requires --roundtrip-timing')
args.roundtrip=args.roundtrip or args.roundtrip_timing
if args.protected_selection and (args.refusals or args.roundtrip or args.invalidation):
    parser.error('--protected-selection runs independently')
args.selection_only=args.selection_only or args.protected_selection
args.matrix=args.matrix or args.selection_only
args.resources=args.resources or args.cancel_preview
if args.resources and args.matrix:
    parser.error('--resources cannot be combined with --matrix or --selection-only')
staged_preview=(args.resources or args.refusals or args.roundtrip or args.invalidation or
                args.protected_selection)
repo=Path(__file__).resolve().parents[3]
base=repo/'local-data/procedural-smoke'
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
def require(condition,message):
    if not condition:
        raise RuntimeError(message)

require(args.run.is_relative_to(base),'Expected private probe output')
run=json.loads((args.run/'verification.json').read_text())
manifest=json.loads((args.run/'build-manifest.json').read_text())
raw_stderr=(args.run/'stderr.log').read_text(encoding='utf-8',errors='replace').splitlines()
# Lines written by the Google Drive shell extension that the native Open dialog loads into the
# process; they are not RME output. They are ignored only if recorded, and everything else must
# still satisfy the strict stderr rules below.
external_markers=('InitGoogle()','GoogleDriveFSPipe','=== Source Location Trace: ===','shell_ipc_client.cc')
ignored_external=[line for line in raw_stderr if any(marker in line for marker in external_markers)]
stderr=[line for line in raw_stderr if line not in ignored_external]
partial_focus_warning=((args.refusals or args.roundtrip or args.invalidation) and bool(stderr)
    and len(set(stderr))==1
    and "\\src\\msw\\window.cpp(" in stderr[0]
    and "'SetFocus' failed with error 0x00000057" in stderr[0])
external_noise_only=bool(ignored_external) and not stderr
if partial_focus_warning or external_noise_only:
    report=(args.run/'run/dialog-probe.log').read_text(encoding='utf-8')
    require(report==run['report'] and 'FAILED\t' not in report,'Unexpected runtime report')
    if args.refusals:
        cases=[line.split('\t',2)[1] for line in report.splitlines() if line.startswith('REFUSAL_CASE_PASS\t')]
        require(cases==list(map(str,range(6))) and report.endswith('REFUSAL_PASS\n'),'Refusal sequence incomplete')
    elif args.roundtrip and not args.roundtrip_timing:
        expected=['ROUNDTRIP_'+name for name in ('FIXTURE_READY','PLACE_PASS','UNDO_PASS','REDO_PASS',
            'SAVED','CLOSED','REOPEN_PASS','PASS')]
        require(report.splitlines()==expected,'Round-trip sequence incomplete')
    require(run['current_sources'] and run['input_assets_unchanged'],'Inputs changed during runtime')
require((run['passed'] or partial_focus_warning or external_noise_only) and run['exit_code']==0,'Native UI sequence did not pass')
require(run.get('matrix',False)==args.matrix,'Use --matrix for a matrix run')
require(run.get('selection_only',False)==args.selection_only,'Selection-only scope mismatch')
require(run.get('resources',False)==args.resources,'Resource baseline scope mismatch')
require(run.get('refusals',False)==args.refusals,'Refusal scope mismatch')
require(run.get('roundtrip',False)==args.roundtrip,'Round-trip scope mismatch')
require(run.get('roundtrip_timing',False)==args.roundtrip_timing,'Round-trip timing scope mismatch')
require(run.get('invalidation',False)==args.invalidation,'Invalidation scope mismatch')
require(manifest['entry_override']=='normal application entry','Unexpected entry override')
# Only these two harness files may come from --archived-harness-root; everything else stays current.
archived_names=('docs/procedural/smoke/g1_run_dialog_probe.py','tests/test_procedural_dialog_roundtrip.cpp')
probe_source=Path(manifest['probe_source'])
probe_key=probe_source.relative_to(repo).as_posix() if probe_source.is_relative_to(repo) else None
archived={}
if args.archived_harness_root:
    require(args.archived_harness_root.is_dir(),'Archived harness root missing')
    recorded={name:set() for name in archived_names}
    for sources in (manifest['sources'],run['sources']):
        for name,value in sources.items():
            if name.replace('\\','/') in recorded:
                recorded[name.replace('\\','/')].add(value)
    if probe_key in recorded:
        recorded[probe_key].add(manifest['probe_source_sha256'])
    require(probe_key==archived_names[1],'Archived harness requires the round-trip observer')
    for name,values in recorded.items():
        require(len(values)==1,'Archived harness file unrecorded or recorded with conflicting hashes: '+name)
        path=args.archived_harness_root/name
        require(path.is_file() and sha(path)==next(iter(values)),'Archived harness bytes differ from manifest: '+name)
        archived[name]=next(iter(values))
def current_source(name,value,message):
    if name.replace('\\','/') in archived:
        require(archived[name.replace('\\','/')]==value,message+name)
    else:
        require(sha(repo/name)==value,message+name)
for name,value in manifest['sources'].items():
    current_source(name,value,'Changed source: ')
for name,value in run['sources'].items():
    current_source(name,value,'Changed dialog input: ')
for name,value in manifest['objects'].items():
    path=args.run/name if name=='input_probe.obj' else repo/'build-ninja'/name
    require(sha(path)==value,'Changed linked object: '+name)
require(sha(repo/'build-ninja/rme.exe')==manifest['normal_exe_sha256'],'Changed normal EXE')
require(sha(args.run/'snapshot_probe.exe')==manifest['exe_sha256'],'Changed probe EXE')
if probe_key in archived:
    require(archived[probe_key]==manifest['probe_source_sha256'],'Changed observer')
else:
    require(sha(probe_source)==manifest['probe_source_sha256'],'Changed observer')
require(not stderr or partial_focus_warning,'Unexpected runtime stderr')
for name,value in run['source_assets'].items():
    require(sha(Path(name))==value,'Changed curated input: '+name)

prior=base/'g1-commit-complete-rme-20261103'
old=json.loads((prior/'build-manifest.json').read_text())
changes=sorted(name.replace('\\','/') for name,value in old['sources'].items() if sha(repo/name)!=value)
expected_changes=['source/ui/main_menubar.cpp','source/ui/menubar/menubar_action_manager.cpp']
staged_cpp=['source/editor/operations/procedural_draw_operations.cpp',
            'source/procedural/adapter/terrain_preparation.cpp']
if staged_preview:
    expected_changes += staged_cpp
require(changes==sorted(expected_changes),
        'Unexpected production CPP change since accepted core probe: '+str(changes))
core=json.loads((prior/'current-verification.json').read_text())
core_changes=sorted(name for name,value in core['sources'].items() if sha(repo/name)!=value)
expected_core_changes=['source/procedural/CMakeLists.txt']
if staged_preview:
    expected_core_changes += ['docs/procedural/smoke/g1_build_input_probe.py',
                              'source/editor/operations/draw_operations.h',
                              'source/editor/operations/procedural_draw_operations.cpp',
                              'source/procedural/adapter/terrain_preparation.cpp',
                              'source/procedural/adapter/terrain_preparation.h']
require(core_changes==sorted(expected_core_changes),
        'Core evidence inputs changed: '+str(core_changes))
if args.matrix:
    previous=json.loads((base/'g1-dialog-matrix-rme-20261103'/'build-manifest.json').read_text())
    ui_changes=sorted(name.replace('\\','/') for name,value in previous['sources'].items() if sha(repo/name)!=value)
    expected_ui_changes=['source/ui/dialogs/terrain_generator_dialog.cpp']
    if args.protected_selection:
        # Staged Preview CPPs may differ only where the GUI matrix still had the historical core hash.
        normalized=lambda sources:{name.replace('\\','/'):value for name,value in sources.items()}
        matrix_sources,core_sources=normalized(previous['sources']),normalized(old['sources'])
        expected_ui_changes += [name for name in staged_cpp
                                if name in matrix_sources and matrix_sources[name]==core_sources[name]]
    require(ui_changes==sorted(expected_ui_changes),
            'Unexpected production CPP change since GUI matrix: '+str(ui_changes))
protection=[]
if args.protected_selection:
    report=(args.run/'run/dialog-probe.log').read_text(encoding='utf-8')
    lines=report.splitlines()
    require(report==run['report'] and 'FAILED\t' not in report and lines[-1:]==['MATRIX_PASS'],
            'Protected selection report incomplete')
    require(not any(line.startswith('MATRIX_500_') for line in lines),'Unexpected 500x500 markers in selection-only run')
    def unique(marker):
        found=[index for index,line in enumerate(lines) if line==marker or line.startswith(marker+'\t')]
        require(len(found)==1,'Expected exactly one marker: '+marker)
        return found[0]
    pairs=[('EXCLUDED','READY'),('PLACE_INTACT','COMMIT_PASS'),('UNDO_INTACT','UNDO_PASS'),('REDO_INTACT','REDO_PASS')]
    positions=[]
    for guard,step in pairs:
        marker='MATRIX_SELECTION_PROTECTED_UID_'+guard
        at,target=unique(marker),unique('MATRIX_SELECTION_'+step)
        require(lines[at]==marker+'\t32101','Protection marker without UID 32101: '+marker)
        require(target==at+1,'Protection marker does not precede selection '+step+': '+marker)
        positions += [at,target]
        protection.append(marker)
    require(positions==sorted(positions),'Protection markers out of order')
    injected,refused=unique('MATRIX_STALE_MEMBERSHIP_INJECTED'),unique('MATRIX_STALE_PLACE_REFUSED')
    require(positions[1]<injected<refused<positions[2],'Stale Place refusal missing or out of order')
resources=[]
if args.resources:
    require(sha(args.run/'run/resources.csv')==run['resource_csv_sha256'],'Resource CSV changed after run')
    if args.cancel_preview:
        require(all(marker in run['report'] for marker in ('RESOURCE_IN_FLIGHT_CANCEL_PASS','RESOURCE_CANCEL_RETRY_PASS')),
                'Missing in-flight cancellation or retry evidence')
    with (args.run/'run/resources.csv').open(newline='') as stream:
        resources=list(csv.DictReader(stream))
    expected=[(str(cycle),'100' if cycle<=3 else '500',command) for cycle in range(1,7)
              for command in ('preview','validate','cancel','settled')]
    if args.cancel_preview:
        expected=[('1','500','interrupted-preview'),('1','500','interrupt')]
        expected += [(str(cycle),'500',command) for cycle in range(1,4)
                     for command in ('preview','validate','cancel','settled')]
    require([(row['cycle'],row['side'],row['command']) for row in resources]==expected,'Resource cases incomplete')
    for row in resources:
        require(float(row['elapsed_ms'])>0 and int(row['samples'])>0,'Missing measurement samples')
        for suffix in ('ws','private'):
            require(int(row['peak_'+suffix])>=int(row['baseline_'+suffix])>0,'Invalid process counters')
result={'result':('PASS_G1_GUI_RESOURCE_BASELINE_ONLY' if args.resources else
        'PASS_G1_NATIVE_DIALOG_SELECTION_UX_ONLY' if args.selection_only else
        'PASS_G1_NATIVE_DIALOG_UX_MATRIX_ONLY' if args.matrix else 'PASS_G1_NATIVE_DIALOG_100_ONLY'),'g1_gate':'IN PROGRESS',
        'normal_exe_sha256':manifest['normal_exe_sha256'],'probe_exe_sha256':manifest['exe_sha256'],
        'rme_objects_checked':len(manifest['objects'])-1,'production_sources_checked':len(manifest['sources']),
        'changed_cpp_since_historical_core':changes,'core_evidence_input_changes':core_changes,
        'historical_core_evidence':'g1-commit-complete-rme-20261103: relevant core inputs unchanged; earlier binary',
        'not_verified':(['Cancellation during generation, asynchronous scheduling, resource bounds under concurrent jobs'] if args.resources else
            ['500x500 GUI and history/input refusal matrix on this final paint-only revision'] if args.selection_only else
            [] if args.matrix else ['GUI selection mode and GUI refusals/failures','500x500 GUI'])+[
            'other GUI refusals (live/busy/no-client/multi-floor) and allocation-failure UI','sprite-rendered preview',
            'responsive cancellation during synchronous generation','whole-app RSS and concurrent jobs',
            'generator GUI save/reopen OTBM and companion round-trip','other DPI settings and platforms']}
if args.resources:
    result['resource_samples']=resources
    result['not_verified'].remove('whole-app RSS and concurrent jobs')
if args.cancel_preview:
    result['result']='PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY'
    result['historical_core_evidence']='Earlier binary; synchronous core probes were not rerun'
    result['not_verified']=['Other cancellation paths and stale targets','Place/Undo/Redo on this binary',
                            'Dense-map and leak testing','Full G1 and OTBM companion round-trip']
if args.protected_selection:
    result.update(result='PASS_G1_GUI_PROTECTED_SELECTION_ONLY',protected_uid=32101,protection_markers=protection,
                  matrix_ui_changes=ui_changes,
                  historical_core_evidence='Earlier binary; synchronous core probes were not rerun',
                  not_verified=['500x500 GUI and history/input refusal matrix on this staged Preview revision',
                                'Protected items other than one ground UID, other selection shapes and floors',
                                'OTBM companion round-trip, dense maps, leak testing and full G1'])
if args.refusals:
    result['result']=('PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING' if partial_focus_warning else
                      'PASS_G1_GUI_PREVIEW_REFUSALS_ONLY')
    if partial_focus_warning:
        result['runtime_stderr']=stderr
    result['historical_core_evidence']='Earlier binary; synchronous core probes were not rerun'
    result['not_verified']=['Real live connections, other allocation sites and input fixtures',
                            'Place/Undo/Redo and OTBM companion round-trip']
if args.invalidation:
    expected=['INVALIDATION_'+name for name in ('FIXTURE_READY','EDIT_IN_FLIGHT','EDIT_PASS','EDIT_RETRY_PASS',
        'CLOSE_READY','CLOSE_IN_FLIGHT','CLOSE_PASS','REOPEN_PASS','PASS')]
    report=(args.run/'run/dialog-probe.log').read_text(encoding='utf-8')
    require(report==run['report'] and report.splitlines()==expected,'Invalidation sequence incomplete')
    result.update(result=('PARTIAL_G1_GUI_PREVIEW_INVALIDATION_FOCUS_WARNING' if partial_focus_warning else
                          'PASS_G1_GUI_PREVIEW_INVALIDATION_ONLY'),
                  historical_core_evidence='Earlier binary; synchronous core probes were not rerun',
                  not_verified=['Other invalidation triggers, dense maps, leak testing and full G1'])
    if partial_focus_warning:
        result['runtime_stderr']=stderr
if args.roundtrip_timing:
    side=run.get('roundtrip_side')
    require(side in (100,500) and (side==500)==(args.run/'run/.g1-dialog-roundtrip-500').is_file() and
            (args.run/'run/.g1-dialog-roundtrip-timing').is_file(),'Timing side disagrees with run markers')
    require(run['scope'].startswith(f'Native {side}x{side} Place/Undo/Redo/Save timing:'),'Timing scope missing')
    report=(args.run/'run/dialog-probe.log').read_text(encoding='utf-8')
    require(report==run['report'] and report.endswith('\n'),'Timing report differs from runner log')
    csv_path=args.run/'run/roundtrip-timing.csv'
    require(sha(csv_path)==run.get('timing_csv_sha256'),'Timing CSV changed after run')
    with csv_path.open(newline='') as stream:
        rows=list(csv.reader(stream))
    header=('order,operation,side,elapsed_observed_ms,observer_tick_ms,max_heartbeat_gap_ms,baseline_ws,peak_ws,end_ws,'
            'baseline_private,peak_private,end_private,samples,max_sample_gap_ms').split(',')
    require(len(rows)==5 and rows[0]==header,'Timing CSV columns or rows incomplete')
    operations=('place','undo','redo','save')
    passes=('PLACE_PASS','UNDO_PASS','REDO_PASS','SAVED')
    expected=['ROUNDTRIP_TIMING_QUANTIZATION\tobserver_tick_ms=20\tsampler_ms=5\telapsed ends at the first observer tick '
              'that sees the history/dirty transition','ROUNDTRIP_FIXTURE_READY']
    timing=[]
    for order,(operation,row) in enumerate(zip(operations,rows[1:]),1):
        require(len(row)==14 and row[:3]==[str(order),operation,str(side)] and row[4]=='20','Unexpected timing row: '+operation)
        values=dict(zip(header,row))
        # Unsigned decimal only: rejects signs, nan and inf before conversion.
        require(all(re.fullmatch(r'\d+(\.\d+)?([eE][+-]?\d+)?',values[name]) for name in header[3:]),
                'Malformed or negative timing metric: '+operation)
        require(all(re.fullmatch(r'\d+',values[name]) for name in header[6:13]),'Non-integral samples or memory: '+operation)
        metrics={name:float(values[name]) for name in header[3:]}
        require(all(math.isfinite(value) for value in metrics.values()),'Non-finite timing metric: '+operation)
        require(metrics['elapsed_observed_ms']>0 and metrics['samples']>0,'Missing elapsed time or samples: '+operation)
        for suffix in ('ws','private'):
            first,peak,end=(metrics[prefix+suffix] for prefix in ('baseline_','peak_','end_'))
            require(min(first,end)>0 and peak>=max(first,end),'Invalid memory counters: '+operation)
        expected += [f'ROUNDTRIP_TIMING_ROW\t{order}\t{operation}\t{values["elapsed_observed_ms"]}',
                     'ROUNDTRIP_'+passes[order-1]]
        timing.append(values)
    expected.append('ROUNDTRIP_TIMING_PASS')
    require(report.splitlines()==expected,'Timing markers incomplete, out of order or disagree with CSV')
    result.update(result=('PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING' if partial_focus_warning else
                          'PASS_G1_GUI_ROUNDTRIP_TIMING_ONLY'),
                  historical_core_evidence='Earlier binary; synchronous core probes were not rerun',
                  not_verified=['Terrain payload comparison, close/reopen and OTBM companion round-trip',
                                'Timing resolution below the 20 ms observer tick, other sizes, dense maps and full G1'],
                  side=side,timing_csv_sha256=run['timing_csv_sha256'],timing_rows=timing)
    if archived:
        result.update(archived_harness_root=str(args.archived_harness_root),archived_harness_sha256=archived)
    if partial_focus_warning:
        result['runtime_stderr']=stderr
if args.roundtrip and not args.roundtrip_timing:
    from g0_container_fixture import parse
    from g0_compare_roundtrip import read_tiles, xml_value

    before_dir=args.run/'run/baseline'
    after_dir=args.run/'run/generated'
    before_magic,before=parse((before_dir/'terrain.otbm').read_bytes(),otbm_version=5)
    after_magic,after=parse((after_dir/'terrain.otbm').read_bytes(),otbm_version=5)
    require(before_magic==after_magic and before[0]==after[0] and struct.unpack_from('<I',after[0],1)[0]==4,
            'OTBM 5 header changed')
    before_map,=before[1]
    after_map,=after[1]
    require(before_map[0]==after_map[0] and after_map[0][0]==2,'Map properties changed')
    metadata=lambda node:[child for child in node[1] if child[0][0]!=4]
    require(metadata(before_map)==metadata(after_map),'Binary map metadata changed')
    old_tiles,new_tiles=read_tiles(before),read_tiles(after)
    side=run.get('roundtrip_side',100)
    require(side in (100,500) and (side==500)==(args.run/'run/.g1-dialog-roundtrip-500').is_file(),
            'Unexpected round-trip size')
    origin=side+200
    fixture={(origin+offset,origin,8 if offset==30 else 7) for offset in (0,10,20,30,40)}
    generated={(x,y,7) for x in range(100,100+side) for y in range(100,100+side)}
    require(old_tiles.keys()==fixture and new_tiles.keys()==fixture|generated,'Unexpected serialized tile positions')
    require(all(old_tiles[pos]==new_tiles[pos] for pos in fixture),'Fixture tile nodes changed')
    require(any(child[0][0]==19 for child in new_tiles[(origin+30,origin,8)][1]),'Missing binary zone membership')
    require(any(child[0][0]==15 and child[1] for child in metadata(after_map)),'Missing binary waypoint')
    companion_hashes={}
    for suffix in ('house','monster','npc','waypoint','zones'):
        name=f'terrain-{suffix}.xml'
        first=ET.parse(before_dir/name).getroot()
        second=ET.parse(after_dir/name).getroot()
        require(len(first)>0 and xml_value(first)==xml_value(second),'Companion changed or is empty: '+name)
        companion_hashes[name]=sha(after_dir/name)
    result.update(result=('PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING' if partial_focus_warning else
                          'PASS_G1_GUI_OTBM5_ROUNDTRIP_ONLY'),
                  historical_core_evidence='Earlier binary; synchronous core probes were not rerun',
                  not_verified=['Other OTBM versions, external readers and full G1'],
                  side=side,generated_tiles=len(generated), protected_tiles=len(fixture),
                  saved_otbm_sha256=sha(after_dir/'terrain.otbm'), companion_sha256=companion_hashes)
    if partial_focus_warning:
        result['runtime_stderr']=stderr
if ignored_external:
    result['ignored_external_stderr']=ignored_external
(args.run/'current-verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
