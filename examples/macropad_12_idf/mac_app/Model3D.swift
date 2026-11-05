import SwiftUI
import SceneKit
import Combine

final class MacropadSceneView: SCNView {
    var onKey: ((Int)->Void)?
    var onLayer: (()->Void)?
    var keyInteractionEnabled=true
    var cameraNode:SCNNode?
    private let focus=SCNVector3(0.008,0,0.004)
    private var downPoint = NSPoint.zero
    private var lastPoint = NSPoint.zero
    private var moved = false
    private var azimuth = -0.92
    private var elevation = 0.70
    private var distance = 0.255
    func updateCamera(){
        guard let camera=cameraNode else{return}
        let horizontal=distance*cos(elevation)
        camera.position=SCNVector3(focus.x+horizontal*cos(azimuth),focus.y+horizontal*sin(azimuth),focus.z+distance*sin(elevation))
    }
    override func mouseDown(with event:NSEvent){ downPoint=convert(event.locationInWindow,from:nil);lastPoint=downPoint;moved=false }
    override func mouseDragged(with event:NSEvent){
        let p=convert(event.locationInWindow,from:nil)
        if hypot(p.x-downPoint.x,p.y-downPoint.y)>3 {moved=true}
        azimuth -= Double(p.x-lastPoint.x)*0.008
        elevation = min(1.48,max(0.36,elevation-Double(p.y-lastPoint.y)*0.007))
        lastPoint=p;updateCamera()
    }
    override func scrollWheel(with event:NSEvent){distance=min(0.48,max(0.14,distance*exp(-Double(event.scrollingDeltaY)*0.008)));updateCamera()}
    override func mouseUp(with event:NSEvent){
        guard !moved, keyInteractionEnabled else{return}
        let point=convert(event.locationInWindow,from:nil)
        for hit in hitTest(point,options:[.boundingBoxOnly:false,.searchMode:SCNHitTestSearchMode.closest.rawValue]) {
            var node:SCNNode?=hit.node
            while let n=node {
                if n.name == "Node1_001" {
                    if n.action(forKey:"layerPress") == nil {
                        let down=SCNAction.moveBy(x:0,y:0,z:-0.002,duration:0.07)
                        let up=SCNAction.moveBy(x:0,y:0,z:0.002,duration:0.11)
                        n.runAction(.sequence([down,up]),forKey:"layerPress")
                    }
                    onLayer?();return
                }
                if let name=n.name,name.hasPrefix("HE_KEY_"),let i=Int(name.dropFirst(7)),(0..<12).contains(i) {onKey?(i);return}
                node=n.parent
            }
        }
    }
}

