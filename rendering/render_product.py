import bpy,math,json,struct,sys
from pathlib import Path
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
PKG=ROOT/'printing_files'
OUT=ROOT/'docs'/'images'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.render.engine='CYCLES'
scene.cycles.samples=48
scene.cycles.use_denoising=True
scene.cycles.adaptive_threshold=.045
scene.cycles.max_bounces=6
try:
 prefs=bpy.context.preferences.addons['cycles'].preferences
 prefs.compute_device_type='OPTIX';prefs.get_devices()
 gpu=[d for d in prefs.devices if d.type!='CPU']
 if gpu:
  for d in prefs.devices:d.use=d.type!='CPU'
  scene.cycles.device='GPU'
except Exception:pass
scene.render.resolution_x=1600;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'
scene.view_settings.exposure=-1.0
scene.world=bpy.data.worlds.new('Studio environment');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.025,.032,.045,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.24

def mat(name,color,rough=.5,metal=0,emission=0):
 m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF')
 p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 if emission:p.inputs['Emission Color'].default_value=(*color,1);p.inputs['Emission Strength'].default_value=emission
 return m
asa=mat('Matte black ASA',(.009,.010,.012),.53)
n=asa.node_tree.nodes;l=asa.node_tree.links
noise=n.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=280;noise.inputs['Detail'].default_value=2
bump=n.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.13;bump.inputs['Distance'].default_value=.000025
l.new(noise.outputs['Fac'],bump.inputs['Height']);l.new(bump.outputs['Normal'],n.get('Principled BSDF').inputs['Normal'])
rubber=mat('Black silicone',(.009,.011,.013),.85)
metal=mat('Stainless screw heads',(.24,.27,.31),.28,.92)
lcd=mat('LCD black',(.002,.007,.011),.3)
white=mat('Display white',(.75,.94,1),.4,emission=2)
cyan=mat('Route cyan',(.025,.72,.84),.4,emission=2)
muted=mat('Muted display ink',(.08,.19,.22),.6,emission=.5)
glass=mat('Clear protective-window concept',(.92,.98,1),.08)
pg=glass.node_tree.nodes.get('Principled BSDF');pg.inputs['Transmission Weight'].default_value=1;pg.inputs['IOR'].default_value=1.46
root=bpy.data.objects.new('RPMLOOP product',None);scene.collection.objects.link(root)
root.rotation_euler=(math.radians(72),0,math.radians(-7))
parts={};groups={};checks=json.loads((PKG/'05_Verification'/'mesh_checks.json').read_text())

def parent(o,material,group='structure'):
 o.parent=root;o.data.materials.append(material);groups.setdefault(group,[]).append(o);return o
def import_part(stem,folder,qty,material=asa):
 data=(PKG/folder/f'{stem}_qty{qty}.stl').read_bytes();num=struct.unpack_from('<I',data,80)[0]
 verts=[];faces=[];idx={}
 for i in range(num):
  v=struct.unpack_from('<12fH',data,84+i*50);face=[]
  for j in [3,6,9]:
   point=tuple(round(x*.001,9) for x in v[j:j+3])
   if point not in idx:idx[point]=len(verts);verts.append(point)
   face.append(idx[point])
  faces.append(face)
 mesh=bpy.data.meshes.new(stem);mesh.from_pydata(verts,[],faces);mesh.update()
 obj=bpy.data.objects.new(stem,mesh);scene.collection.objects.link(obj)
 obj.location=tuple(-x*.001 for x in checks[stem]['translation_mm'])
 parent(obj,material)
 for poly in mesh.polygons:poly.use_smooth=True
 if hasattr(mesh,'set_sharp_from_angle'):mesh.set_sharp_from_angle(angle=math.radians(36))
 bevel=obj.modifiers.new('Render-only edge highlights','BEVEL');bevel.width=.00008;bevel.segments=2;bevel.limit_method='ANGLE'
 normal=obj.modifiers.new('Weighted normals','WEIGHTED_NORMAL');normal.keep_sharp=True
 parts[stem]=obj;return obj
for stem in ['housing','front_bezel','rear_lid','esp32_cradle']:
 import_part(stem,'01_Rigid_STL',1)
for stem in ['PCB_tab_left','PCB_tab_right']:
 obj=import_part(stem,'01_Rigid_STL',2)
 twin=obj.copy();twin.data=obj.data.copy();scene.collection.objects.link(twin);twin.location.y+=.020;twin.parent=root

def box(name,dim,location,material,group='display',bevel=0):
 bpy.ops.mesh.primitive_cube_add(size=1,location=tuple(v*.001 for v in location));o=bpy.context.object;o.name=name
 o.dimensions=tuple(v*.001 for v in dim);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 parent(o,material,group)
 if bevel:
  m=o.modifiers.new('Rounded edge','BEVEL');m.width=bevel*.001;m.segments=5
 return o
