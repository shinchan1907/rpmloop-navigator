"""P1 depth/layout concept. mm. Run with Python to generate SolidCraft commands.
Reserve bodies are space allocations, NOT purchasable parts or printable parts.
"""
import json
from pathlib import Path
from build_display_fit import cmd, box, make_commands

ROOT=Path(__file__).resolve().parent
PARAMETERS={
 'HOUSING_DEPTH':('45 mm','Design allowance behind bezel; includes removable rear lid'),
 'HOUSING_WALL':('3 mm','ASA nominal wall; printer/process validation pending'),
 'REAR_LID_T':('3 mm','Removable rear cover concept; fastening/seal detail pending'),
 'RESERVE_BATTERY_W':('52 mm','Space allocation ONLY; includes future battery packaging allowance'),
 'RESERVE_BATTERY_H':('36 mm','Space allocation ONLY; no battery selected'),
 'RESERVE_BATTERY_T':('12 mm','Whole future battery assembly including retention/expansion allowance must fit'),
 'RESERVE_BATTERY_X':('-16 mm','Battery allocation centre relative to PCB datum'),
 'RESERVE_GPS_W':('30 mm','Whole GPS breakout/antenna allocation; exact board unverified'),
 'RESERVE_GPS_H':('30 mm','Whole GPS assembly allocation; exact board unverified'),
 'RESERVE_GPS_T':('14 mm','Includes antenna and connector height; verify chosen assembly'),
 'RESERVE_GPS_X':('28 mm','GPS allocation centre; antenna orientation/RF validation pending'),
 'RESERVE_REAR_GAP':('2 mm','Allocation clearance from rear cover, not battery expansion spec'),
 'RESERVE_ELECTRONICS_W':('80 mm','Shared space for ESP32, SD, power management and wiring; fit unverified'),
 'RESERVE_ELECTRONICS_H':('50 mm','Shared electronics space allocation'),
 'RESERVE_ELECTRONICS_T':('12 mm','Shared electronics space allocation'),
 'RESERVE_ELECTRONICS_BACK_Z':('-24 mm','Electronics allocation back datum'),
 'INNER_W':('BEZEL_W-2*HOUSING_WALL','Derived internal width'),
 'INNER_H':('BEZEL_H-2*HOUSING_WALL','Derived internal height'),
 'USABLE_REAR_DEPTH':('HOUSING_DEPTH-REAR_LID_T','Derived clear depth behind bezel'),
 'TOTAL_DEVICE_DEPTH':('HOUSING_DEPTH+BEZEL_T','Derived assembled overall depth excluding mount'),
 'RESERVE_BACK_Z':('-HOUSING_DEPTH+REAR_LID_T+RESERVE_REAR_GAP','Allocation back datum follows housing depth'),
}

def additions():
 c=[cmd('parameters.change',name=k,expression=v,comment=note) for k,(v,note) in PARAMETERS.items()]
 c += [
  box('DEEP_HOUSING_P1_CONCEPT','BEZEL_W','BEZEL_H','USABLE_REAR_DEPTH',z='-USABLE_REAR_DEPTH'),
  box('INTERNAL_CAVITY','INNER_W','INNER_H','USABLE_REAR_DEPTH+2*EPS',z='-USABLE_REAR_DEPTH-EPS',operation='cut',target='DEEP_HOUSING_P1_CONCEPT'),
  box('REMOVABLE_REAR_COVER_P1_CONCEPT','BEZEL_W','BEZEL_H','REAR_LID_T',z='-HOUSING_DEPTH'),
  box('RESERVE_ONLY_BATTERY_NOT_SELECTED','RESERVE_BATTERY_W','RESERVE_BATTERY_H','RESERVE_BATTERY_T',x='RESERVE_BATTERY_X',y='0',z='RESERVE_BACK_Z'),
  box('RESERVE_ONLY_M8M_GPS_ASSEMBLY_UNVERIFIED','RESERVE_GPS_W','RESERVE_GPS_H','RESERVE_GPS_T',x='RESERVE_GPS_X',y='0',z='RESERVE_BACK_Z'),
  box('RESERVE_ONLY_ELECTRONICS_POWER_AND_WIRING','RESERVE_ELECTRONICS_W','RESERVE_ELECTRONICS_H','RESERVE_ELECTRONICS_T',x='0',y='0',z='RESERVE_ELECTRONICS_BACK_Z'),
 ]
 for name,color,opacity in [
  ('DEEP_HOUSING_P1_CONCEPT','#25282c',0.28),
  ('REMOVABLE_REAR_COVER_P1_CONCEPT','#25282c',1),
  ('C_BEZEL_FIT_TEST_UNVERIFIED','#25282c',1),
  ('RESERVE_ONLY_BATTERY_NOT_SELECTED','#e58d43',0.65),
  ('RESERVE_ONLY_M8M_GPS_ASSEMBLY_UNVERIFIED','#3db1d4',0.65),
  ('RESERVE_ONLY_ELECTRONICS_POWER_AND_WIRING','#9b77d4',0.3),
 ]: c.append(cmd('appearance.assign',bodies=[name],color=color,opacity=opacity))
 c.append(cmd('document.metadata.set',values={
  'revision':'P1-DEEP-HOUSING-LAYOUT-CONCEPT-NOT-FOR-FINAL-PRINT',
  'description':'100x70x49 mm concept including 4 mm bezel. Rear depth 45 mm. 94x64x42 mm gross internal cavity. Removable rear cover requires fastening and sealing design. Battery/GPS/electronics bodies are reserved volumes only. Active screen dimensions remain unverified.',
  'org.codex.expansion':{
   'gps_requested':'M8M; NEO-M8M interpretation; exact breakout and antenna unknown',
   'battery_selected':False,'reserve_battery_mm':[52,36,12],
   'reserve_gps_assembly_mm':[30,30,14],
   'reserve_electronics_mm':[80,50,12],
   'future_installation':'Remove service cover; use replaceable internal carrier matched to selected parts',
   'next_features':'Rear-cover fastening, silicone seal, internal replaceable carrier, USB entry, reinforced mount and corner finishing require component/interface evidence and review',
   'release_allowed':False,'physical_validation':'NOT PERFORMED',
   'rf_validation':'Antenna placement/sky view through enclosure, display and battery must be tested',
   'battery_validation':'Selected battery assembly must fit allocation including protection, restraint and vendor clearances; temperature/charging conditions unverified',
   'gps_source':'https://content.u-blox.com/sites/default/files/products/documents/NEO-M8_ProductSummary_UBX-16000345.pdf',
  }}))
 return c

if __name__=='__main__':
 for name,commands in [('deep_housing_additions',additions()),('deep_dashboard_complete',make_commands(True)+additions())]:
  (ROOT/f'{name}.json').write_text(json.dumps({'commands':commands},indent=2),encoding='utf-8')
