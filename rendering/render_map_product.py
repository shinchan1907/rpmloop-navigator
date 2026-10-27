"""Render map concepts without altering the original scene or manufacturing assets.

Run in Blender's Python console or: blender --background --python render_map_product.py
LCD textures are native 320x240 exports from docs/preview at its initial ride position.
"""
import bpy
import math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs' / 'images'
SOURCE = ROOT / 'rendering' / 'rpmloop_studio.blend'
OLD_UI = ['Top label','Distance','Instruction','Speed','Units','ETA','Left turn stem',
          'Left arrowhead','UI divider','Progress indicator','Progress accent']

def prepare():
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
    scene = bpy.context.scene
    for name in OLD_UI:
        obj = bpy.data.objects.get(name)
        if obj:
            obj.hide_render = True
            obj.hide_viewport = True
    # Render-only active-area illustration: 4:3, matching 320x240 landscape.
    # Physical LCD active dimensions remain subject to module measurement.
    bpy.data.objects['Active display concept'].hide_render = True
    mesh = bpy.data.meshes.new('Map LCD 4x3 illustration')
    mesh.from_pydata([(-.025,-.01875,0),(.025,-.01875,0),(.025,.01875,0),(-.025,.01875,0)],[],[(0,1,2,3)])
    mesh.uv_layers.new(name='LCD UV')
    for loop,uv in zip(mesh.uv_layers.active.data,[(0,0),(1,0),(1,1),(0,1)]):
        loop.uv = uv
    obj = bpy.data.objects.new('Map LCD concept — 320x240',mesh)
    scene.collection.objects.link(obj)
    obj.parent = bpy.data.objects['RPMLOOP product']
    obj.location.z = -.00079
    material = bpy.data.materials.new('Map LCD emission')
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Strength'].default_value = 1.8
    image_node = nodes.new('ShaderNodeTexImage')
    image_node.interpolation = 'Closest'
    image_node.extension = 'CLIP'
    material.node_tree.links.new(image_node.outputs['Color'],emission.inputs['Color'])
    material.node_tree.links.new(emission.outputs['Emission'],output.inputs['Surface'])
    obj.data.materials.append(material)
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 64
    scene.cycles.adaptive_threshold = .035
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    return scene,image_node,obj

def artwork(node,name):
    image = bpy.data.images.load(str(ROOT/'rendering'/'map-ui'/f'lcd-{name}.png'),check_existing=True)
    image.pack()
    node.image = image

def camera(scene,location,target,lens):
    assert scene.camera is not None, 'Product scene must contain its camera'
    scene.camera.location = location
    scene.camera.data.lens = lens
    scene.camera.rotation_euler = (Vector(target)-scene.camera.location).to_track_quat('-Z','Y').to_euler()

def render(scene,name):
    assert scene.camera is not None
    scene.render.filepath = str(OUT/name)
    bpy.ops.render.render(write_still=True)

def main():
    scene,node,lcd = prepare()
    artwork(node,'day')
    camera(scene,(.17,-.235,.17),(0,.008,.065),60)
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'rendering'/'rpmloop_map_studio.blend'))
    render(scene,'hero-map-day.png')
    artwork(node,'night')
    render(scene,'hero-map-night.png')
    artwork(node,'day')
    camera(scene,(.025,-.25,.15),(0,.006,.064),65)
    render(scene,'display-map-detail.png')
    # Explode the same parts as the original render; no mesh edits.
    bpy.data.objects['front_bezel'].location.z += .018
    bpy.data.objects['rear_lid'].location.z -= .018
    bpy.data.objects['esp32_cradle'].location.z -= .006
    for name in ['LCD glass reference','Optional touch-compatible window concept']:
        bpy.data.objects[name].location.z += .009
    lcd.location.z += .009
    for obj in bpy.data.objects:
        if obj.name.startswith(('Illustrative M2 screw','Hex socket visual')):
            obj.location.z += .018
    camera(scene,(.21,-.29,.20),(0,.01,.07),52)
    render(scene,'exploded-map.png')
    print('RPMLOOP_MAP_RENDER_COMPLETE')

if __name__ == '__main__':
    main()
