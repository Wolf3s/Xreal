#!BPY

"""
Name: 'Blender 2 XreaL Exporter (XreaL BSP)'
Blender: 244
Group: 'Export'
Tooltip: 'fully featured Blender to Xreal Map File Exporter'
"""


#CONFIG: enter the path to the xreal root dir
ENGINEPATH = "/home/otty/Developing/XreaL/"

import blender2xreal_tools
from blender2xreal_tools import *


btHullsize = Blender.Draw.Create(1024)
btHullTex = Blender.Draw.Create("textures/layout/sky")
btMapName = Blender.Draw.Create("blend2Xmap")
btMsg = Blender.Draw.Create("generated by blend2Xmap")
btgvt = Blender.Draw.Create(800)
btMusic = Blender.Draw.Create("")
btFog = Blender.Draw.Create(0.0)
btFr = Blender.Draw.Create(0.03)
btFg = Blender.Draw.Create(0.02)
btFb = Blender.Draw.Create(0.0)
btGSx = Blender.Draw.Create(64)
btGSy = Blender.Draw.Create(64)
btGSz= Blender.Draw.Create(128)
btAr = Blender.Draw.Create(1.0)
btAg = Blender.Draw.Create(1.0)
btAb = Blender.Draw.Create(1.0)
btBuildMode = Blender.Draw.Create(1)
btCompileMode = Blender.Draw.Create(2)
btCamType = Blender.Draw.Create(0)

swMap = Blender.Draw.Create(1)
swCamera = Blender.Draw.Create(0)
swAnim = Blender.Draw.Create(1)

EVENT_NOEVENT = 0
EVENT_QUIT = 1
EVENT_COMPILE = 2
EVENT_LOAD = 3
EVENT_SAVE = 4


EVENT_SWMap = 101
EVENT_SWCamera = 102
EVENT_SWAnim = 103

EVENT_CamInsert = 201
EVENT_CamClearAll = 202
EVENT_CamExport = 203
guiMode = 0

sCamEndFrames = []
sCamBeginFrames = []

sCamFramerate = Blender.Draw.Create(25)


for i in range(0, 50):
	sCamEndFrames.append(Blender.Draw.Create(0))
	sCamBeginFrames.append(Blender.Draw.Create(0))

def Blend2X_PrepareGroups():
	global compile_list
	#log("--->preapring groups...")
	#check if all groups for entitys and data exist
	for cl in compile_list:
		groupname = cl[0]
		if(Blend2X_GroupExists(groupname) == 0):
			log("------>creating group %s" % groupname)
			Blender.Group.New(groupname)


