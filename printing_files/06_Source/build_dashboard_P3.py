"""P3 detailed ASA enclosure. Units mm. Interface values labelled UNVERIFIED
require physical fit tests. This script generates a native SolidCraft timeline.
"""
import json
from pathlib import Path
from build_display_fit import PARAMETERS as FRONT, DERIVED, cmd, box
from build_deep_housing import PARAMETERS as DEPTH
ROOT=Path(__file__).resolve().parent
C=[]

def add(command_id,**params): C.append({'command':command_id,'params':params})
def par(name,value,comment): add('parameters.change',name=name,expression=value,comment=comment)
def rr_loop(w,h,r,x=0,y=0,width=None,height=None):
    add('sketch.draw.dimensioned',command='sketch.rectangle.rounded',
        params={'p0':[x-w/2,y-h/2],'p1':[x+w/2,y+h/2],'radius':r},
        dimensions={'width':width or w,'height':height or h})
def rr(name,w,h,r,z,t,op='new',target=None,x=0,y=0,inner=None,plane='XY',width=None,height=None):
    add('sketch.create',plane=plane,offset=z,name=name+'_profile')
    rr_loop(w,h,r,x,y,width,height)
    if inner: rr_loop(*inner[:3],x=x,y=y)
    add('sketch.finish')
    p=dict(distance=t,operation=op,name=name,body_name=name)
    if inner:p['profiles']=[{'point':[x+w/2-(w-inner[0])/4,y]}]
    if target:p['targets']=[target]
    add('solid.extrude',**p)
def b(name,w,h,t,x=0,y=0,z=0,op='new',target=None):
    C.append(box(name,w,h,t,str(x),str(y),str(z),op,target))
def cyl(name,d,t,base,axis='Z',op='new',target=None):
    p=dict(diameter=d,height=t,base=base,axis=axis,operation=op,name=name,body_name=name)
    if target:p['targets']=[target]
    add('solid.cylinder',**p)
def join(target,tools):add('solid.combine',target=target,tools=tools,operation='join')
def fillet(body,points,r,name):
    add('solid.fillet',edges=[{'body':body,'point':p} for p in points],radius=r,name=name)