box('LCD glass reference',(60,42,2),(0,0,-2),lcd,bevel=.4)
box('Active display concept',(50,32,.08),(0,0,-.9),lcd)
window=box('Optional touch-compatible window concept',(66,48,.8),(0,0,3.4),glass,bevel=.3)

def text(name,content,x,y,size,material=white):
 curve=bpy.data.curves.new(name,'FONT');curve.body=content;curve.size=size*.001;curve.extrude=0;curve.align_x='LEFT'
 obj=bpy.data.objects.new(name,curve);scene.collection.objects.link(obj);obj.location=(x*.001,y*.001,-.00083)
 parent(obj,material,'display');return obj
def line(name,points,width,material=cyan,z=-.82):
 curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=2;curve.bevel_depth=width*.0005;curve.bevel_resolution=3
 spline=curve.splines.new('POLY');spline.points.add(len(points)-1)
 for p,(x,y) in zip(spline.points,points):p.co=(x*.001,y*.001,z*.001,1)
 obj=bpy.data.objects.new(name,curve);scene.collection.objects.link(obj);parent(obj,material,'display');return obj
text('Top label','NEXT TURN',-22,11,2.1,cyan)
text('Distance','250 m',-8,2.2,5.5)
text('Instruction','TURN LEFT',-8,-2.2,2.5)
text('Speed','48',-22,-12.5,5)
text('Units','km/h',-14,-12,1.8,muted)
text('ETA','ETA  10:42',3,-11.5,2.1)
line('Left turn stem',[(-17,-1),(-17,5),(-22,5)],1.1)
line('Left arrowhead',[(-19,8),(-22,5),(-19,2)],1.1)
line('UI divider',[(-22,-6),(22,-6)],.15,muted)
line('Progress indicator',[(-22,14),(22,14)],.15,muted)
line('Progress accent',[(-22,14),(4,14)],.35,cyan)

for x in [-43,43]:
 for y in [-29,29]:
  bpy.ops.mesh.primitive_cylinder_add(vertices=48,radius=.0019,depth=.0015,location=(x*.001,y*.001,.0032))
  obj=bpy.context.object;obj.name='Illustrative M2 screw';parent(obj,metal,'front_hardware')
  bevel=obj.modifiers.new('Head bevel','BEVEL');bevel.width=.00015;bevel.segments=3
  bpy.ops.mesh.primitive_cylinder_add(vertices=6,radius=.00075,depth=.0003,location=(x*.001,y*.001,.00396))
  slot=bpy.context.object;slot.name='Hex socket visual';parent(slot,lcd,'front_hardware')

# Place the actual CAD geometry just over the studio floor.
bpy.context.view_layer.update()
zmin=min((o.matrix_world@Vector(c)).z for o in root.children if o.type=='MESH' for c in o.bound_box)
root.location.z=-zmin+.001
floor=mat('Studio charcoal',(.012,.015,.020),.72)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,0));ground=bpy.context.object;ground.name='Studio floor';ground.data.materials.append(floor)
def aim(obj,point):obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()
def area(name,loc,energy,size,color):
 data=bpy.data.lights.new(name,'AREA');data.energy=energy;data.shape='DISK';data.size=size;data.color=color
 obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=loc;aim(obj,(0,0,.055))
area('Large soft key',(-.16,-.20,.29),3,.22,(.86,.93,1))
area('Cool rim',(.18,.13,.22),5,.16,(.48,.72,1))
area('Warm edge',(-.16,.08,.14),2,.12,(1,.72,.48))
area('Soft front fill',(.10,-.25,.10),.6,.20,(.8,.9,1))
area('Low connector detail fill',(-.10,-.16,.020),.8,.12,(.7,.85,1))
camdata=bpy.data.cameras.new('Product camera');camera=bpy.data.objects.new('Product camera',camdata);scene.collection.objects.link(camera);scene.camera=camera
camera.data.lens=58;camera.data.clip_start=.001;camera.data.clip_end=200
def render(name,location,target,lens=58):
 camera.location=location;camera.data.lens=lens;aim(camera,target)
 scene.render.filepath=str(OUT/name);bpy.ops.render.render(write_still=True)

bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'rendering'/'rpmloop_studio.blend'))
detail_only='--detail-only' in sys.argv
if not detail_only:render('hero.png',(.17,-.235,.17),(0,.008,.065),60)
render('port-and-mount.png',(-.13,-.24,.012),(-.01,-.005,.032),65)
# Exploded product view; display artwork remains illustrative.
parts['front_bezel'].location.z+=.018
parts['rear_lid'].location.z-=.018
parts['esp32_cradle'].location.z-=.006
for obj in groups.get('display',[]):obj.location.z+=.009
for obj in groups.get('front_hardware',[]):obj.location.z+=.018
if not detail_only:render('exploded.png',(.21,-.29,.20),(0,.01,.07),52)
print('RPMLOOP_RENDER_COMPLETE')