def Blend2X_Update():
	global objects
	global scene
	global cameras
	global lightData
	global entityData
	global cameraData
	global worldData
	global brushData
	global worldSpawnData
	global entityBBoxData
	global warnings
	global compile_list
	global logData
	
	scene = []
	cameras = []
	objects = []

	entityData = []
	lightData = []
	worldData = []
	brushData = []
	entityBBoxData = []
	worldSpawnData = []
	
	#cameras are ignored here!
	#cameraData = []
	
	warnings = 0

	Window.WaitCursor(1)
		
	scene = Blender.Scene.getCurrent()
	scene.update(1)
	
	#cameras are ignored here!
	cameras = Blender.Camera.Get()
	objects = scene.objects
	
	compile_list = []
	
	#add possible entity groups here....
	compile_list.append( [ "info_player_start", 0])
	compile_list.append( [ "trigger_multiple", 3])
	compile_list.append( [ "brushdata", 2])
	compile_list.append( [ "meshdata", 1])
	compile_list.append( [ "weapon_rocketlauncher", 0])
	
	worldSpawnData = [  ["perPolyCollision" , "1"]]

	if not btMsg.val == "":
		worldSpawnData.append(["message",("%s" % btMsg.val)])
	if not btgvt.val == "":
		worldSpawnData.append(["gravity",("%s" % btgvt.val)])
	if not btMusic.val == "":
		worldSpawnData.append(["music",("%s" % btMusic.val)])
	
	if btFog.val > 0.0:
		worldSpawnData.append(["fogDensity",("%f" % btFog.val)])
		worldSpawnData.append(["fogColor",("%f %f %f" % (btFr.val,btFg.val,btFb.val))])

	
	worldSpawnData.append(["ambientColor",("%f %f %f" % (btAr.val,btAg.val,btAb.val))])
	worldSpawnData.append(["gridsize",("%i %i %i" % (btGSx.val,btGSy.val,btGSz.val))])


	Blend2X_PrepareGroups()
	
		
	ignore = len(objects) + 1
	
	#log("--->updateing buffer...")

	for ob in objects:
		#log("checking %s" % ob.name)

		if ob.getType() == "Camera":
			#Cameras are ignored here....
			#log("------>found camera '%s'..." % ob.name)
			#cameraData.append(ob)
			ignore = ignore-1
			
		elif ob.getType() == "Lamp":
			ignore = ignore-1
			#log("------>found light '%s'..." % ob.name)
			lightData.append(ob)
			ignore = ignore-1
					
		else:
			for n in range( 0, len(compile_list)):
				if Blend2X_GroupCheck ( ob, compile_list[n][0]):
					ignore = ignore-1
					if compile_list[n][1] == 0:						
						entityData.append(ob)
						#log("------>found entity '%s' type '%s'" % (ob.name, compile_list[n][0]) )
					elif compile_list[n][1] == 1:
						worldData.append(ob)
						#log("------>found mesh data '%s'" % ob.name )
						
					elif compile_list[n][1] == 2:
						brushData.append(ob)
						#log("------>found brushdata '%s'" % ob.name )
					elif compile_list[n][1] == 3:
						entityData.append(ob)
						entityBBoxData.append(ob)
						#log("------>found bbox data entity '%s' type '%s'" % (ob.name, compile_list[n][0]) )					
					else:
						warning("unknown object type '%i' for '%s' (ignoring)" % (compile_list[n][1], ob.name) )	
						
						continue				
					
		
		
	#log("useable data:")
	#log("------>found %i entitys" % len(entityData))
	#log("------>found %i lights" % len(lightData))
	#log("------>found %i cameras" % len(cameraData))
	#log("------>found %i brushs" % len(brushData))	
	#log("------>found %i meshes" % len(worldData))
	#log("------>ignoring %i objects" % ignore)	
	
	
	Window.WaitCursor(0)	
			
def brushBuild(ob, sizeData, file, tex):
	obMesh = ob.getData(mesh=True)
	obFaces = obMesh.faces	

	log("--------->using material %s" % tex)
		
	bDefString = (" ( ( 0.031250 0.000000 0.000000 ) ( 0.000000 0.031250 0.000000 ) ) \"%s\"" % tex )
	
	for iter in enumerate(obFaces):
		k, f = iter				
		normal = f.no							
		center = f.cent			
		origin = ob.getLocation()		
		p = vector_by_matrix ( center, ob.getMatrix() )			
		dist = (vector_dotproduct(vector_sum(origin, p), normal))/vector_length(normal)
		sizeData.append ([(normal[i]) for i in range(3)] + [(-dist) ])	



	#TODO: the texture coordinates are wrong for brushwork. since it should only be 
	#used for manual invisible clipping its ok. 
	
	for i in range ( 0, len(obFaces)):
		if obMesh.faceUV :
			face = obFaces[i]
			print(face.uv)
			bDefString = (" ( ( 0.031250 0.000000 0.000000 ) ( 0.000000 0.031250 0.000000 ) ) \"%s\"" % tex )


		file.write("( %f %f %f %f )" % tuple(sizeData[i]) +  "%s \n" % bDefString )

		