struct MacropadModelView:NSViewRepresentable {
    @ObservedObject var model:StudioModel
    let clock:LightingClock
    var explodedStage:Int = 0
    final class Coordinator {
        var keys:[Int:SCNNode]=[:]
        var positions:[Int:SCNVector3]=[:]
        var materials:[Int:[SCNMaterial]]=[:]
        var highlightedKeys:Set<Int> = []
        var depths:[Int:CGFloat] = [:]
        var leds:[SCNNode] = []
        var layerLEDs:[SCNNode] = []
        var layerKey:SCNNode?;var layerKeyOrigin:SCNVector3?;var hardwareLayerPressed=false
        var shownLayer = -1
        var explodedStage = -1
        var explodedParts:[(SCNNode,SCNVector3)] = []
        var explodedPlate:(SCNNode,SCNVector3)?
        var lightingSignature = ""
        var lastLightHit = 0
        var timerSubscription:AnyCancellable?
        var renderTick:(()->Void)?
    }
    func makeCoordinator()->Coordinator {Coordinator()}
    func makeNSView(context:Context)->MacropadSceneView {
        let view=MacropadSceneView(frame:.zero)
        view.backgroundColor = NSColor(calibratedRed:0.045,green:0.065,blue:0.065,alpha:1)
        view.allowsCameraControl=false
        view.antialiasingMode = .multisampling4X
        // Keep the thin illuminated edges crisp while the scene is moving.
        view.isJitteringEnabled = false
        view.rendersContinuously = false
        view.preferredFramesPerSecond = 24
        view.autoenablesDefaultLighting=false
        view.onKey={i in DispatchQueue.main.async{model.selectKey(i)}}
        guard let url=Bundle.main.url(forResource:"macropad",withExtension:"usdz"),let scene=try? SCNScene(url:url,options:nil) else{return view}
        // USD preview materials do not retain all Blender shader nodes. Use
        // simple native materials for recognizable case and keycap surfaces.
        scene.rootNode.enumerateChildNodes { node,_ in
            if let geometry=node.geometry {
                for material in geometry.materials { material.lightingModel = .physicallyBased; material.roughness.contents=0.75; material.metalness.contents=0.0; material.emission.contents = NSColor.black }
            }
        }
        // Match the reference: red Hall switch housings with black bases and stems.
        if let switches=scene.rootNode.childNode(withName:"MX12",recursively:true) {
            switches.enumerateChildNodes { node,_ in
                guard let geometry=node.geometry?.copy() as? SCNGeometry else{return}
                geometry.materials=geometry.materials.map { original in
                    let material=(original.copy() as? SCNMaterial) ?? original
                    if original.name == "Material_002" {
                        material.diffuse.contents=NSColor(calibratedRed:0.62,green:0.018,blue:0.035,alpha:1)
                        material.roughness.contents=0.28
                    } else if original.name == "Material_001" {
                        material.diffuse.contents=NSColor(calibratedWhite:0.035,alpha:1)
                        material.roughness.contents=0.55
                    }
                    return material
                }
                node.geometry=geometry
            }
        }
        if let layerSwitch=scene.rootNode.childNode(withName:"SW2",recursively:true) {
            layerSwitch.enumerateChildNodes { node,_ in
                guard let geometry=node.geometry?.copy() as? SCNGeometry else{return}
                geometry.materials=geometry.materials.map { original in
                    let material=(original.copy() as? SCNMaterial) ?? original
                    if ["mat_10","Gray_Plastic__Plastic_"].contains(original.name ?? "") {
                        material.diffuse.contents=NSColor(calibratedWhite:0.045,alpha:1)
                        material.roughness.contents=0.6
                    }
                    return material
                }
                node.geometry=geometry
            }
        }
        // The current source model contains a C3 module; finish its shield as metal.
        if let module=scene.rootNode.childNode(withName:"U27",recursively:true) {
            module.enumerateChildNodes { node,_ in
                guard let geometry=node.geometry?.copy() as? SCNGeometry else{return}
                geometry.materials=geometry.materials.map { original in
                    let material=(original.copy() as? SCNMaterial) ?? original
                    if original.name == "mat_10" {
                        material.diffuse.contents=NSColor(calibratedWhite:0.65,alpha:1)
                        material.metalness.contents=0.85;material.roughness.contents=0.34
                    }
                    return material
                }
                node.geometry=geometry
            }
        }
        // Keep the USB-C connector visible when hiding the PCB assembly.
        if model.page != "Özellikler", let board=scene.rootNode.childNode(withName:"Node_0",recursively:true) {
            if let connector=board.childNode(withName:"J2",recursively:false) {
                let world=connector.worldTransform
                connector.removeFromParentNode()
                scene.rootNode.addChildNode(connector)
                connector.transform=world
            }
            board.isHidden=true
        }
        // The encoder's light-colored hardware and the switch plate are metal.
        if let encoder=scene.rootNode.childNode(withName:"_2_1879314_1",recursively:true) {
            encoder.enumerateChildNodes { node,_ in
                guard let geometry=node.geometry?.copy() as? SCNGeometry else{return}
                geometry.materials=geometry.materials.map { original in
                    let material=(original.copy() as? SCNMaterial) ?? original
                    if original.name == "Material_009" {
                        material.diffuse.contents=NSColor(calibratedWhite:0.68,alpha:1)
                        material.metalness.contents=0.85;material.roughness.contents=0.32
                    }
                    return material
                }
                node.geometry=geometry
            }
        }
        if let root=scene.rootNode.childNode(withName:"root",recursively:false),
           let plate=root.childNodes.first(where:{$0.name == "Cube_002"}) {
            plate.enumerateChildNodes { node,_ in
                guard let geometry=node.geometry?.copy() as? SCNGeometry else{return}
                geometry.materials=geometry.materials.map { original in
                    let material=(original.copy() as? SCNMaterial) ?? original
                    material.diffuse.contents=NSColor(calibratedWhite:0.48,alpha:1)
                    material.metalness.contents=0.85;material.roughness.contents=0.36
                    return material
                }
                node.geometry=geometry
            }
        }
        for name in ["Plane_001", "Plane_002"] {
            if let part=scene.rootNode.childNode(withName:name,recursively:true) {
                part.enumerateChildNodes { child,_ in
                    child.geometry?.materials.forEach { $0.diffuse.contents=NSColor(calibratedWhite:0.075,alpha:1);$0.roughness.contents=0.68;$0.metalness.contents=0.0 }
                }
            }
        }
        let coord=context.coordinator
        scene.rootNode.enumerateChildNodes {node,_ in
            guard let name=node.name,name.hasPrefix("HE_KEY_"),let i=Int(name.dropFirst(7)),(0..<12).contains(i) else{return}
            coord.keys[i]=node;coord.positions[i]=node.position
            var list:[SCNMaterial]=[]
            func clone(_ n:SCNNode){if let geometry=n.geometry?.copy() as? SCNGeometry {geometry.materials=geometry.materials.map{($0.copy() as? SCNMaterial) ?? $0};n.geometry=geometry;list.append(contentsOf:geometry.materials)};n.childNodes.forEach{clone($0)}}
            clone(node);list.forEach{$0.diffuse.contents=NSColor(calibratedWhite:0.80,alpha:1);$0.roughness.contents=0.62;$0.metalness.contents=0.0};coord.materials[i]=list
        }
        if let knob=scene.rootNode.childNode(withName:"Cylinder",recursively:true) {
            knob.enumerateChildNodes { child,_ in child.geometry?.materials.forEach { $0.diffuse.contents=NSColor(calibratedWhite:0.48,alpha:1);$0.metalness.contents=0.7;$0.roughness.contents=0.38 } }
        }
        // A flat approximate illuminated surface sits below each keycap.
        let minX=coord.positions.values.map(\.x).min() ?? 0, maxX=coord.positions.values.map(\.x).max() ?? 0
        let minY=coord.positions.values.map(\.y).min() ?? 0, maxY=coord.positions.values.map(\.y).max() ?? 0
        for i in 0..<12 {
            guard let p=coord.positions[i] else {continue}
            let group=SCNNode();group.position=SCNVector3(p.x,p.y,0.01430)
            // Match the Blender key pitch so adjacent surfaces meet edge-to-edge.
            let left:Float = abs(p.x-minX)<0.001 ? 0.002 : 0, right:Float = abs(p.x-maxX)<0.001 ? 0.002 : 0
            let bottom:Float = abs(p.y-minY)<0.001 ? 0.002 : 0, top:Float = abs(p.y-maxY)<0.001 ? 0.002 : 0
            let panel=SCNPlane(width:CGFloat(0.01935+left+right),height:CGFloat(0.019162+bottom+top))
            panel.cornerRadius=0
            let mat=SCNMaterial();mat.lightingModel = .constant;mat.isDoubleSided=false
            panel.materials=[mat]
            let surface=SCNNode(geometry:panel);surface.position=SCNVector3((right-left)/2,(top-bottom)/2,0)
            group.addChildNode(surface)
            scene.rootNode.addChildNode(group);coord.leds.append(group)
        }
        // The exported model combines all four indicators in one mesh and
        // hard-codes the first one white. Replace that mesh at its measured
        // positions, keeping the original indicator dimensions and spacing.
        if let original=scene.rootNode.childNode(withName:"Cube_017",recursively:true), original.parent?.name == "Cube_003" {
            original.isHidden=true
            for index in 0..<4 {
                let marker=SCNPlane(width:0.00277,height:0.00132)
                marker.cornerRadius=0.00018
                let material=SCNMaterial();material.lightingModel = .constant;marker.materials=[material]
                let node=SCNNode(geometry:marker)
                node.position=SCNVector3(-0.0079,0.047925-Float(index)*0.005178,0.01475)
                scene.rootNode.addChildNode(node);coord.layerLEDs.append(node)
            }
        }
        if let root=scene.rootNode.childNode(withName:"root",recursively:false) {
            if let plate=root.childNodes.first(where:{$0.name == "Cube_002"}) {coord.explodedPlate=(plate,plate.position)}
            for name in ["Plane_002","Cylinder","Cube_003"] {
                if let part=root.childNodes.first(where:{$0.name == name}) {coord.explodedParts.append((part,part.position))}
            }
        }
        if let switches=scene.rootNode.childNode(withName:"MX12",recursively:true) {coord.explodedParts.append((switches,switches.position))}
        if let layerKey=scene.rootNode.childNode(withName:"Node1_001",recursively:true) {coord.layerKey=layerKey;coord.layerKeyOrigin=layerKey.position;coord.explodedParts.append((layerKey,layerKey.position))}
        for node in coord.layerLEDs {coord.explodedParts.append((node,node.position))}
        let floor=SCNPlane(width:20,height:20);let floorMaterial=SCNMaterial();floorMaterial.lightingModel = .physicallyBased;floorMaterial.roughness.contents=0.9;floorMaterial.diffuse.contents=NSColor(calibratedRed:0.11,green:0.14,blue:0.14,alpha:1);floor.materials=[floorMaterial]
        let surface=SCNNode(geometry:floor);surface.position=SCNVector3(0.008,0,-0.008);scene.rootNode.addChildNode(surface)
        let target=SCNNode();target.position=SCNVector3(0.008,0,0.004);scene.rootNode.addChildNode(target)
        let camera=SCNNode();camera.camera=SCNCamera();camera.camera?.zNear=0.001;camera.camera?.zFar=30;camera.camera?.fieldOfView=30;camera.camera?.screenSpaceAmbientOcclusionIntensity=0.18;camera.camera?.screenSpaceAmbientOcclusionRadius=0.002;camera.camera?.screenSpaceAmbientOcclusionBias=0.0005;camera.position=SCNVector3(0.13,-0.17,0.17)
        let look=SCNLookAtConstraint(target:target);look.isGimbalLockEnabled=true;look.worldUp=SCNVector3(0,0,1);camera.constraints=[look];scene.rootNode.addChildNode(camera)
        let ambient=SCNNode();ambient.light=SCNLight();ambient.light?.type = .ambient;ambient.light?.intensity=400;ambient.light?.color=NSColor.white;scene.rootNode.addChildNode(ambient)
        let light=SCNNode();light.light=SCNLight();light.light?.type = .directional;light.light?.intensity=800;light.light?.castsShadow=true;light.light?.shadowRadius=6;light.light?.shadowMapSize=CGSize(width:2048,height:2048);light.light?.shadowBias=0.001;light.light?.shadowColor=NSColor.black.withAlphaComponent(0.45);light.eulerAngles=SCNVector3(-0.6,0.4,-0.2);scene.rootNode.addChildNode(light)
        let fill=SCNNode();fill.light=SCNLight();fill.light?.type = .directional;fill.light?.intensity=220;fill.light?.color=NSColor(calibratedRed:0.80,green:0.90,blue:1,alpha:1);fill.eulerAngles=SCNVector3(-1.1,-0.8,2.2);scene.rootNode.addChildNode(fill)
        view.scene=scene;view.pointOfView=camera;view.cameraNode=camera;view.updateCamera()
        coord.timerSubscription=clock.$tick.dropFirst().sink { [weak coord] _ in
            coord?.renderTick?()
        }
        return view
    }
    func updateNSView(_ view:MacropadSceneView,context:Context) {
        let coordinator=context.coordinator
        coordinator.renderTick={ [weak view, weak coordinator] in
            guard let view=view,let coordinator=coordinator else{return}
            self.render(view,coordinator:coordinator)
        }
        render(view,coordinator:coordinator)
    }
    private func render(_ view:MacropadSceneView,coordinator:Coordinator) {
        view.onKey={i in DispatchQueue.main.async{model.selectKey(i)}}
        view.onLayer={DispatchQueue.main.async{model.nextLayer()}}
        if coordinator.shownLayer != model.layer {
            for (i,node) in coordinator.layerLEDs.enumerated() {
                node.geometry?.firstMaterial?.diffuse.contents = i == model.layer ? NSColor.white : NSColor(calibratedWhite:0.35,alpha:1)
            }
            coordinator.shownLayer=model.layer
        }
        let stage=model.page == "Özellikler" ? explodedStage : 0
        view.keyInteractionEnabled = model.page != "Özellikler" || stage == 0
        let stageChanged=coordinator.explodedStage != stage
        SCNTransaction.begin();SCNTransaction.animationDuration=stageChanged ? 0.55 : 0.04
        let hardwarePressed=model.deviceLayer?.pressed == true && stage == 0
        if hardwarePressed != coordinator.hardwareLayerPressed,let key=coordinator.layerKey,let origin=coordinator.layerKeyOrigin {key.removeAction(forKey:"layerPress");key.position=SCNVector3(origin.x,origin.y,origin.z-(hardwarePressed ? 0.002 : 0));coordinator.hardwareLayerPressed=hardwarePressed}
        if stageChanged {
            for (part,origin) in coordinator.explodedParts {part.position=SCNVector3(origin.x,origin.y,origin.z+(stage >= 2 ? (stage == 3 ? 0.055 : 0.035) : 0))}
            if let (plate,origin)=coordinator.explodedPlate {plate.position=SCNVector3(origin.x,origin.y,origin.z+(stage == 3 ? 0.020 : 0))}
            for led in coordinator.leds {led.isHidden = model.page == "Özellikler"}
            coordinator.depths.removeAll();coordinator.explodedStage=stage
        }
        let highlights:Set<Int> = ["Özellikler","Canlı test"].contains(model.page) ? [] : (model.page == "Aydınlatma" ? Set(coordinator.keys.keys.filter{model.lightingKeyHighlighted($0)}) : [model.selected])
        for (i,node) in coordinator.keys {
            let amount=stage == 0 ? CGFloat(model.displayedPercent(i)/100)*0.0032 : (stage == 1 ? -0.035 : (stage == 2 ? -0.075 : -0.095))
            if coordinator.depths[i] != amount, let origin=coordinator.positions[i] { node.position=SCNVector3(origin.x,origin.y,origin.z-amount);coordinator.depths[i]=amount }
            if coordinator.highlightedKeys != highlights {
                for material in coordinator.materials[i] ?? [] {material.emission.contents = highlights.contains(i) ? NSColor(calibratedRed:0.10,green:0.32,blue:0.20,alpha:1) : NSColor.black}
            }
        }
        coordinator.highlightedKeys=highlights
        SCNTransaction.commit()
        let l=model.config.layers[model.layer].light
        let signature="\(model.lightingPreviewActive)|\(l.effectiveEffect)|\(l.colorful)|\(l.color)|\(model.effectiveLightBrightness)|\(l.speed)"
        if signature != coordinator.lightingSignature {
            coordinator.lightingSignature=signature
            let hex=l.color.trimmingCharacters(in:CharacterSet(charactersIn:"#"))
            let rgb=Int(hex,radix:16) ?? 0xA6F0CA
            let color=NSColor(calibratedRed:CGFloat((rgb>>16)&255)/255,green:CGFloat((rgb>>8)&255)/255,blue:CGFloat(rgb&255)/255,alpha:1)
            let level=model.lightingPreviewActive && l.effectiveEffect != "Kapalı" ? CGFloat(model.effectiveLightBrightness/100) : 0
            for (i,node) in coordinator.leds.enumerated() {
                let reactive = ["Tepkisel","Basılan yerden dalga","Derinliğe duyarlı","Derinliğe duyarlı dalga"].contains(l.effectiveEffect)
                node.removeAllActions();node.opacity=reactive || l.effectiveEffect == "Kayan ışık" ? 0 : level
                let shade = l.effectiveEffect=="Gökkuşağı" ? NSColor(calibratedHue:CGFloat(i)/12,saturation:0.75,brightness:1,alpha:1) : color
                for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}
            }
            coordinator.lastLightHit=model.lightHit?.id ?? 0
        }
        if ["Nefes","Dalga"].contains(l.effectiveEffect) {
            let now=Date.timeIntervalSinceReferenceDate,duration=max(0.3,3.0-l.speed*0.025)
            for (i,node) in coordinator.leds.enumerated() {
                node.opacity=CGFloat((model.lightingPreviewActive ? model.effectiveLightBrightness/100 : 0)*model.pulseIntensity(i,at:now,duration:duration,wave:l.effectiveEffect == "Dalga"))
                if l.colorful {let shade=model.cycleColor(i,at:now,duration:duration,wave:l.effectiveEffect == "Dalga");for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}}
            }
        }
        if l.effectiveEffect == "Yılan" {
            let now=Date.timeIntervalSinceReferenceDate,duration=max(0.3,3.0-l.speed*0.025)
            let level=model.lightingPreviewActive ? model.effectiveLightBrightness/100 : 0
            for (i,node) in coordinator.leds.enumerated() {
                node.opacity=CGFloat(level*model.snakeIntensity(i,at:now,duration:duration))
                if l.colorful {let shade=model.snakeColor(at:now,duration:duration);for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}}
            }
        }
        if l.effectiveEffect == "Starlight" {
            let now=Date.timeIntervalSinceReferenceDate,duration=max(0.3,3.0-l.speed*0.025)
            let level=model.lightingPreviewActive ? model.effectiveLightBrightness/100 : 0
            for (i,node) in coordinator.leds.enumerated() {
                node.opacity=CGFloat(level*model.starlightIntensity(i,at:now,duration:duration))
                if l.colorful {let shade=model.starlightColor(i,at:now,duration:duration);for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}}
            }
        }
        if l.effectiveEffect == "Gökkuşağı" {
            let now=Date.timeIntervalSinceReferenceDate,duration=max(0.3,3.0-l.speed*0.025)
            for (i,node) in coordinator.leds.enumerated() {
                let shade=model.rainbowColor(i,at:now,duration:duration)
                for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}
            }
        }
        if ["Derinliğe duyarlı","Derinliğe duyarlı dalga","Basılan yerden dalga"].contains(l.effectiveEffect) {
            let now=Date.timeIntervalSinceReferenceDate
            let brightness=model.lightingPreviewActive ? model.effectiveLightBrightness/100 : 0
            for (i,node) in coordinator.leds.enumerated() {
                let intensity=l.effectiveEffect == "Basılan yerden dalga" ? model.waveLight(i,at:now,sensitive:false) : model.pressureLight(i,at:now)
                node.opacity=CGFloat(brightness*intensity)
                if l.colorful && l.baseEffect == "Basılan yerden dalga" {
                    let shade=model.waveColor(i,at:now,sensitive:l.usesDepth)
                    for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}
                }
                if l.colorful && l.baseEffect == "Tepkisel" {
                    let shade=model.reactiveColor(i)
                    for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}
                }
            }
        }
        if l.baseEffect == "Kayan ışık" {
            let now=Date.timeIntervalSinceReferenceDate,duration=max(0.3,3.0-l.speed*0.025)
            for (i,node) in coordinator.leds.enumerated() {
                node.opacity=CGFloat((model.lightingPreviewActive ? model.effectiveLightBrightness/100 : 0)*model.sweepIntensity(i,at:now,duration:duration))
                if l.colorful {
                    let shade=model.sweepColor(i,at:now,duration:duration)
                    for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=shade}
                }
            }
        }
        if model.lightingPreviewActive, let hit=model.lightHit, hit.id != coordinator.lastLightHit,
           l.effectiveEffect == "Tepkisel" {
            coordinator.lastLightHit=hit.id
            let duration=max(0.3,3.0-l.speed*0.025), level=CGFloat(model.effectiveLightBrightness/100)
            for (i,node) in coordinator.leds.enumerated() {
                guard l.effectiveEffect != "Tepkisel" || i == hit.key else {continue}
                if l.colorful {for piece in node.childNodes {piece.geometry?.firstMaterial?.diffuse.contents=model.reactiveColor(i)}}
                let dx=Double(i/4-hit.key/4), dy=Double(i%4-hit.key%4)
                let delay=l.effectiveEffect == "Tepkisel" ? 0 : sqrt(dx*dx+dy*dy)*duration*0.16
                node.removeAllActions();node.opacity=0
                node.runAction(.sequence([.wait(duration:delay),.fadeOpacity(to:level,duration:0),.fadeOpacity(to:0,duration:duration*0.45)]))
            }
        }
    }
}
