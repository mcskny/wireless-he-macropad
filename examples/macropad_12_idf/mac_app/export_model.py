"""Export the assembled first frame of the user's Blender model; source stays untouched."""
import bpy, os
scene=bpy.context.scene
scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT')
for obj in scene.objects:
    if obj.type != 'MESH' or obj.name == 'Plane' or obj.hide_render:
        continue
    obj.select_set(True)
for n in range(12):
    original=f'Node1.{4+4*n:03d}'
    obj=bpy.data.objects.get(original)
    if obj is None: raise RuntimeError('Keycap missing: '+original)
    channel=(n%3)*4+(3-n//3)
    obj.name=f'HE_KEY_{channel:02d}'
path=os.path.join(os.path.dirname(bpy.data.filepath),'unused')
path=os.environ['MACROPAD_MODEL_EXPORT']
bpy.ops.wm.usd_export(filepath=path, selected_objects_only=True, export_animation=False, export_materials=True)
print('EXPORT_MODEL',path)