def Blend2X_Entitys(file):
	log("--->writing entity data...")
	n = 0

	for ob in entityData:
		log("------>%s :" % ob.name)			
		bbox = 0
		skip = 0
		
		obMesh = ob.getData()
		obMat = obMesh.getMaterials()
					
		for bob in entityBBoxData:
			if(bob == ob):				
					log("--------->found bbox")
					bbox = 1
					if (len (obMat) <= 0):
						warning(" this object has no material (needed for bbox), skipping")
						skip = 1
				
		if skip == 1:
			continue
				
		file.write("// entity %i \n" % (n + 1))
		file.write("{\n")
		file.write("\"classname\" \"%s\"\n" % Blend2X_GroupForEntity(ob).name)
		file.write("\"name\" \"%s\"\n" % ob.getName())
		
		if bbox == 1:			
			log("--------->creating bbox")	

			if (len (obMat) > 1):
						warning(" this object has more than one material, using first")
				
				
			tex = "textures/%s" % obMat[0].name
			log("--------->using material %s" % tex)	

			sizeData = [ ]
									
			file.write("\"model\" \"%s\"\n" % ob.getName())
			file.write("// brush %i\n" % 0)
			file.write("{\n")
			file.write("brushDef3\n")
			file.write("{\n")
			
			brushBuild(ob, sizeData, file, tex)
						
			file.write("}\n")
			file.write("}\n")		
		else:			
			file.write("\"origin\" \"")
			file.write("%i " % ob.LocX)
			file.write("%i " % ob.LocY)
			file.write("%i" % ob.LocZ)
			file.write("\"\n")		
			file.write("\"angle\" \"%i\"\n" % (ob.RotZ / math.pi * 180 ))
	
	
		log("--------->adding property keys...")
		
		props = ob.getAllProperties()
		
		for prop in props:
			file.write("\"%s\" \"%s\"\n" % (prop.name, prop.data ))
			log("------------>%s : %s" % (prop.name, prop.data ))
		
		
		
		log("--------->done")	
		n = n + 1
		
		file.write("}\n")
		
def Blend2X_WorldSpawn(file, size, hullShader, collision):
	global log
	global warning
	global worldSpawnData
	
	log("--->writing worldspawn data...")
	file.write("// entity 0\n")
	file.write("{\n")
	file.write("\"classname\" \"worldspawn\"\n")
	
	for ws in worldSpawnData:
		file.write("\"%s\" \"%s\"\n" % (ws[0],ws[1]))
		log("------>\"%s\" \"%s\"\n" % (ws[0],ws[1]))
		
	brushNum = 0
	
	if size > 0:
		log("--->writing skybox data...")
		log("------>size %i" % size)
		log("------>shader %s" % hullShader)
		
				
		Blend2X_SkyBox(file , size, hullShader)
		brushNum = 5	
	
	for ob in brushData:
		log("---> generating brushdata for '%s'..." % ob.name)
		
		obMesh = ob.getData()
		obMat = obMesh.getMaterials()
		
		if (len (obMat) <= 0):
			warning(" this object has no material (needed for brushdata), skipping")
			continue
		
		if isConcave(obMesh):
			warning(" shape os concave, ignoring")
			continue
			
		log("---------> convex shape detected, ok...")
		
					
		brushNum+=1

		if (len (obMat) > 1):
			warning(" this object has more than one material, using first")

							
		file.write("// brush %i\n" % brushNum)
		file.write("{\n")
		file.write("brushDef3\n")
		file.write("{\n")		
				
		sizeData = [ ]
			
		brushBuild(ob, sizeData, file, obMat)
	
		file.write("}\n")
		file.write("}\n")		
	file.write("}\n")
	
				
	
def Blend2X_Map (filename) :
	out = file(filename, "w")
	
	Blend2X_Header(out)
	Blend2X_WorldSpawn(out, btHullsize.val, btHullTex.val, 1)
	Blend2X_Entitys(out)
	
	for n in range(0, len(lightData)):
		log("--->writing light %s...\n" % lightData[n].getName())
		light = lightData[n]
		lData = light.getData()
		Blend2X_Lights(out, (n+numEntitys), light, lData )
		
		
	
	log("--->writing worldmesh data...")
	Blend2X_WorldMesh(out, numEntitys+numLights, ("models/meshes/%s_WORLDMESH.ase" % btMapName.val)	)

	out.flush()
	
