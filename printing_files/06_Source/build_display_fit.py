"""SolidCraft native parametric fit-test. Units mm. NOT A RELEASE DESIGN.
Run with Python 3; pass path to solidcraft-cli.exe as the first argument.
Every UNVERIFIED value is arbitrary demonstration geometry, NOT a module estimate.
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent
PARAMETERS = {
    'PCB_L': (85, 'User supplied PCB envelope only; physical verification pending'),
    'PCB_W': (48, 'User supplied PCB envelope only; never used for viewing opening'),
    'UNVERIFIED_PCB_T': (1.6, 'DEMO ONLY: measure PCB thickness'),
    'UNVERIFIED_ACTIVE_W': (50, 'DEMO ONLY: no aspect ratio or diagonal inference'),
    'UNVERIFIED_ACTIVE_H': (32, 'DEMO ONLY: measure active pixel bounds'),
    'UNVERIFIED_ACTIVE_X': (0, 'DEMO ONLY: active centre relative to PCB centre'),
    'UNVERIFIED_ACTIVE_Y': (0, 'DEMO ONLY: active centre relative to PCB centre'),
    'UNVERIFIED_GLASS_W': (60, 'DEMO ONLY: measure complete LCD glass'),
    'UNVERIFIED_GLASS_H': (42, 'DEMO ONLY: measure complete LCD glass'),
    'UNVERIFIED_GLASS_T': (2, 'DEMO ONLY: measure glass/touch stack'),
    'UNVERIFIED_GLASS_X': (0, 'DEMO ONLY: glass centre relative to PCB centre'),
    'UNVERIFIED_GLASS_Y': (0, 'DEMO ONLY: glass centre relative to PCB centre'),
    'UNVERIFIED_TOUCH_W': (58, 'DEMO ONLY: touch sensitive/interaction keepout width'),
    'UNVERIFIED_TOUCH_H': (40, 'DEMO ONLY: touch sensitive/interaction keepout height'),
    'UNVERIFIED_TOUCH_DX': (0, 'DEMO ONLY: touch centre offset from active screen'),
    'UNVERIFIED_TOUCH_DY': (0, 'DEMO ONLY: touch centre offset from active screen'),
    'PIXEL_CLEARANCE': (1, 'Design allowance per side; validate physically'),
    'TOUCH_CLEARANCE': (1, 'Design allowance per side outside touch keepout'),
    'BEZEL_W': (100, 'Fit-test outer width, independent from PCB'),
    'BEZEL_H': (70, 'Fit-test outer height, independent from PCB'),
    'BEZEL_T': (4, 'Fit-test bezel thickness'),
    'UNVERIFIED_WINDOW_W': (66, 'Independent DEMO protective-window width'),
    'UNVERIFIED_WINDOW_H': (48, 'Independent DEMO protective-window height'),
    'UNVERIFIED_WINDOW_T': (0.8, 'DEMO ONLY; resistive touch compatibility NOT verified'),
    'WINDOW_FIT': (0.3, 'Pocket clearance per side; tune to process'),
    'RECESS_DEPTH': (1.6, 'Stationary perimeter pocket depth'),
    'GASKET_EDGE_INSET': (0.2, 'Gasket outer edge inset from window'),
    'GASKET_WIDTH': (1.5, 'Continuous ring width; validate seal compression'),
    'UNVERIFIED_GASKET_T': (0.6, 'DEMO ONLY: compressed gasket height not specified'),
    'UNVERIFIED_PCB_BACK_Z': (-8, 'DEMO ONLY: PCB rear Z, bezel rear face Z=0'),
    'UNVERIFIED_GLASS_BACK_Z': (-3, 'DEMO ONLY: glass rear Z; measure stack datum'),
    'REFERENCE_SKIN_T': (0.1, 'Visualization thickness only, not physical LCD stack'),
    'EPS': (0.05, 'Boolean through-cut extension'),
}
DERIVED = {
    'OPEN_W': 'max(UNVERIFIED_ACTIVE_W+2*PIXEL_CLEARANCE,UNVERIFIED_TOUCH_W+2*TOUCH_CLEARANCE+2*abs(UNVERIFIED_TOUCH_DX))',
    'OPEN_H': 'max(UNVERIFIED_ACTIVE_H+2*PIXEL_CLEARANCE,UNVERIFIED_TOUCH_H+2*TOUCH_CLEARANCE+2*abs(UNVERIFIED_TOUCH_DY))',
    'POCKET_W': 'UNVERIFIED_WINDOW_W+2*WINDOW_FIT',
    'POCKET_H': 'UNVERIFIED_WINDOW_H+2*WINDOW_FIT',
    'GASKET_OUT_W': 'UNVERIFIED_WINDOW_W-2*GASKET_EDGE_INSET',
    'GASKET_OUT_H': 'UNVERIFIED_WINDOW_H-2*GASKET_EDGE_INSET',
    'GASKET_IN_W': 'GASKET_OUT_W-2*GASKET_WIDTH',
    'GASKET_IN_H': 'GASKET_OUT_H-2*GASKET_WIDTH',
    'SEAL_Z': 'BEZEL_T-RECESS_DEPTH',
}

def cmd(command, **params):
    return {'command': command, 'params': params}

def box(name, w, h, t, x='UNVERIFIED_ACTIVE_X', y='UNVERIFIED_ACTIVE_Y', z='0', operation='new', target=None):
    params = dict(length=w, width=h, height=t,
                  center=[x, y, f'({z})+({t})/2'],
                  operation=operation, name=name, body_name=name)
    if target:
        params['targets'] = [target]
    return cmd('solid.box', **params)

def make_commands(assembly=False):
    commands = [cmd('parameters.change', name=k, expression=f'{v} mm', comment=c)
                for k, (v, c) in PARAMETERS.items()]
    commands += [cmd('parameters.change', name=k, expression=v,
                     comment='Derived; UNVERIFIED until upstream dimensions are verified')
                 for k, v in DERIVED.items()]
    commands.append(cmd('document.metadata.set', values={
        'project': 'Separate display PCB / active area / bezel fit-test',
        'revision': 'P0-UNVERIFIED-NOT-FOR-FINAL-ENCLOSURE',
        'description': 'PCB 85x48 user supplied. Every other module dimension is UNVERIFIED DEMO ONLY. Measure actual module, print bezel, validate visibility and touch before release. No mounting holes or retention fabricated without evidence.',
        'org.codex.display_fit': {
            'release_allowed': False,
            'manufacturer': None, 'module_model': None,
            'mounting_holes_mm': None, 'connector_position_mm': None,
            'total_thickness_mm': None, 'black_border_bounds_mm': None,
            'touch_controller_keepout_mm': None,
            'rear_retention': 'PENDING: support PCB at verified mounting locations; never clamp glass',
            'protective_window_touch_test': 'NOT PERFORMED; optional window is a reference only',
            'gasket_mating_surface': 'stationary bezel pocket floor, outside touch keepout',
            'physical_fit_test': 'NOT PERFORMED',
        }}))
    commands += [
        box('C_BEZEL_FIT_TEST_UNVERIFIED', 'BEZEL_W', 'BEZEL_H', 'BEZEL_T'),
        box('ACTIVE_AND_TOUCH_DERIVED_OPENING', 'OPEN_W', 'OPEN_H', 'BEZEL_T+2*EPS',
            z='-EPS', operation='cut', target='C_BEZEL_FIT_TEST_UNVERIFIED'),
        box('INDEPENDENT_WINDOW_RECESS', 'POCKET_W', 'POCKET_H', 'RECESS_DEPTH+EPS',
            z='SEAL_Z', operation='cut', target='C_BEZEL_FIT_TEST_UNVERIFIED'),
        cmd('appearance.assign', bodies=['C_BEZEL_FIT_TEST_UNVERIFIED'], color='#8994a6'),
    ]
    if assembly:
        commands += [
            box('A_PCB_85x48_ENVELOPE_ONLY_T_UNVERIFIED', 'PCB_L', 'PCB_W', 'UNVERIFIED_PCB_T',
                x='0', y='0', z='UNVERIFIED_PCB_BACK_Z'),
            box('GLASS_UNVERIFIED_REFERENCE', 'UNVERIFIED_GLASS_W', 'UNVERIFIED_GLASS_H', 'UNVERIFIED_GLASS_T',
                x='UNVERIFIED_GLASS_X', y='UNVERIFIED_GLASS_Y', z='UNVERIFIED_GLASS_BACK_Z'),
            box('B_ACTIVE_LCD_UNVERIFIED_REFERENCE', 'UNVERIFIED_ACTIVE_W', 'UNVERIFIED_ACTIVE_H', 'REFERENCE_SKIN_T',
                z='UNVERIFIED_GLASS_BACK_Z+UNVERIFIED_GLASS_T'),
            box('TOUCH_REGION_UNVERIFIED_KEEP_OUT', 'UNVERIFIED_TOUCH_W', 'UNVERIFIED_TOUCH_H', 'REFERENCE_SKIN_T',
                x='UNVERIFIED_ACTIVE_X+UNVERIFIED_TOUCH_DX', y='UNVERIFIED_ACTIVE_Y+UNVERIFIED_TOUCH_DY',
                z='UNVERIFIED_GLASS_BACK_Z+UNVERIFIED_GLASS_T+REFERENCE_SKIN_T'),
            box('GASKET_UNVERIFIED_STATIONARY_SURFACE', 'GASKET_OUT_W', 'GASKET_OUT_H', 'UNVERIFIED_GASKET_T', z='SEAL_Z'),
            box('GASKET_INNER_CUT', 'GASKET_IN_W', 'GASKET_IN_H', 'UNVERIFIED_GASKET_T+2*EPS',
                z='SEAL_Z-EPS', operation='cut', target='GASKET_UNVERIFIED_STATIONARY_SURFACE'),
            box('OPTIONAL_WINDOW_UNVERIFIED_TOUCH_COMPATIBILITY', 'UNVERIFIED_WINDOW_W', 'UNVERIFIED_WINDOW_H', 'UNVERIFIED_WINDOW_T',
                z='SEAL_Z+UNVERIFIED_GASKET_T'),
        ]
        for name, color, opacity in [
            ('A_PCB_85x48_ENVELOPE_ONLY_T_UNVERIFIED','#208a65',1),
            ('GLASS_UNVERIFIED_REFERENCE','#303846',1),
            ('B_ACTIVE_LCD_UNVERIFIED_REFERENCE','#248fea',1),
            ('TOUCH_REGION_UNVERIFIED_KEEP_OUT','#edb94f',0.2),
            ('GASKET_UNVERIFIED_STATIONARY_SURFACE','#35343a',1),
            ('OPTIONAL_WINDOW_UNVERIFIED_TOUCH_COMPATIBILITY','#b2e3f0',0.18)]:
            commands.append(cmd('appearance.assign', bodies=[name], color=color, opacity=opacity))
    return commands

if __name__ == '__main__':
    for stem, assembly in [('bezel_fit_test_UNVERIFIED', False), ('display_stack_UNVERIFIED', True)]:
        script = ROOT / f'{stem}.json'
        script.write_text(json.dumps({'commands': make_commands(assembly)}, indent=2), encoding='utf-8')
        if len(sys.argv) > 1:
            args = [sys.argv[1], 'run', str(script), '--save', str(ROOT / f'{stem}.solidcraft'),
                    '--out', str(ROOT / f'{stem}.step'), '--json-out', str(ROOT / f'{stem}_measurements.json')]
            if not assembly:
                args += ['--out', str(ROOT / f'{stem}.stl')]
            subprocess.run(args, check=True)