def build():
    C.clear()
    for k,(v,note) in FRONT.items():par(k,f'{v} mm',note)
    for k,v in DERIVED.items():par(k,v,'Derived from UNVERIFIED display/touch geometry')
    for k,(v,note) in DEPTH.items():par(k,v,note)
    params={
      'OUTER_CORNER_R':('6 mm','Exterior rounded corners'),
      'INSERT_LENGTH':('4 mm','User specified M2 x 4 mm insert length'),
      'UNVERIFIED_INSERT_PILOT_D':('2.8 mm','PLACEHOLDER; select from insert manufacturer and printed coupon'),
      'INSERT_POCKET_DEPTH':('4.4 mm','Blind pilot depth including installation allowance'),
      'UNVERIFIED_INSERT_OD':('3.2 mm','Reference sleeve only; actual knurl outside diameter unknown'),
      'SCREW_CLEAR_D':('2.4 mm','M2 printed clearance target; process verification required'),
      'SCREW_HEAD_RECESS_D':('5.2 mm','Allowance for M2 head and separate sealing washer'),
      'SCREW_HEAD_RECESS_DEPTH':('2 mm','1 mm lid floor remains; use suitable M2 head length/type'),
      'SCREW_X':('43 mm','Enclosure fastener centres; not display mounting holes'),
      'SCREW_Y':('29 mm','Enclosure fastener centres; avoids display PCB outline'),
      'JOINT_GROOVE_DEPTH':('1.15 mm','23.3 percent compression target for 1.5 mm gasket'),
      'JOINT_CORD_D':('1.5 mm','Trial gasket cord diameter; verify supplier and closure load'),
      'JOINT_GROOVE_W':('2.1 mm','Widened gland for elastomer volume; seal performance untested'),
      'ESP32_BOARD_W':('18 mm','User-provided board drawing outline'),
      'ESP32_BOARD_L':('23.5 mm','User-provided board drawing PCB outline; excludes USB projection'),
      'UNVERIFIED_ESP32_T':('1 mm','Measure actual PCB thickness and underside components'),
      'ESP32_SLIDE_CLEAR':('0.3 mm','Per-side guide allowance; verify solder/wire clearance'),
      'UNVERIFIED_PORT_W':('10.5 mm','Direct-board USB aperture placeholder'),
      'UNVERIFIED_PORT_H':('4.2 mm','Direct-board USB aperture placeholder'),
      'UNVERIFIED_USB_Z':('-19.2 mm','USB socket centre from unverified stack height'),
      'UNVERIFIED_FORK_T':('3 mm','Action-camera finger thickness; measure actual mating mount'),
      'UNVERIFIED_FORK_GAP':('3.3 mm','Action-camera mating clearance; test coupon before road use'),
      'MOUNT_BORE_D':('5.3 mm','M5 pivot clearance target'),
      'UNVERIFIED_M5_NUT_AF':('8.2 mm','Captive nut across-flats clearance; verify hardware'),
      'BRAND_CUT_DEPTH':('0.5 mm','Recessed Instagram-style glyph and rpmloop lettering'),
    }
    for k,(v,note) in params.items():par(k,v,note)
    shell='HOUSING_P3';bezel='FRONT_BEZEL_P3';lid='REAR_LID_P3'
    rr(shell,100,70,'OUTER_CORNER_R',-42,'USABLE_REAR_DEPTH',width='BEZEL_W',height='BEZEL_H')
    rr('Hollow_interior',94,64,3,-42.05,42.1,'cut',shell,width='INNER_W',height='INNER_H')
    for end,z in [('FRONT',-2.5),('REAR',-42)]:
      land=end+'_wide_seal_land'
      rr(land,97,67,4.5,z,2.5,inner=(91,61,1.5))
      join(shell,[land])
    rr(bezel,100,70,'OUTER_CORNER_R',0,'BEZEL_T',width='BEZEL_W',height='BEZEL_H')
    fillet(bezel,[[0,35,4]],1,'Soft_front_rim_R1')
    rr('Pixel_and_touch_opening',60,42,0.8,-0.05,4.1,'cut',bezel,width='OPEN_W',height='OPEN_H')
    rr('Independent_window_rebate',66.6,48.6,2,2.4,1.65,'cut',bezel,width='POCKET_W',height='POCKET_H')
    rr(lid,100,70,'OUTER_CORNER_R',-45,'REAR_LID_T',width='BEZEL_W',height='BEZEL_H')
    fillet(lid,[[0,35,-45]],0.8,'Soft_rear_rim_R08')

    # Integral pads have blind insert bores. Both front and rear are serviceable.
    for suffix,z,t in [('FRONT',-9,9),('REAR',-42,9)]:
      for i,(sx,sy) in enumerate([(-1,-1),(-1,1),(1,-1),(1,1)],1):
        x=sx*43;y=sy*29
        pad=f'{suffix}_boss_{i}'
        rr(pad,8.6,8.6,2,z,t,x=x,y=y)
        join(shell,[pad])
        start='-INSERT_POCKET_DEPTH' if suffix=='FRONT' else '-USABLE_REAR_DEPTH'
        cyl(f'{suffix}_M2_insert_pilot_{i}','UNVERIFIED_INSERT_PILOT_D','INSERT_POCKET_DEPTH',
            [f'{sx}*SCREW_X',f'{sy}*SCREW_Y',start],op='cut',target=shell)
        plate=bezel if suffix=='FRONT' else lid
        holez=-0.05 if suffix=='FRONT' else -45.05
        cyl(f'{suffix}_M2_clearance_{i}','SCREW_CLEAR_D',4.1 if suffix=='FRONT' else 3.1,
            [f'{sx}*SCREW_X',f'{sy}*SCREW_Y',holez],op='cut',target=plate)
        recessz=2 if suffix=='FRONT' else -45.05
        cyl(f'{suffix}_head_and_washer_recess_{i}','SCREW_HEAD_RECESS_D',2.05,
            [x,y,recessz],op='cut',target=plate)
        # Separate silicone sealing washers under heads. Geometry is compressed placeholder.
        wz=2 if suffix=='FRONT' else -43.4
        washer=f'{suffix}_screw_sealing_washer_{i}_REFERENCE'
        cyl(washer,4.8,0.4,[x,y,wz])
        cyl(washer+'_bore',2.4,0.5,[x,y,wz-0.05],op='cut',target=washer)
    # Continuous perimeter groove in the stationary housing at both joints.
    for suffix,z in [('FRONT',-1.15),('REAR',-42)]:
      rr(suffix+'_silicone_gasket_groove',97.6,67.6,4.8,z,'JOINT_GROOVE_DEPTH','cut',shell,
         inner=(93.4,63.4,2.7))
      # Compressed flat gasket volume preserved: 1.3 x 1.5 / 1.15 = 1.69565 wide.
      rr(suffix+'_compressed_silicone_gasket_REFERENCE',97.195652,67.195652,4.597826,z,1.15,
         inner=(93.804348,63.804348,2.902174))

    # Rounded integral mount root and TWO interlocking ears, pivot axis X.
    root='Mount_reinforced_root'
    rr(root,30,9,2.5,-32,22,x=0,y=-37.5)
    join(shell,[root])
    for i in [1,2]:
      xc=f'{i-1.5}*(UNVERIFIED_FORK_T+UNVERIFIED_FORK_GAP)'
      ear=f'Action_camera_finger_{i}'
      b(ear,'UNVERIFIED_FORK_T',10.5,14,x=xc,y=-44.25,z=-28)
      cyl(ear+'_round_tip',14,'UNVERIFIED_FORK_T',[f'({xc})-UNVERIFIED_FORK_T/2',-49.5,-21],axis='X',op='join',target=ear)
      join(shell,[ear])
    cyl('Pivot_M5_through_bore','MOUNT_BORE_D',25,[-12.5,-49.5,-21],axis='X',op='cut',target=shell)
    # Two-ear device mates BETWEEN a three-ear accessory. Its outer fingers
    # need free space: the M5 nut receiver belongs to that accessory, not here.
    # Hidden nominal mating gauges are verification geometry, never printed parts.
    for i,x in enumerate([-6.3,0,6.3],1):
      mate=f'REFERENCE_MATING_FINGER_{i}_UNVERIFIED'
      b(mate,3,10.5,14,x=x,y=-54.75,z=-28)
      cyl(mate+'_tip',14,3,[x-1.5,-49.5,-21],axis='X',op='join',target=mate)
      cyl(mate+'_bore',5.3,3.1,[x-1.55,-49.5,-21],axis='X',op='cut',target=mate)
    cyl('M5_THUMB_KNOB_CLEARANCE_REFERENCE',24,6,[16,-49.5,-21],axis='X')

    # PCB support shoulders contact only the back of the PCB edges; glass is untouched.
    for side in [-1,1]:
      b(f'Display_rear_support_ledge_{side}',5.5,31,5,x=side*44.25,z=-13,op='join',target=shell)
      for yy in [-10,10]:
        # Threaded holes in the ledges accept replaceable PCB retaining tabs.
        b(f'Display_tab_standoff_{side}_{yy}',5.8,7,1.6,x=side*45.5,y=yy,z=-8,op='join',target=shell)
        cyl(f'Display_tab_insert_{side}_{yy}','UNVERIFIED_INSERT_PILOT_D',4.4,[side*45.5,yy,-10.8],op='cut',target=shell)
        tab=f'PCB_edge_retaining_tab_{side}_{yy}_UNVERIFIED'
        b(tab,7,5,2,x=side*43.5,y=yy,z=-6.4)
        cyl(tab+'_screw',2.4,2.1,[side*45.5,yy,-6.45],op='cut',target=tab)
    # ESP32 cradle: replaceable, two insert-supported fixing ears, interrupted guide lips.
    carrier='ESP32_REMOVABLE_GUIDED_CRADLE_UNVERIFIED_Z'
    rr(carrier,29,26,2,-24,1.6,x=-27,y=-18.5)
    rr('ESP32_underside_component_relief',14,16,1,-24.05,1.7,'cut',carrier,x=-27,y=-19)
    for x in [-43,-11]:
      cyl(f'Cradle_body_anchor_{x}',8,6,[x,-28.5,-30],op='join',target=shell)
      cyl(f'Cradle_anchor_M2_insert_{x}','UNVERIFIED_INSERT_PILOT_D',4.4,[x,-28.5,-28.4],op='cut',target=shell)
      cyl(f'Cradle_fixing_ear_{x}',6.5,1.6,[x,-28.5,-24],op='join',target=carrier)
      cyl(f'Cradle_M2_clearance_{x}',2.4,1.7,[x,-28.5,-24.05],op='cut',target=carrier)
    for x in [-37.3,-16.7]:
      for yy in [-8.5,-29.5]:
        b(f'ESP32_guide_lip_{x}_{yy}',2,3.5,2.2,x=x,y=yy,z=-22.4,op='join',target=carrier)
    # Groove clears board thickness, leaving upper capturing lips at the corner ends.
    b('ESP32_board_slide_groove','ESP32_BOARD_W+2*ESP32_SLIDE_CLEAR','ESP32_BOARD_L+2*ESP32_SLIDE_CLEAR',1.4,
      x=-27,y=-19,z=-21.9,op='cut',target=carrier)
    for yy,stop_t in [(-6.65,1),(-31.15,0.5)]:
      for x in [-34,-20]:
        b(f'ESP32_end_stop_{x}_{yy}',3,stop_t,1.1,x=x,y=yy,z=-22.4,op='join',target=carrier)
    b('ESP32_18x23p5_PCB_REFERENCE','ESP32_BOARD_W','ESP32_BOARD_L','UNVERIFIED_ESP32_T',x=-27,y=-19,z=-21.8)
    b('ESP32_USB_C_SOCKET_UNVERIFIED',9,7,3.2,x=-27,y=-30,z=-20.8)
    # USB-C relocated to bottom -Y, beside the two-finger mount.
    rr('USB_C_through_opening_UNVERIFIED',10.5,4.2,1.4,-36,5,'cut',shell,x=-27,y=19.2,plane='XZ',
       width='UNVERIFIED_PORT_W',height='UNVERIFIED_PORT_H')
    rr('USB_C_plug_overmould_access_UNVERIFIED',14,8,2,-35.1,2.7,'cut',shell,x=-27,y=19.2,plane='XZ')
    rr('USB_C_cap_gasket_groove_UNVERIFIED',19,13,3,-35.05,0.85,'cut',shell,x=-27,y=19.2,plane='XZ',inner=(16,10,1.5))
    b('USB_CABLE_CLEARANCE_REFERENCE_18x30x12',18,30,12,x=-27,y=-50,z=-25.2)
    # Front branding: recessed vector camera glyph and the exact requested handle.
    rr('Instagram_camera_outline_engraving',5.2,5.2,1.35,3.5,0.55,'cut',bezel,x=-23.5,y=-29.4,inner=(4.1,4.1,0.8))
    add('sketch.create',plane='XY',offset=3.5,name='Instagram_lens_profile')
    add('sketch.circle.center',center=[-23.5,-29.4],radius=1.25)
    add('sketch.circle.center',center=[-23.5,-29.4],radius=0.7)
    add('sketch.finish')
    add('solid.extrude',distance=0.55,profiles=[{'point':[-22.5,-29.4]}],operation='cut',targets=[bezel],name='Instagram_lens_engraving')
    cyl('Instagram_viewfinder_dot',0.65,0.55,[-22.05,-27.95,3.5],op='cut',target=bezel)
    add('sketch.create',plane='XY',offset='BEZEL_T-BRAND_CUT_DEPTH',name='rpmloop_lettering_profile')
    add('sketch.text',text='rpmloop',at=[-17,-31],height=4,font='C:/Windows/Fonts/arialbd.ttf')
    add('sketch.finish')
    add('solid.extrude',distance='BRAND_CUT_DEPTH+EPS',operation='cut',targets=[bezel],name='rpmloop_recessed_wordmark')
    # Large uppercase wordmark on the top OUTER wall (+Y), not over the display.
    # Local x=-X, y=+Z gives an outward +Y normal and readable lettering from outside.
    add('sketch.create',plane={'origin':[0,0,0],'x_dir':[-1,0,0],'y_dir':[0,0,1]},offset=34.4,name='TOP_RPMLOOP_profile')
    add('sketch.text',text='RPMLOOP',at=[-35,-26],height=10,font='C:/Windows/Fonts/arialbd.ttf')
    add('sketch.finish')
    add('solid.extrude',distance=0.65,operation='cut',targets=[shell],name='TOP_RPMLOOP_large_engraving')

    # Independent display/window and expansion reference bodies; never printable hardware.
    b('DISPLAY_PCB_85x48_T_UNVERIFIED','PCB_L','PCB_W','UNVERIFIED_PCB_T',z='UNVERIFIED_PCB_BACK_Z')
    b('LCD_GLASS_UNVERIFIED','UNVERIFIED_GLASS_W','UNVERIFIED_GLASS_H','UNVERIFIED_GLASS_T',z='UNVERIFIED_GLASS_BACK_Z')
    b('ACTIVE_LCD_UNVERIFIED','UNVERIFIED_ACTIVE_W','UNVERIFIED_ACTIVE_H',0.1,z=-1)
    rr('DISPLAY_WINDOW_UNVERIFIED_TOUCH',66,48,1.7,3,0.8)
    rr('DISPLAY_GASKET_UNVERIFIED',65.6,47.6,1.5,2.4,0.6,inner=(62.6,44.6,0.6))
    b('RESERVE_BATTERY_NOT_SELECTED',52,36,12,x=-16,z=-40)
    b('RESERVE_M8M_ASSEMBLY_UNVERIFIED',30,30,14,x=28,z=-40)
    # Uncompressed manufacture masters. Hidden from assembly; export separately.
    rr('MASTER_JOINT_GASKET_UNCOMPRESSED',96.8,66.8,4.4,10,1.5,inner=(94.2,64.2,3.1))
    rr('MASTER_DISPLAY_GASKET_UNVERIFIED',65.4,47.4,1.4,13,0.8,inner=(62.8,44.8,0.1))
    rr('MASTER_USB_GASKET_UNVERIFIED',18.5,12.5,2.75,16,1,inner=(16.5,10.5,1.75))
    cyl('MASTER_M2_SEALING_WASHER',4.8,0.5,[0,0,19])
    cyl('Master_washer_hole',2.4,0.6,[0,0,18.95],op='cut',target='MASTER_M2_SEALING_WASHER')
    rr('M2_INSERT_PILOT_COUPON',38,14,2,22,7)
    for x,d in [(-12,2.6),(-4,2.8),(4,3.0),(12,3.2)]:
      cyl('Coupon_pilot_'+str(d),d,4.45,[x,0,24.6],op='cut',target='M2_INSERT_PILOT_COUPON')
    # Compression washers and joint gaskets are references, not STL parts.
    for name,color,opacity in [(shell,'#24272b',1),(bezel,'#24272b',1),(lid,'#24272b',1),
       (carrier,'#657381',1),('DISPLAY_PCB_85x48_T_UNVERIFIED','#238c68',1),
       ('LCD_GLASS_UNVERIFIED','#252b34',1),('ACTIVE_LCD_UNVERIFIED','#338fc8',1),
       ('DISPLAY_WINDOW_UNVERIFIED_TOUCH','#aed9e5',0.18),('DISPLAY_GASKET_UNVERIFIED','#494949',1),
       ('RESERVE_BATTERY_NOT_SELECTED','#e59948',0.6),('RESERVE_M8M_ASSEMBLY_UNVERIFIED','#3db5ce',0.6),
       ('ESP32_18x23p5_PCB_REFERENCE','#287eac',1),('ESP32_USB_C_SOCKET_UNVERIFIED','#bfc5cf',1)]:
      add('appearance.assign',bodies=[name],color=color,opacity=opacity)
    add('browser.visibility',folder='sketches',visible=False)
    add('browser.visibility',folder='origin',visible=False)
    add('browser.visibility',items=['USB_CABLE_CLEARANCE_REFERENCE_18x30x12','MASTER_JOINT_GASKET_UNCOMPRESSED',
        'MASTER_DISPLAY_GASKET_UNVERIFIED','MASTER_USB_GASKET_UNVERIFIED','MASTER_M2_SEALING_WASHER','M2_INSERT_PILOT_COUPON',
        'REFERENCE_MATING_FINGER_1_UNVERIFIED','REFERENCE_MATING_FINGER_2_UNVERIFIED','REFERENCE_MATING_FINGER_3_UNVERIFIED',
        'M5_THUMB_KNOB_CLEARANCE_REFERENCE'],visible=False)
    add('document.metadata.set',values={
      'project':'Royal Enfield Hunter 350 / rpmloop',
      'revision':'P3-RELOCATED-USB-TWO-FINGER-FIT-VALIDATION-REQUIRED',
      'description':'P3: USB-C relocated to bottom -Y at X=-27 beside TWO-finger camera mount. ESP32 cradle rotated and resized to clear enclosure walls. Widened 2.1 x 1.15 mm joint seal glands and uncompressed gasket masters included. 49 mm case depth excluding mount. Fit prototype only; actual active display/insert/USB interfaces remain unverified.',
      'org.codex.p2':{'release_allowed':False,'insert_OD_and_pilot':'UNVERIFIED; default pilot 2.8 / reference OD 3.2',
      'esp32_outline_source':'User-supplied dimensioned image: 18 x 23.5 mm, R1 corners',
      'usb_strategy':'Bottom -Y X=-27 direct ESP32 USB-C; 18 x 30 x 12 mm cable envelope has 3 mm lateral clearance from mount root. Actual plug and Z stack unverified',
      'mount_interface':'TWO 3 mm fingers, one 3.3 mm gap; 5.3 mm M5 bore. No outer nut collar; use mating accessory nut receiver. M5 knob on +X side, opposite USB. Hidden nominal mating gauges and 24 mm knob allowance UNVERIFIED',
      'joint_seal':'1.5 mm uncompressed flat silicone ring, 1.3 mm wide, in 2.1 x 1.15 mm gland; nominal cross-section fill 80.75 percent; not IP rated',
      'pcb_support':'Rear edge ledges and replaceable retaining tabs; actual display component/edge keepout unverified',
      'remaining':'Protective USB cap, plug strain relief, insert selection, display drawing, screw length/head selection, component clearance, RF, touch, heat, rain and vibration tests',
      'native_param_note':'Sketch widths/heights and extrusion depths use parameters; layout offsets and some clearances are source-configurable fixed sketch placements. Rebuild from source for wholesale layout changes.'}})
    return list(C)

if __name__=='__main__':
    (ROOT/'hunter_dashboard_P3_commands.json').write_text(json.dumps({'commands':build()},indent=2),encoding='utf-8')
    print(len(C),'commands')