def Blend2X_Compile ():
	
	FILE = ENGINEPATH
	FILE += ("base/models/meshes/%s_WORLDMESH.ase" % btMapName.val)
	
	log("---> starting ASE mesh data generation!")
	log("---> writing %s ..." % FILE)
	ase_compile(FILE, worldData)

	FILE = ENGINEPATH
	FILE += ("base/maps/%s.map" % btMapName.val)
	log("---> generating %s ..." % FILE)
	Blend2X_Map(FILE)
	
	if btCompileMode.val == 2:
		log("---> generating full BSP for %s ..." % FILE)
		Blender.Redraw(1)			
		subprocess.call(("cd %s/blender && sh ./blender2xreal_build.sh %s %s" % ( ENGINEPATH, btBuildMode.val, FILE)), shell=True)
	else:
		log("---> skipping BSP build (set compilemode)")




class MD5CameraVX:
  def __init__(self, framerate, type):
    self.commandline = ""
    self.framerate = framerate
    self.type = type
    self.cuts = []
    self.frames = []
  def to_md5camera(self):
    buf = "MD5Version 11\n"
    buf = buf + "commandline \"%s\"\n\n" % (self.commandline)
    buf = buf + "numFrames %i\n" % (len(self.frames))
    buf = buf + "frameRate %i\n" % (self.framerate)
    buf = buf + "numCuts %i\n" % (len(self.cuts))
    buf = buf + "type %i\n\n" % (self.type)

    buf = buf + "cuts {\n"
    for c in self.cuts:
      buf = buf + "\t%i\n" % (c)
    buf = buf + "}\n\n"
    
    buf = buf + "camera {\n"
    for f in self.frames:
      buf = buf + "\t( %f %f %f ) ( %f %f %f ) %f\n" % (f)
    buf = buf + "}\n\n"
    return buf

def export_camera(cams, ranges, framerate,type, FILE):
	if len(cams)==0: return None
	if len(ranges) != len(cams): return None

	context = scene.getRenderingContext()
	
	scale = 1.0
	
	themd5cam = MD5CameraVX(framerate, type)
	
	for camIndex in range(len(cams)):
		camobj = Blender.Object.Get(cams[len(cams)-camIndex-1].name)

  
		#generate the animation
		r = ranges[camIndex]

		for i in range(r[0], r[1]):
			log("exporting cam %i frame %i" % ( camIndex, i))
			context.currentFrame(i)    
			scene.makeCurrent()
			Blender.Redraw() # apparently this has to be done to update the object's matrix. Thanks theeth for pointing that out
			loc = camobj.getLocation()
			m1 = camobj.getMatrix('worldspace')
	    
			# this is because blender cams look down their negative z-axis and "up" is y
			# doom3 cameras look down their x axis, "up" is z
			m2 = [[-m1[2][0], -m1[2][1], -m1[2][2], 0.0], [-m1[0][0], -m1[0][1], -m1[0][2], 0.0], [m1[1][0], m1[1][1], m1[1][2], 0.0], [0,0,0,1]]
			qx, qy, qz, qw = matrix2quaternion(m2)
			if qw>0:
				qx,qy,qz = -qx,-qy,-qz
	
			fov = 2 * math.atan(16/cams[camIndex].getLens())*180/math.pi
			themd5cam.frames.append((loc[0]*scale, loc[1]*scale, loc[2]*scale, qx, qy, qz, fov))
			print("Adding frame...")
					
		if camIndex != len(cams) - 1:
			themd5cam.cuts.append(len(themd5cam.frames) + 1)
			log("adding cut...")
			
	try:
		file = open(FILE, 'w')
  	except IOError, (errno, strerror):
		alert( "IOError #%s: %s" % (errno, strerror))

  	buffer = themd5cam.to_md5camera()
	print(buffer)
  	file.write(buffer)
  	file.close()
  	log("...done!")

	return



def CamExport(FILE):
	log("exporting to %s..." % FILE)

	ranges = []
	
	for i in range ( 0, len(cameraData)):
		if sCamBeginFrames[i].val == sCamEndFrames[i].val:
			alert("Error: Begin = End in cam %i" % i)
			return
		if sCamBeginFrames[i].val > sCamEndFrames[i].val:
			alert("Error: Begin > End in cam %i" % i)
			return
			
		ranges.append([sCamBeginFrames[i].val, sCamEndFrames[i].val])

	
	export_camera(cameraData, ranges, sCamFramerate.val,btCamType.val, FILE)

	
def Blend2X_CamExport(data):
	Blender.Window.FileSelector(CamExport, "Select md5camera file...")

	return


def Blend2X_GUI_event(evt, val):	
	if evt == Blender.Draw.ESCKEY:
		Blender.Draw.Exit()
	return

def Blend2X_GUI_button_event(evt):
	global guiMode, cameras, cameraData

	Blend2X_Update()

	if evt == EVENT_QUIT:
		log("---> goodbye!")
		Blender.Draw.Exit()
		return
		
  	if evt == EVENT_COMPILE:
		Blend2X_Update()

		if btMapName.val == "":
			log("---> empty mapname given!")
			return

		
		log("---> compiling map.....%s" % btMapName.val)
		Window.WaitCursor(1)
		Blend2X_Compile()

		log("\n")
		log("FINISHED with %i warnings ( see log" % warnings)	
		log("and stdout for more information)")	
		log("\n")
		Window.WaitCursor(0)
		alert("Build finished.")
		#Blender.Draw.Exit()

	
	if evt == EVENT_SWMap:
		guiMode = 0
		
	if evt == EVENT_SWCamera:
		guiMode = 1

	if evt == EVENT_SWAnim:	
		guiMode = 2

	if evt == EVENT_CamInsert:
		camMenu = []
		
		i = 1
		for cam in cameras:			
			camMenu.append(("%s" % ( cam.name), i))
			i = i + 1

		cam = Draw.PupTreeMenu((camMenu)) - 1
		if cam >= 0:
			cameraData.append(cameras[cam])
		
	if evt == EVENT_CamClearAll:
		cameraData = []
		
	if evt == EVENT_CamExport:
		if len(cameraData) <= 0:
			log("no cameras to export")
		else:
			log("exporting %i cameras:" % len( cameraData))
			Blend2X_CamExport(cameraData)
		
		
	Blender.Redraw(1)

		
	#alert("TODO")
		

btCamType.val = 1
def Blend2X_GUI_Camera(x_start, y_start, btHeight):
	global cameraData, cameras, btCamType, sCamFramerate
	
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Camera options: ")

	y_start = y_start - 30
	

	Blender.BGL.glRasterPos2i(x_start, y_start)
	Draw.Text("Type: ")
	btCamType = Draw.Menu("Type: %t|dynamic %x1|client %x2", EVENT_NOEVENT, x_start + 70, y_start-5, 170, btHeight, btCamType.val, "select camtype")

	y_start = y_start - 35
	sCamFramerate = Blender.Draw.Slider("Framerate:", EVENT_NOEVENT, x_start , y_start, 240, 20, sCamFramerate.val, 1, 25, 0, "The Framerate")
		

	y_start = y_start - 50	
	Draw.Button("Insert...", EVENT_CamInsert, x_start, y_start, 70, btHeight)
	Draw.Button("Clear all...", EVENT_CamClearAll, x_start + 80, y_start, 70, btHeight)
	Draw.Button("Export", EVENT_CamExport, x_start + 160, y_start, 70, btHeight)
	y_start = y_start - 30
			
		
	if len(cameraData) == 0:
		Blender.BGL.glRasterPos2i(x_start, y_start-20)
		Draw.Text("no cameras selected ( from %i) " % len(cameras))	
		return
	
	i = 0	
	for cam in cameraData :
		Blender.BGL.glRasterPos2i(x_start, y_start)
		Draw.Text(("Camera %i:" % i))	
		sCamBeginFrames[i] = Blender.Draw.Slider("Start Frame:", EVENT_NOEVENT, x_start + 10, y_start-30, 240, 20, sCamBeginFrames[i].val, 1, 500, 0, "The first frame of animation to export")
		sCamEndFrames[i] = Blender.Draw.Slider("End Frame:", EVENT_NOEVENT, x_start + 10, y_start-55, 240, 20, sCamEndFrames[i].val, 1, 500, 0, "The last frame of animation to export")		
				
		i = i + 1
		y_start = y_start - 80
		
		
		
	
def Blend2X_GUI_Anim(x_start, y_start, btHeight):
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Anim options: ")
	return
		
def Blend2X_GUI_MapBsp(x_start, y_start, btHeight):
	global btHullsize, btHullTex, btMapName, btBuildMode,btMsg,btgvt , btMusic, btCompileMode,  btFog, btFr, btFg, btFb, btAr, btAg, btAb, btGSx, btGSy, btGSz
	global swMap
	global swCamera
	global swAnim
		
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Map / BSP options: ")
	btMapName=Draw.String("Mapname: ", EVENT_NOEVENT, x_start, y_start-30, 240, btHeight, btMapName.val, 20, "Mapname without any extention")
	btMsg=Draw.String("MotD: ", EVENT_NOEVENT, x_start, y_start-55, 240, btHeight, btMsg.val, 40, "Message of the day")
	btMusic=Draw.String("Music: ", EVENT_NOEVENT, x_start, y_start-80, 240, btHeight, btMusic.val, 40, "Mapname without any extention")
	btgvt=Draw.Number("Gravity:", EVENT_NOEVENT, x_start, y_start-105, 240, btHeight, btgvt.val, 0, 3000, "Set gravity ( 800 default)")	

	#TODO add these worldspawn keys:
	#["deluxeMapping" , "1"]
			
	y_start -= 130
	Blender.BGL.glRasterPos2i(x_start, y_start)
	Draw.Text("Compilemode: ")
	btCompileMode=Draw.Menu("Compilemode %t|map only %x1|full bsp %x2", EVENT_NOEVENT, x_start + 100, y_start-5, 140, btHeight, btCompileMode.val, "select compilemode")
	
	y_start -= 25			
	if btCompileMode.val == 2:
		Blender.BGL.glRasterPos2i(x_start, y_start)
		Draw.Text("Buildmode: ")
		btBuildMode=Draw.Menu("Buildmode %t|test %x1|full %x2|xmap %x3", EVENT_NOEVENT, x_start + 100, y_start-5, 140, btHeight, btBuildMode.val, "select buildmode")
	
	y_start -= 40
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Fog: ")

	btFog=Draw.Number("Density:", EVENT_NOEVENT, x_start, y_start-30, 240, btHeight, btFog.val, 0.0, 1.0, "Set fog density. Set to 0 to disable ")
	if btFog.val > 0.0:
		btFr=Draw.Number("R: ", EVENT_NOEVENT, x_start, y_start-55, 75, btHeight, btFr.val, 0.0, 1.0, "RED")
		btFg=Draw.Number("G: ", EVENT_NOEVENT, x_start+81, y_start-55, 75, btHeight, btFg.val, 0.0, 1.0, "GREEN")
		btFb=Draw.Number("B: ", EVENT_NOEVENT, x_start+162, y_start-55, 75, btHeight, btFb.val, 0.0, 1.0, "BLUE")
	
	
	y_start -= 90
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Skybox: ")

	btHullsize=Draw.Number("Size:", EVENT_NOEVENT, x_start, y_start-30, 240, btHeight, btHullsize.val, 0, 8192, "Set size of caulkhull. Set to 0 to disable hull ( recommend )")
	btHullTex=Draw.String("Material: ", EVENT_NOEVENT, x_start, y_start-55, 240, btHeight, btHullTex.val, 100, "Hull Material")


	y_start -= 80
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Ambient: ")
	btAr=Draw.Number("R: ", EVENT_NOEVENT, x_start, y_start-30, 75, btHeight, btAr.val, 0.0, 1.0, "RED")
	btAg=Draw.Number("G: ", EVENT_NOEVENT, x_start+81, y_start-30, 75, btHeight, btAg.val, 0.0, 1.0, "GREEN")
	btAb=Draw.Number("B: ", EVENT_NOEVENT, x_start+162, y_start-30, 75, btHeight, btAb.val, 0.0, 1.0, "BLUE")

	y_start -= 50
	Blender.BGL.glRasterPos2i(x_start-10, y_start)
	Draw.Text("Lightmap Grid Size: ")
	btGSx=Draw.Number("X: ", EVENT_NOEVENT, x_start, y_start-30, 75, btHeight, btGSx.val, 8, 1024, "X Size")
	btGSy=Draw.Number("Y: ", EVENT_NOEVENT, x_start+81, y_start-30, 75, btHeight, btGSy.val, 8, 1024, "Y Size")
	btGSz=Draw.Number("Z: ", EVENT_NOEVENT, x_start+162, y_start-30, 75, btHeight, btGSz.val, 8, 1024, "Z Size")

	y_start -= 80
	Draw.Button("Start Compile", EVENT_COMPILE, x_start+15, y_start, 200, btHeight, "Start compilation")
			
	return


def Blend2X_GUI_Console(x, y):
	global logData
	
	line = 0

  	Blender.BGL.glRasterPos2i(x, y + 4)
	Draw.Text("console:")
	
	start = len(logData) - 40
	if start < 0:
		start=0
		
	for i in range ( start , len(logData)):
		line = line - 14
		
		Blender.BGL.glRasterPos2i(x, y + line)
		Draw.Text("%s" % logData[ i])				
		
	

def Blend2X_GUI_Main():
	
	c_x = 300
	c_y = 600

	x_start = 30
	y_start = 600
	
	btHeight = 18
	
  	Blender.BGL.glClearColor(0.6,0.6,0.6,1.0)
  	Blender.BGL.glClear(Blender.BGL.GL_COLOR_BUFFER_BIT)

	Blend2X_GUI_Console(c_x, c_y)
	
	Draw.Button("Map/Bsp", EVENT_SWMap, x_start, y_start-550, 70, btHeight)
	Draw.Button("Camera", EVENT_SWCamera, x_start+80, y_start-550, 70, btHeight)
	Draw.Button("Anim", EVENT_SWAnim, x_start+160, y_start-550, 70, btHeight)


  	Draw.Button("Quit", EVENT_QUIT, x_start, y_start - 580, 70, btHeight, "Quit this script")
  	Draw.Button("Load", EVENT_SAVE, x_start+80, y_start - 580, 70, btHeight, "Load settings")
  	Draw.Button("Save", EVENT_LOAD, x_start+160, y_start - 580, 70, btHeight, "Save settings")

	if guiMode == 0:
		Blend2X_GUI_MapBsp(x_start, y_start, btHeight)
	elif guiMode == 1:
		Blend2X_GUI_Camera(x_start, y_start, btHeight)
	elif guiMode == 2:
		Blend2X_GUI_Anim(x_start, y_start, btHeight)



log("")	
log("Blender2Xreal Suite v.0.1 startup...")
log("")	
	
#log("TODO:")
#log("1. entity key=value thing..")
#log("2. light shader")
#log("3. propper texture alignment for brushes ( default atm)")


Blend2X_Update()

Blender.Draw.Register (Blend2X_GUI_Main, Blend2X_GUI_event, Blend2X_GUI_button_event)