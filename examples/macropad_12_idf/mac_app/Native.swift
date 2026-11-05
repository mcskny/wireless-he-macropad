import SwiftUI
import AppKit
import Charts
import Combine
import Darwin
import ApplicationServices
import IOKit

struct LightConfig: Codable, Equatable {
    var effect="Sabit"; var color="#A6F0CA"; var brightness=60.0; var speed=50.0
    var effectSpeeds:[String:Double]? = nil
    var depthSensitive:Bool? = nil
    var rainbowSweep:Bool? = nil; var rainbowWave:Bool? = nil; var rainbowReactive:Bool? = nil; var sweepColorStyle:String? = nil
    var rainbowBreath:Bool? = nil; var rainbowFlow:Bool? = nil; var rainbowStarlight:Bool? = nil; var rainbowSnake:Bool? = nil
    var baseEffect:String {switch effect {case "Derinliğe duyarlı":return "Tepkisel";case "Derinliğe duyarlı dalga":return "Basılan yerden dalga";default:return effect}}
    var usesDepth:Bool {false}
    var effectiveEffect:String {if usesDepth && baseEffect == "Tepkisel" {return "Derinliğe duyarlı"};if usesDepth && baseEffect == "Basılan yerden dalga" {return "Derinliğe duyarlı dalga"};return baseEffect}
    var colorful:Bool {switch baseEffect {case "Kayan ışık":return rainbowSweep ?? false;case "Basılan yerden dalga":return rainbowWave ?? false;case "Tepkisel":return rainbowReactive ?? false;case "Nefes":return rainbowBreath ?? false;case "Dalga":return rainbowFlow ?? false;case "Starlight":return true;case "Yılan":return rainbowSnake ?? true;default:return false}}
}
struct GameSettings: Codable, Equatable {
    var enabled=false; var forward=4; var left=1; var back=5; var right=9
    var deadzone=10.0; var runPoint=75.0
    var managedRunKeys:[Int]? = nil
    var kind:String? = nil
}
struct SOCDSettings: Codable, Equatable { var enabled=false; var a=0; var b=1; var follows:String? = nil }
struct KeyboardAssignment: Codable, Equatable { var keyCode:UInt8; var modifiers:UInt8; var label:String }
struct KeyBehavior: Codable, Equatable { var mode="normal"; var secondAction=""; var secondPoint=75.0; var primaryKey:KeyboardAssignment? = nil; var secondaryKey:KeyboardAssignment? = nil }
struct EncoderHold: Codable, Equatable { var enabled=false; var left=""; var right=""; var push="" }
struct LayerConfig: Codable, Equatable { var encoderHold:EncoderHold? = nil; var name=""; var keys=Array(repeating:"",count:12); var left=""; var right=""; var push=""; var light=LightConfig(); var keyBehaviors:[KeyBehavior]? = nil; var game:GameSettings? = nil; var socdSettings:SOCDSettings? = nil; var presetID:String? = nil; var taskProfile:String {presetID ?? name} }
struct HallConfig: Codable, Equatable { var mode="Geleneksel"; var point=50.0; var hysteresis=5.0; var press=5.0; var release=5.0; var min=5.0; var max=95.0 }
struct Cal: Codable, Equatable { var free:Int?; var pressed:Int? }
struct Config: Codable, Equatable {
    var testChannelMap:[Int]? = nil
    var layers=(0..<4).map { LayerConfig(name:"Layer \($0+1)") }; var hall=Array(repeating:HallConfig(),count:12)
    var mode="Hibrit"; var idle=15.0; var brightness=60.0; var socd=false; var socdA=0; var socdB=1
    var joyXM=0; var joyXP=0; var joyYM=0; var joyYP=0; var macro=""
    var calibration=Array(repeating:Cal(),count:12)
    // Raw ADC channel order; absent in existing configurations means linear.
    var responseCurves:[Double]? = nil
}
struct SensorCapture {
    enum Kind {case free,pressed,noise}
    let kind:Kind; let started:TimeInterval; let mapping:[Int]; let key:Int?
    var samples:[[Int]]=[]
}
struct SensorNoiseResult {let samples:Int;let elapsed:Double;let spans:[Int];let deviations:[Double]}
enum SensorMath {
    static func percent(raw:Int,free:Int,pressed:Int,exponent:Double)->Double? {
        guard abs(free-pressed)>=10,exponent.isFinite,(0.4...2.5).contains(exponent) else{return nil}
        let linear=max(0,min(1,Double(free-raw)/Double(free-pressed)))
        return pow(linear,exponent)*100
    }
}
struct LocalTrigger {
    var pressed=false;var highest=0.0;var lowest=0.0
    mutating func update(_ position:Double,_ h:HallConfig)->Bool {
        let p=max(0,min(100,position))
        if h.mode != "Rapid Trigger" || h.min >= h.max {
            pressed = pressed ? p > max(0,h.point-h.hysteresis) : p >= h.point
            highest=p;lowest=p;return pressed
        }
        if p <= h.min {pressed=false;highest=p;lowest=p;return false}
        if p >= h.max {pressed=true;highest=p;lowest=p;return true}
        if pressed {highest=max(highest,p);if p < highest-h.release {pressed=false;lowest=p}}
        else {lowest=min(lowest,p);if p > lowest+h.press {pressed=true;highest=p}}
        return pressed
    }
}
struct LocalSOCD {
    var last:Int? = nil
    mutating func resolve(_ states:inout [Bool],previous:[Bool],a:Int,b:Int) {
        guard states.indices.contains(a),states.indices.contains(b),a != b else {last=nil;return}
        let newA=states[a] && !previous[a],newB=states[b] && !previous[b]
        if newA && newB {last=nil} else if newA {last=a} else if newB {last=b}
        if states[a] && states[b] {if last == a {states[b]=false} else if last == b {states[a]=false} else {states[a]=false;states[b]=false}}
    }
}
struct Point: Identifiable { let id:Int; let value:Double }
struct RateTelemetry:Decodable,Equatable {
    let epoch:Int, active:Bool, app_open:Bool, gate_enabled:Bool, stats_valid:Bool
    let scans:Int, submitted:Int, skipped:Int, replaced:Int, interval_ms:Int
    let gate_on_us:Int?
    var scanRate:Int {interval_ms>0 ? Int((Double(scans)*1000/Double(interval_ms)).rounded()) : 0}
    var sendRate:Int {interval_ms>0 ? Int((Double(submitted)*1000/Double(interval_ms)).rounded()) : 0}
}
struct BridgeTelemetry:Decodable,Equatable {let rx:Int, invalid:Int, sequence_gaps:Int, restarts:Int, pending:Int, usb:Int, usb_drop:Int}
struct DeviceLayer:Decodable,Equatable {let layer:Int;let pressed:Bool}
struct HIDWriteStatus:Decodable,Equatable {let state:String;let message:String?}
struct Packet: Decodable { let connected:Bool; let values:[Int]?; let sample_id:Int?; let port:String?; let rate_status:RateTelemetry?; let bridge_status:BridgeTelemetry?; let layer_status:DeviceLayer?;let hid_write_status:HIDWriteStatus? }
// Physical test wiring, measured 1 Nov: logical keys are column-major;
// array values are the corresponding raw ADC/mux channels.
let testChannelForKey=[1,3,6,9,0,4,7,10,2,5,8,11]
enum SweepTurnTiming {
    static let pulseHalfWidth=0.18
    static let period=11.0/12.0 + 2*pulseHalfWidth
    static func cycle(at time:TimeInterval,duration:Double)->Double {floor(time/duration/period)}
    static func intensity(key:Int,at time:TimeInterval,duration:Double)->Double {
        let t=time/duration
        let phase=t-floor(t/period)*period
        let elapsed=phase-Double(key)/12
        guard elapsed >= 0 && elapsed <= 2*pulseHalfWidth else{return 0}
        return max(0,1-abs(elapsed-pulseHalfWidth)/pulseHalfWidth)
    }
}
struct LightHit { let id:Int; let key:Int; let time:TimeInterval }
struct LightWave { let key:Int; let time:TimeInterval; let duration:Double; let hue:Double; var peak:Double; var held:Bool }

final class LightingClock: ObservableObject { @Published var tick = 0 }

// Optional localhost-only CrossOver test bridge. The ESP32 serial path is unchanged.
final class DrivingBridge {
    private var fd:Int32 = -1
    private var lastAck:TimeInterval = 0
    private var target = sockaddr_in()
    init() {
        fd=socket(AF_INET,SOCK_DGRAM,0)
        if fd >= 0 { _=fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK) }
        target.sin_len=UInt8(MemoryLayout<sockaddr_in>.size)
        target.sin_family=sa_family_t(AF_INET)
        target.sin_port=in_port_t(27861).bigEndian
        target.sin_addr=in_addr(s_addr:in_addr_t(INADDR_LOOPBACK).bigEndian)
    }
    deinit {if fd >= 0 {close(fd)}}
    func send(steering:Double,throttle:Double,brake:Double)->Bool {
        guard fd >= 0 else{return false}
        func axis(_ value:Double)->Int16 {Int16(max(-32767,min(32767,Int((value*32767).rounded()))))}
        func pedal(_ value:Double)->Int16 {Int16(max(-32768,min(32767,Int((-32768+min(1,max(0,value))*65535).rounded()))))}
        var bytes:[UInt8]=[87,72,69,49]
        for x in [axis(steering),pedal(throttle),pedal(brake)] {let u=UInt16(bitPattern:x);bytes.append(UInt8(u & 255));bytes.append(UInt8(u >> 8))}
        let sent=bytes.withUnsafeBytes { data in
            withUnsafePointer(to:&target) { address in
                address.withMemoryRebound(to:sockaddr.self,capacity:1) { sendto(fd,data.baseAddress,data.count,0,$0,socklen_t(MemoryLayout<sockaddr_in>.size)) }
            }
        }
        var reply=[UInt8](repeating:0,count:4)
        while true {
            let n=recv(fd,&reply,reply.count,0)
            if n <= 0 {break}
            if n == 4 && reply == [87,72,69,65] {lastAck=ProcessInfo.processInfo.systemUptime}
        }
        return sent == bytes.count && ProcessInfo.processInfo.systemUptime-lastAck < 1
    }
}
// Mac input is deliberately opt-in and separate from the CrossOver test bridge.
// Unsupported descriptive profile tasks never generate guessed shortcuts.
// Observe published HID devices only; never open or capture keyboard input.
private func wirelessHEBluetoothHIDPresent() -> Bool {
    var iterator:io_iterator_t=0
    guard IOServiceGetMatchingServices(kIOMainPortDefault,IOServiceMatching("IOHIDDevice"),&iterator)==KERN_SUCCESS else{return false}
    defer {IOObjectRelease(iterator)}
    while true {
        let service=IOIteratorNext(iterator);if service==0 {break}
        let name=IORegistryEntryCreateCFProperty(service,"Product" as CFString,kCFAllocatorDefault,0)?.takeRetainedValue() as? String ?? ""
        let transport=IORegistryEntryCreateCFProperty(service,"Transport" as CFString,kCFAllocatorDefault,0)?.takeRetainedValue() as? String ?? ""
        IOObjectRelease(service)
        if name=="Wireless HE" && transport.localizedCaseInsensitiveContains("Bluetooth") {return true}
    }
    return false
}
final class MacKeyOutput {
    struct Key: Hashable { let code:CGKeyCode; let flags:UInt64 }
    private var down:Set<Key> = []
    private var mediaDown:Set<Int> = []
    // NX system-defined media key codes. These are momentary actions.
    static let media:[String:Int] = ["Ses artır":0,"Sesi artır":0,"Ses azalt":1,"Sesi azalt":1,"Ekran parlaklığını artır":2,"Ekran parlaklığını azalt":3,"Sesi kapat":7,"Sessize al":7,"Sesi kapat / aç":7,"Oynat / Duraklat":16,"Önceki parça":18,"Sonraki parça":17]
    // These combinations are suggestions, not claimed defaults. The user must
    // assign the same shortcuts in the target application or game.
    static let customPresetActions:[String:[String]] = [
        "OBS Studio":["Sahne 1","Sahne 2","Sahne 3","Mikrofon aç/kapat","Kamera aç/kapat","Masaüstü sesi","Yayın başlat","Yayın durdur","Kayıt başlat/durdur","Geçiş","Stüdyo modu","İşaretleyici"],
        "Minecraft":["Etkileşim"],
        "FPS":["Yetenek 1","Yetenek 2","Eğil","Zıpla","Koş","Yeniden doldur","Etkileşim","Harita"],
        "Euro Truck Simulator 2":["Korna","Kamera","Sol sinyal","Dörtlü","Sağ sinyal","Far","Park freni","Silecek"],
        "Assetto Corsa":["Debriyaj","El freni","Kamera","ABS","Çekiş kontrolü","Yeniden başlat"],
        "Fusion 360":["Çizim","Döndürerek oluştur","Birleştir","Görünümü sıfırla"],
        "Discord":["Bas-konuş","Bildirimler","Kamera aç/kapat","Ekran paylaşımı","Görüşmeden ayrıl"],
        "DaVinci Resolve":["Yakınlaştır −","Yakınlaştır +"],
        "Microsoft Flight Simulator":["İniş takımı","Kamera","Fren","Kanatçık azalt","Kanatçık artır","Motor gücü −","Motor gücü +","Pilot görünümü"],
        "Farming Simulator":["Araç değiştir","Kamera","Alet kaldır","Alet aç/kapat","Alet indir","Motor","Araç seç","Harita"]
    ]
    static func customPresetShortcut(_ label:String,profile:String)->(UInt8,UInt8)? {
        guard let index=customPresetActions[profile]?.firstIndex(of:label),index<12 else{return nil}
        return (UInt8(58+index),0x07) // Control + Option/Alt + Shift + F1…F12
    }
    static func customPresetShortcutText(_ label:String,profile:String)->String? {
        guard let (key,_)=customPresetShortcut(label,profile:profile) else{return nil}
        return "⌃⌥⇧ F\(Int(key)-57)"
    }
    static let standard:[String:(UInt8,UInt8)] = ["Geri al":(29,8),"Kaydet":(22,8),"Spotlight":(44,8),"Ekranı kilitle":(20,9),"Mission Control":(82,1),"Uygulama pencerelerini göster":(81,1),"Tüm ekranın görüntüsü":(32,10),"Seçili alanın görüntüsü":(33,10),"Yeni sekme aç":(23,8),"Sekmeyi kapat":(26,8),"Sayfayı yenile":(21,8),"Geri git":(47,8),"İleri git":(48,8),"Önceki sekme":(43,3),"Sonraki sekme":(43,1)]
    static let hidToMac:[UInt8:CGKeyCode] = [
        4:0,5:11,6:8,7:2,8:14,9:3,10:5,11:4,12:34,13:38,14:40,15:37,16:46,17:45,18:31,19:35,20:12,21:15,22:1,23:17,24:32,25:9,26:13,27:7,28:16,29:6,
        30:18,31:19,32:20,33:21,34:23,35:22,36:26,37:28,38:25,39:29,
        40:36,41:53,42:51,43:48,44:49,45:27,46:24,47:33,48:30,49:42,51:41,52:39,53:50,54:43,55:47,56:44,57:57,
        58:122,59:120,60:99,61:118,62:96,63:97,64:98,65:100,66:101,67:109,68:103,69:111,74:115,75:116,76:117,77:119,78:121,79:124,80:123,81:125,82:126,
        224:59,225:56,226:58,227:55,228:62,229:60,230:61,231:54]
    static func fromHID(_ code:UInt8,_ modifiers:UInt8)->Key? {
        guard let mac=hidToMac[code] else{return nil}
        var flags:CGEventFlags=[]
        if modifiers & 1 != 0 {flags.insert(.maskControl)}
        if modifiers & 2 != 0 {flags.insert(.maskShift)}
        if modifiers & 4 != 0 {flags.insert(.maskAlternate)}
        if modifiers & 8 != 0 {flags.insert(.maskCommand)}
        return Key(code:mac,flags:flags.rawValue)
    }
    static func resolve(_ assignment:KeyboardAssignment?,_ label:String)->Key? {
        if let a=assignment, a.label == label {return fromHID(a.keyCode,a.modifiers)}
        if let shortcut=standard[label] {return fromHID(shortcut.0,shortcut.1)}
        // Only explicit shortcut suffixes are interpreted; task descriptions are not shortcuts.
        let parts=label.components(separatedBy:" · ")
        guard parts.count == 2 else{return nil}
        let tokens=parts[1].split(separator:" ").map(String.init)
        guard let name=tokens.last else{return nil}
        var modifiers:UInt8=0
        for token in tokens.dropLast() {switch token.lowercased(){case "cmd":modifiers |= 8;case "shift":modifiers |= 2;case "ctrl":modifiers |= 1;case "option":modifiers |= 4;default:return nil}}
        let names:[String:UInt8] = ["TAB":43,"SPACE":44,"ENTER":40,"ESC":41,"SHIFT":225,"CTRL":224,"OPTION":226,"CMD":227,"F1":58,"F2":59,"F3":60,"F4":61,"F5":62,"F6":63,"F7":64,"F8":65,"F9":66,"F10":67,"F11":68,"F12":69]
        let upper=name.uppercased()
        if let hid=names[upper] {return fromHID(hid,modifiers)}
        if let ascii=upper.utf8.first,upper.utf8.count == 1,ascii >= 65 && ascii <= 90 {return fromHID(ascii-65+4,modifiers)}
        if let ascii=upper.utf8.first,upper.utf8.count == 1,ascii >= 49 && ascii <= 57 {return fromHID(ascii-49+30,modifiers)}
        if upper == "0" {return fromHID(39,modifiers)}
        return nil
    }
    static func presetKey(_ label:String,profile:String)->Key? {
        let shortcuts:[String:(UInt8,UInt8)]
        switch profile {
        case "Minecraft":
            shortcuts=["Envanter":(8,0),"Zıpla":(44,0),"Eğil":(225,0),"Koş":(224,0),"Eşya 1":(30,0),"Eşya 2":(31,0),"Eşya 3":(32,0)]
        case "Assetto Corsa":
            // The keyboard bindings shown in the user's Assetto control screen.
            // They remain configurable in the game and can be overridden per key.
            shortcuts=["Vites küçült":(224,0),"Vites büyüt":(44,0)]
        case "Photoshop":
            shortcuts=["Katman ekle":(17,10)] // Shift-Command-N
        case "Fusion 360":
            shortcuts=["Ekstrüzyon":(8,0),"Taşı":(16,0),"Ölç":(12,0),"Dikdörtgen":(21,0),"Çember":(6,0),"Çizgi":(15,0)]
        case "DaVinci Resolve":
            shortcuts=["Oynat / Duraklat":(44,0),"İleri al":(15,0),"Kes":(5,0),"Böl":(49,8),"Seçim aracı":(4,0),"Önceki işaret":(82,2),"İşaret ekle":(16,0),"Sonraki işaret":(81,2)]
        case "Spotify":
            // Spotify desktop defaults: option and shift use USB modifier bits.
            shortcuts=["Karıştır":(22,4),"Tekrar":(21,4),"Beğen":(5,6),"Çalma listesi":(30,6),"Arama":(14,8),"Kitaplık":(39,6)]
        case "Discord":
            shortcuts=["Mikrofon aç/kapat":(16,10),"Kulaklık aç/kapat":(7,10),"Önceki kanal":(82,4),"Sonraki kanal":(81,4)]
        default:shortcuts=[:]
        }
        guard let shortcut=shortcuts[label] ?? customPresetShortcut(label,profile:profile) else{return nil}
        return fromHID(shortcut.0,shortcut.1)
    }
    static func profileIsFrontmost(_ profile:String)->Bool {
        // Application-specific shortcuts should not type into an unrelated app.
        guard profile == "Spotify" || profile == "Discord" else{return true}
        let bundle=NSWorkspace.shared.frontmostApplication?.bundleIdentifier ?? ""
        return profile == "Spotify" ? bundle == "com.spotify.client" : bundle == "com.hnc.Discord"
    }
    private func modifierFlags(_ keys:Set<Key>)->CGEventFlags {
        var flags:CGEventFlags=[]
        for key in keys {switch key.code {case 56,60:flags.insert(.maskShift);case 59,62:flags.insert(.maskControl);case 58,61:flags.insert(.maskAlternate);case 55,54:flags.insert(.maskCommand);default:break}}
        return flags
    }
    private func post(_ key:Key,_ pressed:Bool,_ held:Set<Key>) {
        guard let event=CGEvent(keyboardEventSource:nil,virtualKey:key.code,keyDown:pressed) else{return}
        event.flags=CGEventFlags(rawValue:key.flags).union(modifierFlags(held));event.post(tap:.cghidEventTap)
    }
    private func postMedia(_ code:Int,_ pressed:Bool) {
        let data=(code << 16) | ((pressed ? 0x0a : 0x0b) << 8)
        guard let event=NSEvent.otherEvent(with:.systemDefined,location:.zero,modifierFlags:[],timestamp:0,windowNumber:0,context:nil,subtype:8,data1:data,data2:-1)?.cgEvent else{return}
        event.post(tap:.cghidEventTap)
    }
    func update(_ wanted:Set<Key>) {
        for key in down.subtracting(wanted) {down.remove(key);post(key,false,down)}
        let added=wanted.subtracting(down)
        // Press modifiers first so subsequent events carry their flags.
        for key in added.sorted(by:{ modifierFlags([$0]).isEmpty == false && modifierFlags([$1]).isEmpty }) {down.insert(key);post(key,true,down)}
    }
    func updateMedia(_ wanted:Set<Int>) {for code in mediaDown.subtracting(wanted) {postMedia(code,false)};for code in wanted.subtracting(mediaDown) {postMedia(code,true)};mediaDown=wanted}
    func releaseAll(){update([]);updateMedia([])}
}

final class StudioModel: ObservableObject {
    @Published private(set) var bluetoothHIDPresent=false
    @Published private(set) var bleControlReady=false
    @Published private(set) var bleSlotIndex:Int?=nil
    @Published private(set) var bleSlotMask:Int?=nil
    private var bleStudio:BLEStudio?
    private var blePendingLayer:Int?
    private var bleLastLights:[[Int]]=[]
    private var bleLightInFlight=false
    private var bluetoothStatusTimer:Timer?
    var deviceVisible:Bool {freshSensor || bluetoothHIDPresent}
    func searchBluetooth(){bleStudio?.setEnabled(true)}
    @Published var config:Config { didSet { if let d=try? JSONEncoder().encode(config) { UserDefaults.standard.set(d,forKey:"native-config-v1") } } }
    @Published var macOutputEnabled=false {didSet {if !macOutputEnabled {macKeyOutput.releaseAll();macPressed=Array(repeating:false,count:12)}}}
    private let macKeyOutput=MacKeyOutput()
    private var macPressed=Array(repeating:false,count:12)
    @Published var driveOutputEnabled=false {didSet {if !driveOutputEnabled {_ = driveBridge.send(steering:0,throttle:0,brake:0);driveBridgeStatus="Kapalı"}}}
    @Published private(set) var driveBridgeStatus="Kapalı"
    private let driveBridge=DrivingBridge()
    @Published var values:[Int]? = nil
    @Published private(set) var rateStatus:RateTelemetry? = nil
    @Published private(set) var bridgeStatus:BridgeTelemetry? = nil
    @Published var hidWriteStatus:HIDWriteStatus?=nil
    @Published var hidWriteError:String?=nil
    func writeHIDProfiles(){
        func assignment(_ explicit:KeyboardAssignment?,_ label:String,_ profile:String)->(UInt8,UInt8)? {
            if let explicit=explicit,explicit.label==label{return (explicit.keyCode,explicit.modifiers)}
            let media:[String:UInt8]=["Oynat / Duraklat":0xf0,"Önceki parça":0xf1,"Sonraki parça":0xf2,"Ses artır":0xf3,"Sesi artır":0xf3,"Ses azalt":0xf4,"Sesi azalt":0xf4,"Sesi kapat":0xf5,"Sessize al":0xf5,"Sesi kapat / aç":0xf5,"Ekran parlaklığını artır":0xf6,"Ekran parlaklığını azalt":0xf7,"Oynatmayı durdur":0xf8,"İleri sar":0xf9,"Geri sar":0xfa]
            let preset=MacKeyOutput.presetKey(label,profile:profile)
            if preset == nil,let code=media[label]{return(code,0)}
            guard let key=MacKeyOutput.resolve(nil,label) ?? preset,let usage=MacKeyOutput.hidToMac.first(where:{$0.value==key.code})?.key else{return nil}
            let flags=CGEventFlags(rawValue:key.flags);var mods:UInt8=0
            if flags.contains(.maskControl){mods|=1};if flags.contains(.maskShift){mods|=2};if flags.contains(.maskAlternate){mods|=4};if flags.contains(.maskCommand){mods|=8}
            return (usage,mods)
        }
        let targetLayer=layer
        var bytes=[UInt8](repeating:0,count:12*16)
        for l in [targetLayer] {
            let layerConfig=config.layers[l];let behaviors=layerConfig.keyBehaviors?.count == 12 ? layerConfig.keyBehaviors! : Array(repeating:KeyBehavior(),count:12)
            for key in 0..<12 {
                let label=layerConfig.keys[key],behavior=behaviors[key];if label.isEmpty {if behavior.mode == "twoStage" && !behavior.secondAction.isEmpty {hidWriteError="Layer \(l+1), KEY \(key): önce birinci görevi seç.";return};continue}
                guard let first=assignment(behavior.primaryKey,label,layerConfig.taskProfile) else {hidWriteError="Layer \(l+1), KEY \(key): ‘\(label)’ USB tuş görevi olarak desteklenmiyor. Klavye veya medya görevi seç.";return}
                let channel=channelMap[key],cal=config.calibration[channel],hall=config.hall[key]
                guard hall.mode == "Geleneksel" || hall.mode == "Rapid Trigger" else {hidWriteError="KEY \(key): desteklenmeyen tetikleme modu.";return}
                guard let free=cal.free,let pressed=cal.pressed,(0...4095).contains(free),(0...4095).contains(pressed),abs(free-pressed)>=30 else {hidWriteError="Layer \(l+1), KEY \(key): serbest ve tam basılı kalibrasyonunu yap.";return}
                let point=Int(hall.point.rounded()),hyst=Int(hall.hysteresis.rounded());guard (1...100).contains(point),hyst>=0,hyst<point else {hidWriteError="KEY \(key): tetikleme eşiğini ve histerezisi kontrol et.";return}
                var second:(UInt8,UInt8)=(0,0);var point2=0
                if behavior.mode == "twoStage" {
                    guard !behavior.secondAction.isEmpty,let a=assignment(behavior.secondaryKey,behavior.secondAction,layerConfig.taskProfile) else {hidWriteError="Layer \(l+1), KEY \(key): ikinci kademe için desteklenen bir görev seç.";return}
                    second=a;point2=Int(behavior.secondPoint.rounded());guard point2>point && point2<=100 else {hidWriteError="Layer \(l+1), KEY \(key): ikinci eşik ilk eşikten yüksek olmalı.";return}
                }
                var flags:UInt8=0,rt:[UInt8]=[0,0,0,0]
                if hall.mode == "Rapid Trigger" {
                    let press=Int(hall.press.rounded()),release=Int(hall.release.rounded()),minimum=Int(hall.min.rounded()),maximum=Int(hall.max.rounded())
                    guard (1...100).contains(press),(1...100).contains(release),(0...100).contains(minimum),(0...100).contains(maximum),minimum<maximum else {hidWriteError="KEY \(key): Rapid Trigger hassasiyetlerini ve sınırlarını kontrol et.";return}
                    flags=0x40;rt=[UInt8(press),UInt8(release),UInt8(minimum),UInt8(maximum)]
                }
                let offset=channel*16
                bytes.replaceSubrange(offset..<offset+16,with:[UInt8(free&255),UInt8(free>>8),UInt8(pressed&255),UInt8(pressed>>8),first.0,first.1,UInt8(point),UInt8(hyst),second.0,second.1,UInt8(point2),flags]+rt)
            }
            if let socd=layerConfig.socdSettings, socd.enabled {
                guard (0..<12).contains(socd.a),(0..<12).contains(socd.b),socd.a != socd.b else {hidWriteError="Layer \(l+1): SOCD için iki farklı tuş seç.";return}
                let a=channelMap[socd.a],b=channelMap[socd.b]
                guard bytes[a*16+4] != 0,bytes[b*16+4] != 0 else {hidWriteError="Layer \(l+1): SOCD tuşlarının ikisine de görev ata.";return}
                bytes[a*16+11] |= 0x80 | UInt8(b);bytes[b*16+11] |= 0x80 | UInt8(a)
            }
        }
        if bleControlReady && !freshSensor {
            hidWriteError=nil;hidWriteStatus=HIDWriteStatus(state:"writing",message:"BLE")
            bleStudio?.writeLayer(targetLayer,bytes:bytes){[weak self] ok in
                self?.hidWriteStatus=HIDWriteStatus(state:ok ? "saved":"error",message:ok ? "BLE · C6’ya kaydedildi" : "BLE yazımı başarısız; bağlantıyı kontrol et")
            };return
        }
        guard let body=try? JSONSerialization.data(withJSONObject:["layer":targetLayer,"data":bytes.map{String(format:"%02x",$0)}.joined()] as [String:Any]) else {hidWriteError="Profil hazırlanamadı.";return}
        hidWriteError=nil;hidWriteStatus=HIDWriteStatus(state:"writing",message:nil)
        var request=URLRequest(url:URL(string:"http://127.0.0.1:18768/hid-write")!);request.httpMethod="POST";request.httpBody=body;request.setValue("application/json",forHTTPHeaderField:"Content-Type");request.timeoutInterval=4
        URLSession.shared.dataTask(with:request){[weak self] _,response,error in DispatchQueue.main.async{guard let self=self else{return};if error != nil || (response as? HTTPURLResponse)?.statusCode != 202 {self.hidWriteStatus=HIDWriteStatus(state:"error",message:"Yazma başlatılamadı; S3 bağlantısını kontrol et.")}}}.resume()
    }
    @Published private(set) var sensorSerialPort:String? = nil
    @Published private(set) var rawValues:[Int]? = nil
    private var previewFree:[Int]? = nil
    var channelMap:[Int] {
        guard let map=config.testChannelMap, map.count == 12, Set(map) == Set(0..<12) else {return testChannelForKey}
        return map
    }
    func assignChannel(_ channel:Int,toKey key:Int) {
        guard (0..<12).contains(channel),(0..<12).contains(key) else{return}
        var map=channelMap
        guard let other=map.firstIndex(of:channel) else{return}
        map.swapAt(key,other)
        config.testChannelMap=map
        if let raw=rawValues {values=map.map{raw[$0]}}
        history=[];ranges=Array(repeating:nil,count:12);lightPressed=Array(repeating:nil,count:12)
    }
    @Published var ranges:[ClosedRange<Int>?] = Array(repeating:nil,count:12)
    @Published var history:[Point]=[]
    @Published var error:String? = nil
    @Published var selected=4 { didSet { history=[] } }
    private var applyingDeviceLayer=false
    @Published var layer=0 {didSet {
        if layer != oldValue && !applyingDeviceLayer {
            if bleControlReady && !freshSensor {blePendingLayer=layer;syncBLELayer()}
            else if deviceLayer != nil {postLightControl(["kind":"layer","layer":layer])}
        }
    }}
    private func syncBLELayer(){
        guard let desired=blePendingLayer,bleControlReady,let ble=bleStudio else{return}
        ble.selectLayer(desired){[weak self] ok in if ok && self?.blePendingLayer==desired {self?.blePendingLayer=nil}}
    }
    @Published var deviceLayer:DeviceLayer?=nil
    private var lastLightingSync:TimeInterval=0
    private func postLightControl(_ value:[String:Any]) {
        guard let data=try? JSONSerialization.data(withJSONObject:value) else{return}
        var request=URLRequest(url:URL(string:"http://127.0.0.1:18768/light-control")!);request.httpMethod="POST";request.httpBody=data;request.setValue("application/json",forHTTPHeaderField:"Content-Type");request.timeoutInterval=1
        URLSession.shared.dataTask(with:request){_,_,_ in}.resume()
    }
    private func syncLighting() {
        let now=ProcessInfo.processInfo.systemUptime
        guard now-lastLightingSync>=1.0 else{return};lastLightingSync=now
        let effects=["Kapalı":0,"Sabit":1,"Nefes":2,"Dalga":3,"Gökkuşağı":4,"Kayan ışık":5,"Tepkisel":6,"Basılan yerden dalga":7,"Starlight":8,"Yılan":9]
        var profiles:[[Int]]=[]
        for entry in config.layers {
            let light=entry.light
            guard let effect=effects[light.baseEffect],light.color.count==7,light.color.first=="#",let rgb=Int(light.color.dropFirst(),radix:16) else{return}
            let flags=(light.colorful ? 1:0)+((light.baseEffect == "Kayan ışık" && light.colorful && light.sweepColorStyle == "tur") ? 4:0)
            profiles.append([effect,(rgb>>16)&255,(rgb>>8)&255,rgb&255,Int(max(0,min(100,min(config.brightness,light.brightness))).rounded()),Int(max(1,min(100,light.speed)).rounded()),flags])
        }
        if bleControlReady && !freshSensor {
            guard profiles != bleLastLights,!bleLightInFlight,let ble=bleStudio else{return}
            bleLightInFlight=true
            ble.sendLights(profiles){[weak self] ok in self?.bleLightInFlight=false;if ok {self?.bleLastLights=profiles}}
            return
        }
        guard let data=try? JSONSerialization.data(withJSONObject:profiles) else{return}
        var request=URLRequest(url:URL(string:"http://127.0.0.1:18768/lighting")!);request.httpMethod="POST";request.httpBody=data;request.setValue("application/json",forHTTPHeaderField:"Content-Type");request.timeoutInterval=1
        URLSession.shared.dataTask(with:request){_,_,_ in}.resume()
    }
    @Published var page="Canlı test"
    @Published var simulate=false
    @Published var demoDepth=0.0 { didSet { if simulate && oldValue < 10 && demoDepth >= 10 { triggerLight(selected) };updateLightPeak() } }
    @Published var lightingPreview=true
    var lightingPreviewActive:Bool {page == "Aydınlatma" || lightingPreview}
    @Published private(set) var previewPressedKeys:Set<Int> = []
    private var previewPressIDs:[Int:Int] = [:]
    @Published var lightHit:LightHit? = nil
    @Published private(set) var keyLightHits:[LightHit?]=Array(repeating:nil,count:12)
    @Published private(set) var lightWaves:[LightWave]=[]
    let lightingClock=LightingClock()
    private var lightPeak=0.0
    private var lightingTimer:Timer?
    private var lightHitID=0
    private var keyLightHues:[Double]=Array(repeating:0,count:12)
    private var lightPressed:[Bool?] = Array(repeating:nil,count:12)
    func triggerLight(_ key:Int) {
        let now=Date.timeIntervalSinceReferenceDate
        guard (0..<12).contains(key) else{return}
        lightPeak=(lightingPercent(key) ?? 0)/100;lightHitID += 1
        keyLightHues[key]=(Double(lightHitID)*0.61803398875).truncatingRemainder(dividingBy:1)
        let hit=LightHit(id:lightHitID,key:key,time:now);keyLightHits[key]=hit;lightHit=hit
        let light=config.layers[layer].light
        if light.baseEffect == "Basılan yerden dalga" {
            let duration=max(0.3,3.0-light.speed*0.025)
            lightWaves.removeAll {now-$0.time > $0.duration*1.1}
            if lightWaves.count >= 64 {lightWaves.removeFirst()}
            let hue=(Double(lightHitID)*0.61803398875).truncatingRemainder(dividingBy:1)
            lightWaves.append(LightWave(key:key,time:now,duration:duration,hue:hue,peak:lightPeak,held:lightPeak>0.05))
        }
    }
    func nextLayer() { layer=(layer+1)%4 }
    func selectKey(_ key:Int) {
        selected=key; triggerLight(key)
        guard page == "Aydınlatma" || (page == "Canlı test" && !simulate) else{return}
        if deviceLayer?.layer == layer {postLightControl(["kind":"hit","key":key,"layer":layer,"id":lightHitID])}
        let id=(previewPressIDs[key] ?? 0)+1
        previewPressIDs[key]=id; previewPressedKeys.insert(key)
        DispatchQueue.main.asyncAfter(deadline:.now()+0.18) { [weak self] in
            guard let self=self, self.previewPressIDs[key] == id else{return}
            self.previewPressedKeys.remove(key)
        }
    }
    func displayedPercent(_ key:Int)->Double {
        let actual=(page == "Aydınlatma" ? (percent(key) ?? estimatedDepth(key)) : (visualPercent(key) ?? estimatedDepth(key))) ?? 0
        return previewPressedKeys.contains(key) && (page == "Aydınlatma" || (page == "Canlı test" && !simulate)) ? max(actual,100) : actual
    }
    // Keep lighting highlights aligned with the same press/release hysteresis
    // used by reactive effects, so idle ADC noise is not shown as a press.
    func lightingKeyHighlighted(_ key:Int)->Bool {
        guard (0..<lightPressed.count).contains(key) else{return false}
        return previewPressedKeys.contains(key) || lightPressed[key] == true
    }
    // Only drives the model while full calibration is unavailable.
    // This estimates motion from the highest observed free value, not mm or calibrated depth.
    func estimatedDepth(_ key:Int)->Double? {
        guard let v=values?[key],let free=previewFree?[channelMap[key]],free >= 100 else{return nil}
        let deadband=max(15,Double(free)*0.02)
        return max(0,min(100,(Double(free-v)-deadband)/max(1,Double(free)-deadband)*100))
    }
    func visualPercent(_ i:Int)->Double? { simulate ? (i == selected ? demoDepth : 0) : percent(i) }
    func lightingPercent(_ i:Int)->Double? {page == "Aydınlatma" ? percent(i) : visualPercent(i)}
    private func updateLightPeak(){
        if let hit=lightHit {lightPeak=max(lightPeak,(lightingPercent(hit.key) ?? 0)/100)}
        for index in lightWaves.indices where lightWaves[index].held {
            let depth=(lightingPercent(lightWaves[index].key) ?? 0)/100
            if depth<=0.05 {lightWaves[index].held=false} else {lightWaves[index].peak=max(lightWaves[index].peak,depth)}
        }
    }
    private func waveContribution(_ wave:LightWave,key i:Int,at time:TimeInterval,sensitive:Bool)->Double {
        let dx=Double(i/4-wave.key/4),dy=Double(i%4-wave.key%4)
        let elapsed=time-wave.time-sqrt(dx*dx+dy*dy)*wave.duration*0.16
        guard elapsed>=0 else{return 0}
        return (sensitive ? min(1,wave.peak) : 1)*max(0,1-elapsed/(wave.duration*0.45))
    }
    func waveLight(_ i:Int,at time:TimeInterval,sensitive:Bool)->Double {
        guard lightingPreviewActive else{return 0}
        return min(1,lightWaves.reduce(0.0) { $0+waveContribution($1,key:i,at:time,sensitive:sensitive) })
    }
    func waveColor(_ i:Int,at time:TimeInterval,sensitive:Bool)->NSColor {
        var red=0.0,green=0.0,blue=0.0,total=0.0
        for wave in lightWaves {
            let weight=waveContribution(wave,key:i,at:time,sensitive:sensitive)
            guard weight>0 else{continue}
            let c=NSColor(calibratedHue:wave.hue,saturation:0.8,brightness:1,alpha:1).usingColorSpace(.sRGB)!
            red+=Double(c.redComponent)*weight;green+=Double(c.greenComponent)*weight;blue+=Double(c.blueComponent)*weight;total+=weight
        }
        guard total>0 else{return .black}
        return NSColor(srgbRed:red/total,green:green/total,blue:blue/total,alpha:1)
    }
    func sweepColor(_ i:Int,at time:TimeInterval,duration:Double)->NSColor {
        let light=config.layers[layer].light
        let hue=light.sweepColorStyle == "tur" ? (SweepTurnTiming.cycle(at:time,duration:duration)*0.61803398875).truncatingRemainder(dividingBy:1) : (time/duration/6+Double(i)/12).truncatingRemainder(dividingBy:1)
        return NSColor(calibratedHue:hue,saturation:0.8,brightness:1,alpha:1)
    }
    func reactiveColor(_ i:Int)->NSColor {NSColor(calibratedHue:keyLightHues[i],saturation:0.8,brightness:1,alpha:1)}
    func rainbowColor(_ i:Int,at time:TimeInterval,duration:Double)->NSColor {
        let hue=(time/(duration*6)+Double(i)/12).truncatingRemainder(dividingBy:1)
        return NSColor(calibratedHue:hue,saturation:0.75,brightness:1,alpha:1)
    }
    func cycleColor(_ i:Int,at time:TimeInterval,duration:Double,wave:Bool)->NSColor {
        // The pulse reaches its minimum at phase 0.5; wave keys have a phase delay.
        let delay=wave ? Double(i)*duration/18 : 0
        let cycle=floor((time-delay)/duration+0.5)
        let hue=((cycle*0.61803398875).truncatingRemainder(dividingBy:1)+1).truncatingRemainder(dividingBy:1)
        return NSColor(calibratedHue:hue,saturation:0.8,brightness:1,alpha:1)
    }
    func pulseIntensity(_ i:Int,at time:TimeInterval,duration:Double,wave:Bool)->Double {
        let delay=wave ? Double(i)*duration/18 : 0
        let phase=((time-delay).truncatingRemainder(dividingBy:duration)+duration).truncatingRemainder(dividingBy:duration)/duration
        return abs(2*phase-1)
    }
    func snakeIntensity(_ key:Int,at time:TimeInterval,duration:Double)->Double {
        let order=[0,1,2,3,7,6,5,4,8,9,10,11]
        guard let index=order.firstIndex(of:key) else{return 0}
        let phase=(time/(duration/4)).truncatingRemainder(dividingBy:15)
        return max(0,1-abs((phase-2)-Double(index))/2)
    }
    func snakeColor(at time:TimeInterval,duration:Double)->NSColor {
        let cycle=floor(time/(duration*15/4))
        return NSColor(calibratedHue:CGFloat((cycle*0.61803398875).truncatingRemainder(dividingBy:1)),saturation:0.75,brightness:1,alpha:1)
    }
    private func starlightKeys(cycle:UInt64)->[Int] {
        // Each checkerboard group has no edge-adjacent keys; alternating
        // groups also prevents the next flash from reusing the same keys.
        let groups=[[0,2,5,7,8,10],[1,3,4,6,9,11]]
        let candidates=groups[Int(cycle % 2)]
        var hash=cycle &+ 0x9E3779B97F4A7C15
        hash=(hash ^ (hash >> 30)) &* 0xBF58476D1CE4E5B9
        hash=(hash ^ (hash >> 27)) &* 0x94D049BB133111EB
        hash ^= hash >> 31
        var index=Int(hash % 15)
        var excluded=(0,1)
        for first in 0..<5 {
            let count=5-first
            if index < count {excluded=(first,first+1+index);break}
            index-=count
        }
        return candidates.enumerated().compactMap { i,key in i == excluded.0 || i == excluded.1 ? nil : key }
    }
    func starlightColor(_ key:Int,at time:TimeInterval,duration:Double)->NSColor {
        let cycle=floor(time/duration)
        let keys=starlightKeys(cycle:UInt64(max(0,cycle)))
        let offset=Double(keys.firstIndex(of:key) ?? 0)*0.25
        let hue=(cycle*0.61803398875+offset).truncatingRemainder(dividingBy:1)
        return NSColor(calibratedHue:CGFloat(hue),saturation:0.75,brightness:1,alpha:1)
    }
    func starlightIntensity(_ key:Int,at time:TimeInterval,duration:Double)->Double {
        let t=time/duration
        let keys=starlightKeys(cycle:UInt64(max(0,floor(t))))
        guard let index=keys.firstIndex(of:key) else{return 0}
        let phase=t-floor(t)
        let progress=(phase-Double(index)*0.08)/0.72
        guard progress > 0 && progress < 1 else{return 0}
        return (1-cos(2 * Double.pi * progress))/2
    }
    func sweepIntensity(_ i:Int,at time:TimeInterval,duration:Double)->Double {
        let light=config.layers[layer].light
        if light.colorful && light.sweepColorStyle == "tur" {return SweepTurnTiming.intensity(key:i,at:time,duration:duration)}
        let phase=((time/duration-Double(i)/12).truncatingRemainder(dividingBy:1)+1).truncatingRemainder(dividingBy:1)
        return max(0.05,1-abs(phase-0.18)/0.18)
    }
    func pressureLight(_ i:Int,at time:TimeInterval)->Double {
        let light=config.layers[layer].light
        guard lightingPreviewActive else{return 0}
        if light.effectiveEffect == "Derinliğe duyarlı" {return min(1,max(0,(lightingPercent(i) ?? 0)/100))}
        guard light.effectiveEffect == "Derinliğe duyarlı dalga" else{return 0}
        return waveLight(i,at:time,sensitive:true)
    }
    var effectiveLightBrightness:Double { min(config.brightness,config.layers[layer].light.brightness) }
    var task:Process?; var timer:Timer?; var busy=false; var sampleID=0
    @Published var wizardActive=false
    @Published var wizardStep=0
    @Published var wizardMessage=""
    @Published var wizardFree:[Int]? = nil
    @Published var wizardPressed:[Int?]=Array(repeating:nil,count:12)
    @Published var sensorCapture:SensorCapture? = nil
    @Published var noiseResult:SensorNoiseResult? = nil
    @Published var noiseMessage=""
    @Published private(set) var triggerStates=Array(repeating:false,count:12)
    private var triggerMemory=Array(repeating:LocalTrigger(),count:12)
    private var triggerHall:[HallConfig]? = nil
    private var localSOCD=LocalSOCD()
    private var socdLayer:Int? = nil
    private var lastSOCD:SOCDSettings? = nil
    private var triggerMap:[Int]? = nil
    private var wizardMap:[Int]? = nil
    static let physicalOrder=(0..<12).map{($0%3)*4+$0/3}
    var freshSensor:Bool {values != nil && lastSensorSampleID != nil && ProcessInfo.processInfo.systemUptime-lastSensorSampleAt < 0.3}
    var wizardAllReleased:Bool {
        guard freshSensor,let raw=rawValues,let free=wizardFree,wizardPressed.allSatisfy({$0 != nil}) else{return false}
        return (0..<12).allSatisfy { channel in
            let span=abs(free[channel]-wizardPressed[channel]!)
            return abs(raw[channel]-free[channel]) <= max(50,span/20)
        }
    }
    private var nextPollTime:TimeInterval=0
    private var captureSampleID:Int?
    private var lastSensorSampleID:Int?
    private var lastSensorSampleAt:TimeInterval=0
    init() {
        if let d=UserDefaults.standard.data(forKey:"native-config-v1"), let c=try? JSONDecoder().decode(Config.self,from:d), c.layers.count==4, c.hall.count==12, c.calibration.count==12 { config=c } else { config=Config() }
        let start=UserDefaults.standard.string(forKey:"studio-start-page") ?? "Canlı test"
        if ["Canlı test","Katmanlar","Oyun profili","Hall ayarları","Aydınlatma","Güç ve pil","Gelişmiş"].contains(start) {page=start}
        migrateLegacyGame()
        startServer()
        bluetoothHIDPresent=wirelessHEBluetoothHIDPresent()
        let ble=BLEStudio();bleStudio=ble
        ble.slotStatus={ [weak self] index,mask,deviceLayer in
            guard let self=self else{return};self.bleSlotIndex=index;self.bleSlotMask=mask
            if self.blePendingLayer == nil && (0..<4).contains(deviceLayer) && self.layer != deviceLayer {
                self.applyingDeviceLayer=true;self.layer=deviceLayer;self.applyingDeviceLayer=false
            }
        }
        ble.changed={ [weak self] ready in
            guard let self=self else{return};self.bleControlReady=ready;self.bleLastLights=[];if !ready {self.bleSlotIndex=nil;self.bleSlotMask=nil}
            if ready {self.blePendingLayer=nil}
        }
        ble.setEnabled(bluetoothHIDPresent)
        bluetoothStatusTimer=Timer.scheduledTimer(withTimeInterval:2.0,repeats:true){[weak self] _ in
            let present=wirelessHEBluetoothHIDPresent()
            if self?.bluetoothHIDPresent != present {self?.bluetoothHIDPresent=present;self?.bleStudio?.setEnabled(present)}
            guard let self=self else{return}
            if self.bleControlReady && !self.freshSensor {self.syncBLELayer();self.syncLighting()}
        }
        lightingTimer=Timer.scheduledTimer(withTimeInterval:1.0/24.0,repeats:true){[weak self] _ in
            guard let self=self else{return}
            guard NSApp.mainWindow?.occlusionState.contains(.visible) == true else{return}
            let now=Date.timeIntervalSinceReferenceDate
            let hadWaves = !self.lightWaves.isEmpty
            if hadWaves && self.lightWaves.contains(where:{now-$0.time > $0.duration*1.1}) {self.lightWaves.removeAll {now-$0.time > $0.duration*1.1}}
            let light=self.config.layers[self.layer].light
            let hitDuration=max(0.3,3.0-light.speed*0.025)*0.45
            if self.keyLightHits.contains(where:{hit in hit.map{now-$0.time > hitDuration} ?? false}) {
                self.keyLightHits=self.keyLightHits.map { hit in
                    guard let hit=hit, now-hit.time <= hitDuration else{return nil}
                    return hit
                }
            }
            let animated=self.lightingPreviewActive && ["Kayan ışık","Nefes","Dalga","Gökkuşağı","Starlight","Yılan"].contains(light.baseEffect) && ["Canlı test","Aydınlatma","Katmanlar"].contains(self.page)
            let reactive=self.lightingPreviewActive && light.effectiveEffect == "Tepkisel" && self.keyLightHits.contains{$0 != nil}
            if hadWaves || animated || reactive {self.lightingClock.tick &+= 1}
        }
    }
    func migrateLegacyGame() {
        let d=UserDefaults.standard
        var game=GameSettings()
        if d.object(forKey:"game-w") != nil {game.forward=d.integer(forKey:"game-w")}
        if d.object(forKey:"game-a") != nil {game.left=d.integer(forKey:"game-a")}
        if d.object(forKey:"game-s") != nil {game.back=d.integer(forKey:"game-s")}
        if d.object(forKey:"game-d") != nil {game.right=d.integer(forKey:"game-d")}
        if d.object(forKey:"game-deadzone") != nil {game.deadzone=d.double(forKey:"game-deadzone")}
        if d.object(forKey:"game-run") != nil {game.runPoint=d.double(forKey:"game-run")}
        var c=config
        for i in c.layers.indices { if c.layers[i].game == nil {c.layers[i].game=game}; if c.layers[i].socdSettings == nil {c.layers[i].socdSettings=SOCDSettings(enabled:c.socd,a:c.socdA,b:c.socdB)} }
        config=c
    }
    func startServer() {
        guard let project=Bundle.main.object(forInfoDictionaryKey:"TestProjectPath") as? String, let python=Bundle.main.object(forInfoDictionaryKey:"TestPythonPath") as? String else { error="Test sunucusunun yolu bulunamadı."; return }
        let p=Process(); p.executableURL=URL(fileURLWithPath:python)
        let serverPath=URL(fileURLWithPath:project).appendingPathComponent("server.py").path
        var arguments=["-u",serverPath,"--http-port","18768","--studio-heartbeat"]
        let port=UserDefaults.standard.string(forKey:"test-serial-port") ?? ""
        if port.hasPrefix("/dev/cu.usbmodem") || port.hasPrefix("/dev/cu.usbserial") {arguments += ["--port",port]}
        p.arguments=arguments; p.currentDirectoryURL=URL(fileURLWithPath:"/tmp"); p.standardOutput=FileHandle.nullDevice; p.standardError=FileHandle.nullDevice
        do { try p.run(); task=p } catch { self.error="Test sunucusu başlatılamadı: \(error.localizedDescription)"; return }
        timer=Timer.scheduledTimer(withTimeInterval:0.02,repeats:true) { [weak self] _ in self?.poll() }
    }
    func stop() { macOutputEnabled=false;driveOutputEnabled=false;timer?.invalidate();lightingTimer?.invalidate();bluetoothStatusTimer?.invalidate(); if let p=task,p.isRunning { p.interrupt() } }
    func poll() {
        if lastSensorSampleID != nil && ProcessInfo.processInfo.systemUptime-lastSensorSampleAt >= 0.3 && triggerStates.contains(true) {resetLocalTriggers()}
        if macOutputEnabled && ProcessInfo.processInfo.systemUptime-lastSensorSampleAt >= 0.3 {
            macKeyOutput.releaseAll();macPressed=Array(repeating:false,count:12)
        }
        if sensorCapture != nil && ProcessInfo.processInfo.systemUptime-lastSensorSampleAt >= 1 {cancelSensorCapture("Yeni örnek gelmedi; ölçüm kaydedilmedi.")}
        guard !busy else{return}
        let now=ProcessInfo.processInfo.systemUptime
        guard now >= nextPollTime else{return}
        let visible=NSApp.mainWindow?.occlusionState.contains(.visible) == true
        nextPollTime=now + ((values == nil || (!visible && !driveOutputEnabled && !macOutputEnabled)) ? 0.25 : 0.02)
        if let p=task, !p.isRunning { error="Yerel sunucu çalışmıyor. 18768 portunu kontrol edip uygulamayı yeniden açın."; values=nil;rateStatus=nil;bridgeStatus=nil; return }
        busy=true
        var req=URLRequest(url:URL(string:"http://127.0.0.1:18768/state")!); req.cachePolicy = .reloadIgnoringLocalCacheData; req.timeoutInterval=1
        URLSession.shared.dataTask(with:req) { [weak self] data,_,_ in
            let packet=data.flatMap { try? JSONDecoder().decode(Packet.self,from:$0) }
            DispatchQueue.main.async { [weak self] in
                guard let self=self else{return}; self.busy=false
                let rate=packet?.rate_status;let bridge=packet?.bridge_status
                if !self.bleControlReady,let status=packet?.hid_write_status,status.state != "idle",status != self.hidWriteStatus {self.hidWriteStatus=status}
                if self.rateStatus != rate {self.rateStatus=rate}
                if self.bridgeStatus != bridge {self.bridgeStatus=bridge}
                let deviceLayer=packet?.connected == true ? packet?.layer_status : nil
                if self.deviceLayer != deviceLayer {
                    let previous=self.deviceLayer;self.deviceLayer=deviceLayer
                    if let deviceLayer=deviceLayer,(0..<4).contains(deviceLayer.layer),previous?.layer != deviceLayer.layer {self.applyingDeviceLayer=true;self.layer=deviceLayer.layer;self.applyingDeviceLayer=false}
                }
                guard let packet=packet,packet.connected,let rawValues=packet.values,rawValues.count==12 else { if self.sensorCapture != nil {self.cancelSensorCapture("Bağlantı kesildi; ölçüm kaydedilmedi.")}; if self.values != nil {self.history=[];self.ranges=Array(repeating:nil,count:12);self.values=nil;self.rawValues=nil;self.previewFree=nil;self.lightPressed=Array(repeating:nil,count:12)};if self.sensorSerialPort != nil {self.sensorSerialPort=nil};self.resetLocalTriggers();self.macKeyOutput.releaseAll();self.updateDrivingOutput();return }
                self.syncLighting()
                if self.sensorSerialPort != packet.port {self.sensorSerialPort=packet.port}
                if let sample=packet.sample_id, sample != self.lastSensorSampleID {
                    self.lastSensorSampleID=sample;self.lastSensorSampleAt=ProcessInfo.processInfo.systemUptime
                }
                if let sample=packet.sample_id, sample != self.captureSampleID { self.captureSampleID=sample;self.receiveSensorCapture(rawValues) }
                self.rawValues=rawValues
                if let baseline=self.previewFree {self.previewFree=zip(baseline,rawValues).map{max($0,$1)}}
                else {self.previewFree=rawValues}
                let v=self.channelMap.map {rawValues[$0]}
                self.values=v
                self.updateLocalTriggers()
                self.updateDrivingOutput()
                self.updateMacOutput()
                if !self.simulate || self.page == "Aydınlatma" { for i in 0..<12 { guard let p=self.percent(i) ?? self.estimatedDepth(i) else {self.lightPressed[i]=nil;continue}; let previous=self.lightPressed[i]; let pressed=previous == true ? p > 5 : p >= 10; self.lightPressed[i]=pressed; if previous != true && pressed {self.triggerLight(i);if self.deviceLayer?.layer == self.layer {self.postLightControl(["kind":"physical_hit","key":i,"layer":self.layer,"id":self.lightHitID])}} } }
                self.updateLightPeak()
                for (i,n) in v.enumerated() { if let r=self.ranges[i] { self.ranges[i]=Swift.min(r.lowerBound,n)...Swift.max(r.upperBound,n) } else { self.ranges[i]=n...n } }
                self.sampleID+=1; self.history.append(Point(id:self.sampleID,value:Double(v[self.selected]))); if self.history.count>120 {self.history.removeFirst()}
            }
        }.resume()
    }
    func resetLocalTriggers(){triggerMemory=Array(repeating:LocalTrigger(),count:12);triggerStates=Array(repeating:false,count:12);localSOCD=LocalSOCD();socdLayer=nil}
    func updateLocalTriggers() {
        guard freshSensor else {resetLocalTriggers();return}
        if triggerHall != config.hall || triggerMap != channelMap || socdLayer != layer {resetLocalTriggers();triggerHall=config.hall;triggerMap=channelMap;socdLayer=layer}
        let previous=triggerMemory.map{$0.pressed}
        for i in 0..<12 {if let p=percent(i) {_ = triggerMemory[i].update(p,config.hall[i])} else {triggerMemory[i]=LocalTrigger()}}
        var states=triggerMemory.map{$0.pressed}
        let pair=config.layers[layer].socdSettings ?? SOCDSettings(enabled:config.socd,a:config.socdA,b:config.socdB)
        if lastSOCD != pair {localSOCD=LocalSOCD();lastSOCD=pair}
        if pair.enabled,(0..<12).contains(pair.a),(0..<12).contains(pair.b),pair.a != pair.b {
            localSOCD.resolve(&states,previous:previous,a:pair.a,b:pair.b)
        } else {localSOCD=LocalSOCD()}
        if triggerStates != states {triggerStates=states}
    }
    func updateMacOutput() {
        guard macOutputEnabled, !wizardActive, AXIsProcessTrusted(), lastSensorSampleID != nil,
              ProcessInfo.processInfo.systemUptime-lastSensorSampleAt < 0.3 else {macKeyOutput.releaseAll();macPressed=Array(repeating:false,count:12);return}
        let layerConfig=config.layers[layer]
        let behaviors=layerConfig.keyBehaviors?.count == 12 ? layerConfig.keyBehaviors! : Array(repeating:KeyBehavior(),count:12)
        var wanted:Set<MacKeyOutput.Key>=[]
        var media:Set<Int>=[]
        for i in 0..<12 {
            guard let amount=percent(i) else {macPressed[i]=false;continue}
            let threshold=max(1,min(100,config.hall[i].point))
            macPressed[i]=triggerStates[i]
            if macPressed[i], let key=MacKeyOutput.resolve(behaviors[i].primaryKey,layerConfig.keys[i]) {wanted.insert(key)}
            if macPressed[i], behaviors[i].primaryKey == nil, MacKeyOutput.resolve(nil,layerConfig.keys[i]) == nil, MacKeyOutput.profileIsFrontmost(layerConfig.taskProfile), let key=MacKeyOutput.presetKey(layerConfig.keys[i],profile:layerConfig.taskProfile) {wanted.insert(key)}
            if macPressed[i], behaviors[i].primaryKey == nil, MacKeyOutput.presetKey(layerConfig.keys[i],profile:layerConfig.taskProfile) == nil, let code=MacKeyOutput.media[layerConfig.keys[i]] {media.insert(code)}
            if macPressed[i], behaviors[i].mode == "twoStage", amount >= max(threshold,behaviors[i].secondPoint),
               let key=MacKeyOutput.resolve(behaviors[i].secondaryKey,behaviors[i].secondAction) {wanted.insert(key)}
            if macPressed[i], behaviors[i].mode == "twoStage", amount >= max(threshold,behaviors[i].secondPoint), behaviors[i].secondaryKey == nil, let code=MacKeyOutput.media[behaviors[i].secondAction] {media.insert(code)}
        }
        macKeyOutput.update(wanted)
        macKeyOutput.updateMedia(media)
    }
    func updateDrivingOutput() {
        guard driveOutputEnabled else{return}
        let game=config.layers[layer].game ?? GameSettings()
        let keys=[game.forward,game.left,game.back,game.right]
        let fresh=lastSensorSampleID != nil && ProcessInfo.processInfo.systemUptime-lastSensorSampleAt < 0.3
        let valid=fresh && !wizardActive && game.enabled && game.kind == "driving" && Set(keys).count == 4 && keys.allSatisfy{(0..<12).contains($0)}
        let amounts=valid ? keys.compactMap{percent($0)} : []
        guard amounts.count == 4 else {
            let linked=driveBridge.send(steering:0,throttle:0,brake:0)
            let status=linked ? "Sensör ve kalibrasyon bekleniyor" : "CrossOver köprüsü bekleniyor"
            if driveBridgeStatus != status {driveBridgeStatus=status};return
        }
        let raw=(amounts[3]-amounts[1])/100
        let zone=min(0.5,max(0,game.deadzone/100))
        let steer=abs(raw)<=zone ? 0 : (raw < 0 ? -1.0 : 1.0)*min(1,(abs(raw)-zone)/(1-zone))
        let linked=driveBridge.send(steering:steer,throttle:amounts[0]/100,brake:amounts[2]/100)
        let status=linked ? "Köprüye canlı veri gönderiliyor" : "CrossOver köprüsü bekleniyor"
        if driveBridgeStatus != status {driveBridgeStatus=status}
    }
    func percent(_ i:Int)->Double? {
        guard (0..<12).contains(i),let v=values?[i],let a=config.calibration[channelMap[i]].free,let b=config.calibration[channelMap[i]].pressed,abs(a-b)>=10 else{return nil}
        return SensorMath.percent(raw:v,free:a,pressed:b,exponent:curve(i))
    }
    func curve(_ i:Int)->Double {let values=config.responseCurves;guard (0..<12).contains(i),let values=values,values.count==12,values[channelMap[i]].isFinite else{return 1};return min(2.5,max(0.4,values[channelMap[i]]))}
    func setCurve(_ value:Double,all:Bool) {var c=config;var curves=c.responseCurves?.count == 12 ? c.responseCurves! : Array(repeating:1.0,count:12);let v=min(2.5,max(0.4,value));if all {curves=Array(repeating:v,count:12)} else {curves[channelMap[selected]]=v};c.responseCurves=curves;config=c}
    func startWizard() {guard freshSensor,sensorCapture == nil else{wizardMessage="Güncel sensör verisi bekleniyor.";return};wizardActive=true;macOutputEnabled=false;driveOutputEnabled=false;wizardStep=0;wizardFree=nil;wizardPressed=Array(repeating:nil,count:12);wizardMap=channelMap;wizardMessage="Tüm tuşları bırak, ardından serbest ölçümü başlat. Mac ve sürüş çıktıları kapatıldı; kalibrasyondan sonra yeniden açabilirsin."}
    func cancelWizard(){sensorCapture=nil;wizardActive=false;wizardMap=nil;wizardMessage="Kalibrasyon iptal edildi; eski değerler korundu."}
    func beginCapture(_ kind:SensorCapture.Kind) {
        guard freshSensor,sensorCapture == nil else {wizardMessage="Güncel sensör verisi bekleniyor.";return}
        guard kind == .noise || (wizardActive && wizardMap == channelMap) else {cancelWizard();wizardMessage="Kanal eşlemesi değişti; yeniden başla.";return}
        let key = kind == .pressed ? Self.physicalOrder[wizardStep] : nil
        sensorCapture=SensorCapture(kind:kind,started:ProcessInfo.processInfo.systemUptime,mapping:channelMap,key:key)
        if kind == .noise {noiseResult=nil;noiseMessage="Ölçülüyor: tuşlara dokunma."} else {wizardMessage="Ölçülüyor; konumu koru."}
    }
    func cancelSensorCapture(_ message:String){let kind=sensorCapture?.kind;sensorCapture=nil;if kind == .noise {noiseMessage=message} else {wizardMessage=message}}
    func receiveSensorCapture(_ raw:[Int]) {
        guard var capture=sensorCapture else{return}
        guard capture.mapping == channelMap else{cancelSensorCapture("Kanal eşlemesi değişti; ölçüm iptal edildi.");return}
        capture.samples.append(raw)
        let elapsed=ProcessInfo.processInfo.systemUptime-capture.started
        let duration = capture.kind == .noise ? 5.0 : 0.8
        guard elapsed >= duration else{sensorCapture=capture;return}
        sensorCapture=nil
        guard capture.samples.count >= 5 else {if capture.kind == .noise {noiseMessage="Yeterli örnek yok; tekrar dene."} else {wizardMessage="Yeterli örnek yok; tekrar dene."};return}
        let means=(0..<12).map { channel in Double(capture.samples.reduce(0){$0+$1[channel]})/Double(capture.samples.count) }
        if capture.kind == .noise {
            let spans=(0..<12).map { channel in let v=capture.samples.map{$0[channel]};return (v.max() ?? 0)-(v.min() ?? 0) }
            let deviations=(0..<12).map { channel in sqrt(capture.samples.reduce(0.0){$0+pow(Double($1[channel])-means[channel],2)}/Double(capture.samples.count)) }
            noiseResult=SensorNoiseResult(samples:capture.samples.count,elapsed:elapsed,spans:spans,deviations:deviations);noiseMessage="Tuşlar bırakılmışsa bu değerler boşta değişimi gösterir."
        } else if capture.kind == .free {
            let spans=(0..<12).map { channel in let v=capture.samples.map{$0[channel]};return (v.max() ?? 0)-(v.min() ?? 0) }
            guard spans.max() ?? 0 <= 50 else {wizardMessage="Serbest ölçüm çok değişken; tuşları bırakıp tekrar ölç.";return}
            wizardFree=means.map{Int($0.rounded())};wizardMessage="İstenen tuşa sonuna kadar basıp ölçümü başlat ve basılı tut."
        } else if let key=capture.key,let free=wizardFree {
            let channel=capture.mapping[key];let value=Int(means[channel].rounded())
            let samples=capture.samples.map{$0[channel]};let span=(samples.max() ?? 0)-(samples.min() ?? 0)
            guard span <= 50 && abs(value-free[channel]) >= 30 else {wizardMessage="Basış yeterince sabit değil veya aralık çok küçük; tekrar ölç.";return}
            wizardPressed[channel]=value;wizardStep+=1
            wizardMessage=wizardStep == 12 ? "Ölçüm tamamlandı. Eski kalibrasyonu değiştirmek için Kaydet’e bas." : "Sıradaki tuşa basıp basılı tut."
        }
    }
    func saveWizard() {
        guard wizardActive,wizardStep==12,wizardMap==channelMap,wizardAllReleased,let free=wizardFree,wizardPressed.allSatisfy({$0 != nil}) else{return}
        var c=config;for channel in 0..<12 {c.calibration[channel]=Cal(free:free[channel],pressed:wizardPressed[channel]!)};config=c
        wizardActive=false;wizardMap=nil;wizardMessage="12 tuşun kalibrasyonu Mac’e kaydedildi."
    }

    func capture(_ pressed:Bool) { guard let v=values?[selected] else{return};if pressed {config.calibration[channelMap[selected]].pressed=v} else {config.calibration[channelMap[selected]].free=v} }
    func captureAll(){guard let v=values else{return};for i in 0..<12{config.calibration[channelMap[i]].free=v[i]}}
}

let studioGridIcon:NSImage = {
    guard let url=Bundle.main.url(forResource:"studio-grid",withExtension:"svg"),let image=NSImage(contentsOf:url) else{return NSImage()}
    return image
}()
let accent=Color(nsColor:NSColor(name:nil){appearance in appearance.bestMatch(from:[.darkAqua,.aqua]) == .darkAqua ? NSColor(calibratedRed:0.65,green:0.95,blue:0.80,alpha:1) : NSColor(calibratedRed:0.03,green:0.43,blue:0.31,alpha:1)})
let studioBackground=Color(nsColor:NSColor(name:nil){appearance in appearance.bestMatch(from:[.darkAqua,.aqua]) == .darkAqua ? NSColor(calibratedRed:0.055,green:0.075,blue:0.075,alpha:1) : NSColor(calibratedRed:0.94,green:0.96,blue:0.95,alpha:1)})
struct GlassCard<Content:View>:View {
    @ViewBuilder var content:Content
    var body:some View { if #available(macOS 26.0, *) { content.padding(22).frame(maxWidth:.infinity,alignment:.leading).background(Color.white.opacity(0.025),in:RoundedRectangle(cornerRadius:22)).glassEffect(.regular, in: .rect(cornerRadius:22)).overlay(RoundedRectangle(cornerRadius:22).strokeBorder(.white.opacity(0.07),lineWidth:1)) } else { content.padding(20).frame(maxWidth:.infinity,alignment:.leading).background(.regularMaterial,in:RoundedRectangle(cornerRadius:20)) } }
}
// Preserve model keys and user data; translate only strings presented by the UI.
private func studioDisplay(_ value:String)->String {
    guard UserDefaults.standard.string(forKey:"studio-language") == "en" else { return value }
    if let url=Bundle.main.url(forResource:"en",withExtension:"lproj"), let bundle=Bundle(url:url) {
        let translated=bundle.localizedString(forKey:value,value:nil,table:nil)
        if translated != value { return translated }
        if value.contains(" · ") {
            return value.components(separatedBy:" · ").map { studioDisplay($0) }.joined(separator:" · ")
        }
    }
    var result=value
    for (source,target) in [(". satır", " row"), (". sıra", " row"), (". sütun", " column"), ("Eşle · ADC", "Map · ADC"), ("Serbest:", "Released:"), ("Basılı:", "Pressed:"), ("sıra boşluğu", "sequence gaps"), ("geçersiz", "invalid"), ("örnek", "samples"), ("Seçili tuş tetik durumu:", "Selected key trigger:"), ("BASILI", "PRESSED"), ("SERBEST", "RELEASED"), ("Mac testi", "Mac test"), ("bağlı değil", "disconnected"), ("atanmış", "assigned"), ("genel üst sınır", "global limit"), ("Etkin:", "Effective:"), (" sn", " s"), ("ÖNİZLEME", "PREVIEW"), ("’e uygula", " apply"), ("Özel renk", "Custom color"), ("rengini kaldır", "remove color"), ("hareket miktarı", "movement amount"), ("direksiyon", "steering"), ("Gaz:", "Throttle:"), ("Fren:", "Brake:"), ("aktivasyon", "actuation")] {
        result=result.replacingOccurrences(of:source,with:target)
    }
    return result
}
struct SectionTitle:View { let title:String; var subtitle=""; var body:some View { VStack(alignment:.leading,spacing:4){Text(LocalizedStringKey(title)).font(.headline);if !subtitle.isEmpty {Text(verbatim:studioDisplay(subtitle)).font(.caption).foregroundStyle(.secondary)}} } }
struct RangeControl:View { let label:String; @Binding var value:Double; var bounds:ClosedRange<Double>=0...100; var unit="%"; var body:some View { VStack(alignment:.leading,spacing:6){HStack{Text(LocalizedStringKey(label));Spacer();Text("\(Int(value))\(unit)").foregroundStyle(accent).monospacedDigit()}.font(.subheadline);Slider(value:$value,in:bounds,step:1).tint(accent).accessibilityLabel(LocalizedStringKey(label)).accessibilityValue("\(Int(value))\(unit)")} } }
struct MainView:View {
    @ObservedObject var model:StudioModel
    @AppStorage("studio-appearance") private var appearance="system"
    @AppStorage("studio-language") private var language="tr"
    let pages:[(String,String)]=[("Canlı test","waveform.path.ecg"),("Katmanlar","square.grid.3x3"),("Oyun profili","gamecontroller"),("Hall ayarları","switch.2"),("Aydınlatma","sparkles"),("Güç ve pil","battery.75percent"),("Gelişmiş","slider.horizontal.3"),("Özellikler","cube.transparent")]
    var body:some View {
        NavigationSplitView {
            VStack(alignment:.leading,spacing:16){HStack{Image(nsImage:studioGridIcon).resizable().interpolation(.high).frame(width:30,height:30);VStack(alignment:.leading){Text("WIRELESS HE").font(.headline);Text("MACROPAD STUDIO").font(.caption2).foregroundStyle(.secondary)}}.padding(.horizontal,12).padding(.top,20)
                List(selection:$model.page){ForEach(pages,id:\.0){p in Label { Text(LocalizedStringKey(p.0)) } icon: { Image(systemName:p.1) }.tag(p.0)}}.listStyle(.sidebar)
                VStack(alignment:.leading,spacing:5){
                    HStack(spacing:7){Circle().fill(model.deviceVisible ? accent : Color.orange).frame(width:8,height:8);Text(LocalizedStringKey(model.deviceVisible ? "CİHAZ GÖRÜLÜYOR" : "CİHAZ BAĞLI DEĞİL")).font(.caption2.bold()).foregroundStyle(model.deviceVisible ? accent : Color.orange)}
                    Text(LocalizedStringKey(model.freshSensor ? "USB · sensör verisi geliyor" : model.bleControlReady ? "BLE · ayar bağlantısı hazır" : model.bluetoothHIDPresent ? "BLE · klavye algılandı" : "Cihaz bağlantısı bekleniyor")).font(.caption.weight(.semibold))
                    if model.bluetoothHIDPresent && !model.freshSensor {Text("BLE üzerinden sensör verisi henüz desteklenmiyor").font(.caption2).foregroundStyle(.secondary)}
                }.padding(14)
            }.navigationSplitViewColumnWidth(min:190,ideal:220)
        } detail: {
            ScrollView { VStack(alignment:.leading,spacing:18) {
                HStack(alignment:.top){VStack(alignment:.leading,spacing:5){Text("WIRELESS HE / TEST STUDIO").font(.caption2.weight(.bold)).tracking(1.6).foregroundStyle(accent);Text(LocalizedStringKey(model.page)).font(.system(size:32,weight:.bold,design:.rounded));Text(LocalizedStringKey(subtitle)).font(.subheadline).foregroundStyle(.secondary)};Spacer();VStack(alignment:.trailing,spacing:5){Button("Yazdır",systemImage:"arrow.down.to.line"){model.writeHIDProfiles()}.buttonStyle(.borderedProminent).disabled((!model.freshSensor && !model.bleControlReady) || model.hidWriteStatus?.state == "writing").help(model.bleControlReady && !model.freshSensor ? "Seçili layer’ın klavye ve medya görevlerini C6’ya kaydet." : "Seçili layer’ın klavye ve medya görevlerini S3’e kaydet.");Text(verbatim:model.hidWriteError ?? (model.hidWriteStatus?.state == "saved" ? (model.hidWriteStatus?.message ?? "S3’e kaydedildi") : model.hidWriteStatus?.state == "writing" ? "S3’e yazılıyor… \(model.hidWriteStatus?.message ?? "")" : model.hidWriteStatus?.state == "error" ? (model.hidWriteStatus?.message ?? "Yazma başarısız") : "Seçili layer’ın klavye ve medya görevlerini kaydet")).font(.caption2).foregroundStyle(model.hidWriteError != nil || model.hidWriteStatus?.state == "error" ? Color.orange : Color.secondary).frame(maxWidth:360,alignment:.trailing)}}
                if let error=model.error {Text(error).foregroundStyle(.orange).font(.caption)}
                switch model.page {case "Katmanlar":LayersView(model:model);case "Oyun profili":GameProfileView(model:model);case "Hall ayarları":HallView(model:model);case "Aydınlatma":LightView(model:model);case "Güç ve pil":PowerView(model:model);case "Gelişmiş":AdvancedView(model:model);case "Özellikler":FeaturesView(model:model);default:LiveView(model:model)}
            }.padding(28).frame(maxWidth:.infinity,alignment:.leading)}.frame(maxWidth:.infinity,maxHeight:.infinity).background(studioBackground.ignoresSafeArea())
        }.environment(\.locale,Locale(identifier:language)).preferredColorScheme(appearance == "dark" ? .dark : appearance == "light" ? .light : nil).tint(accent)
    }
    var subtitle:String { switch model.page {case "Canlı test":return "12 analog kanal · USB üzerinden gerçek zamanlı izleme";case "Katmanlar":return "Dört katman için tuş ve encoder taslaklarını düzenle";case "Oyun profili":return "Analog hareket için yön ve hız taslağı";case "Hall ayarları":return "Seçili tuş için tetikleme davranışını planla";case "Aydınlatma":return "NeoPixel ayarlarını ekran üzerinde önizle";case "Güç ve pil":return "Hedef cihazın güç tercihlerini tasarla";case "Özellikler":return "Macropad’in parçalarını katman katman incele";default:return "SOCD, joystick ve makro taslakları"} }
}
final class LightGridNativeView:NSView {
    override var isFlipped:Bool {true}
    var shade:((Int,TimeInterval)->Color)?
    var subscription:AnyCancellable?
    private var cells:[CALayer]=[]
    override init(frame:NSRect) {
        super.init(frame:frame)
        wantsLayer=true
        guard let root=layer else{return}
        root.backgroundColor=NSColor.black.withAlphaComponent(0.85).cgColor
        root.cornerRadius=8;root.masksToBounds=true;root.isGeometryFlipped=true
        for index in 0..<12 {
            let cell=CALayer();cell.frame=CGRect(x:CGFloat(index%3)*66,y:CGFloat(index/3)*66,width:66,height:index >= 9 ? 70.5 : 66.5)
            root.addSublayer(cell);cells.append(cell)
        }
    }
    required init?(coder:NSCoder) {fatalError("init(coder:) is not supported")}
    func refresh() {
        guard let shade=shade else{return}
        let time=Date.timeIntervalSinceReferenceDate
        CATransaction.begin();CATransaction.setDisableActions(true)
        for (index,cell) in cells.enumerated() {
            let key=(index%3)*4+index/3
            cell.backgroundColor=NSColor(shade(key,time)).usingColorSpace(.sRGB)?.cgColor
        }
        CATransaction.commit()
    }
}
struct LightGridNative:NSViewRepresentable {
    @ObservedObject var model:StudioModel
    let shade:(Int,TimeInterval)->Color
    func makeNSView(context:Context)->LightGridNativeView {
        let view=LightGridNativeView(frame:NSRect(x:0,y:0,width:198,height:270))
        view.shade=shade
        view.subscription=model.lightingClock.$tick.dropFirst().sink {[weak view] _ in view?.refresh()}
        view.refresh();return view
    }
    func updateNSView(_ view:LightGridNativeView,context:Context) {view.shade=shade;view.refresh()}
}
struct KeyPad:View {
    @ObservedObject var model:StudioModel
    var mappings=false
    var showLabels=true
    private let columns=Array(repeating:GridItem(.fixed(62),spacing:6),count:3)
    private func ledColor(_ index:Int,at time:TimeInterval)->Color {
        let light=model.config.layers[model.layer].light
        guard model.lightingPreviewActive,light.effectiveEffect != "Kapalı" else{return .clear}
        let duration=max(0.3,3.0-light.speed*0.025)
        let pulse=model.pulseIntensity(index,at:time,duration:duration,wave:light.effectiveEffect == "Dalga")
        var animation=(light.effectiveEffect == "Nefes" || light.effectiveEffect == "Dalga") ? pulse : 1.0
        if light.effectiveEffect == "Yılan" {animation=model.snakeIntensity(index,at:time,duration:duration)}
        if light.effectiveEffect == "Starlight" {animation=model.starlightIntensity(index,at:time,duration:duration)}
        if light.effectiveEffect == "Kayan ışık" {
            animation=model.sweepIntensity(index,at:time,duration:duration)
        }
        if light.effectiveEffect == "Tepkisel" {
            animation=0
            if let hit=model.keyLightHits[index] {
                let dx=Double(index/4-hit.key/4), dy=Double(index%4-hit.key%4)
                let distance=sqrt(dx*dx+dy*dy)
                let wait=light.effectiveEffect == "Tepkisel" ? 0 : distance*duration*0.16
                let elapsed=time-hit.time-wait
                if (light.effectiveEffect != "Tepkisel" || index == hit.key) && elapsed >= 0 {animation=max(0,1-elapsed/(duration*0.45))}
            }
        }
        if light.effectiveEffect == "Basılan yerden dalga" {animation=model.waveLight(index,at:time,sensitive:false)}
        if light.effectiveEffect == "Derinliğe duyarlı" || light.effectiveEffect == "Derinliğe duyarlı dalga" {animation=model.pressureLight(index,at:time)}
        let brightness=min(1,max(0,model.effectiveLightBrightness/100))*animation
        if light.colorful && light.baseEffect == "Yılan" {return Color(nsColor:model.snakeColor(at:time,duration:duration)).opacity(brightness)}
        if light.colorful && light.baseEffect == "Starlight" {return Color(nsColor:model.starlightColor(index,at:time,duration:duration)).opacity(brightness)}
        if light.colorful && light.baseEffect == "Kayan ışık" {return Color(nsColor:model.sweepColor(index,at:time,duration:duration)).opacity(brightness)}
        if light.colorful && light.baseEffect == "Basılan yerden dalga" {return Color(nsColor:model.waveColor(index,at:time,sensitive:light.usesDepth)).opacity(brightness)}
        if light.colorful && light.baseEffect == "Tepkisel" {return Color(nsColor:model.reactiveColor(index)).opacity(brightness)}
        if light.colorful && ["Nefes","Dalga"].contains(light.baseEffect) {return Color(nsColor:model.cycleColor(index,at:time,duration:duration,wave:light.baseEffect == "Dalga")).opacity(brightness)}
        if light.effectiveEffect == "Gökkuşağı" {return Color(nsColor:model.rainbowColor(index,at:time,duration:duration)).opacity(brightness)}
        let hex=light.color.trimmingCharacters(in:CharacterSet(charactersIn:"#"))
        let n=Int(hex,radix:16) ?? 0xA6F0CA
        return Color(red:Double((n>>16)&255)/255,green:Double((n>>8)&255)/255,blue:Double(n&255)/255).opacity(brightness)
    }
    var body:some View {
        VStack(spacing:0) {
            HStack(alignment:.center,spacing:6){
                Button {model.nextLayer()} label:{ZStack {RoundedRectangle(cornerRadius:9).fill(LinearGradient(colors:[Color(white:0.48),Color(white:0.27)],startPoint:.topLeading,endPoint:.bottomTrailing)).shadow(color:.black.opacity(0.65),radius:3,x:0,y:4);RoundedRectangle(cornerRadius:7).fill(LinearGradient(colors:[Color(white:0.56),Color(white:0.38)],startPoint:.topLeading,endPoint:.bottomTrailing)).padding(6).offset(y:-2);if showLabels {Text("LAYER").font(.system(size:8,weight:.bold)).foregroundStyle(.white.opacity(0.65))}}.frame(width:54,height:54).scaleEffect(model.deviceLayer?.pressed == true ? 0.94 : 1).offset(y:model.deviceLayer?.pressed == true ? 2 : 0).animation(.easeOut(duration:0.08),value:model.deviceLayer?.pressed)}.buttonStyle(.plain).accessibilityLabel("Sonraki katmana geç; uygulama önizlemesi")
                VStack(spacing:5){ForEach(0..<4){i in Capsule().fill(i == model.layer ? accent : Color(white:0.35)).frame(width:9,height:3)}}.accessibilityLabel("Düzenlenen katman göstergesi; canlı cihaz durumu değil")
                Spacer(minLength:10)
                ZStack {Circle().fill(Color.black).shadow(color:.black.opacity(0.7),radius:3,x:0,y:3);Circle().fill(RadialGradient(colors:[Color(white:0.78),Color(white:0.49),Color(white:0.28)],center:.topLeading,startRadius:1,endRadius:48)).padding(4);Circle().stroke(.white.opacity(0.14),lineWidth:1).padding(5)}.frame(width:58,height:58).accessibilityLabel("Encoder; canlı veri yok")
            }.padding(.bottom,12)
            HStack(alignment:.top,spacing:0){
                LazyVGrid(columns:columns,spacing:6){ForEach(0..<12,id:\.self){idx in
                    let i=(idx%3)*4+idx/3
                    let isSelected=model.page == "Aydınlatma" ? model.lightingKeyHighlighted(i) : model.selected == i && ["Canlı test","Katmanlar"].contains(model.page)
                    let progress=model.displayedPercent(i)/100
                    let label=mappings && !model.config.layers[model.layer].keys[i].isEmpty ? model.config.layers[model.layer].keys[i] : "KEY \(i)"
                    Button {model.selectKey(i)} label:{
                        ZStack {
                            RoundedRectangle(cornerRadius:7).fill(LinearGradient(colors:[Color(white:0.84),Color(white:0.61)],startPoint:.topLeading,endPoint:.bottomTrailing)).padding(3).offset(y:2+progress*2).shadow(color:.black.opacity(0.48),radius:2,x:0,y:3-progress*2)
                            RoundedRectangle(cornerRadius:6).fill(LinearGradient(colors:isSelected ? [Color(red:0.73,green:0.98,blue:0.86),Color(red:0.38,green:0.81,blue:0.61)] : [Color(white:0.98),Color(white:0.88)],startPoint:.topLeading,endPoint:.bottomTrailing)).padding(.horizontal,7).padding(.top,5).padding(.bottom,10).offset(y:progress*3).overlay(RoundedRectangle(cornerRadius:6).strokeBorder((model.page == "Aydınlatma" ? isSelected : model.selected == i) ? accent.opacity(0.9) : .white.opacity(0.25),lineWidth:(model.page == "Aydınlatma" ? isSelected : model.selected == i) ? 2 : 0.7).padding(.horizontal,7).padding(.top,5).padding(.bottom,10).offset(y:progress*3))
                            if showLabels {VStack(spacing:1){Text(verbatim:studioDisplay(label)).font(.system(size:9,weight:.bold)).lineLimit(mappings ? 2 : 1).multilineTextAlignment(.center).minimumScaleFactor(0.8).truncationMode(.tail).frame(width:43,height:mappings ? 22 : nil);Text(model.visualPercent(i).map{String(format:"%.0f%%",$0)} ?? (model.values.map{String($0[i])} ?? "—")).font(.system(size:10,weight:.medium,design:.monospaced))}.foregroundStyle(Color(white:0.28)).offset(y:-2+progress*3)}
                        }.frame(width:62,height:62).contentShape(Rectangle())
                    }.buttonStyle(.plain).help(label).accessibilityLabel("\(i/4+1). sütun, \(i%4+1). satır, KEY \(i), \(label)").accessibilityAddTraits(isSelected ? .isSelected : [])
                }}.frame(width:198).background {
                    LightGridNative(model:model,shade:ledColor)
                        .frame(width:198,height:270).offset(y:2).allowsHitTesting(false)
                }
                Spacer(minLength:0)
            }
        }
        .padding(.horizontal,17).padding(.top,16).padding(.bottom,19)
        .frame(width:310)
        .background(LinearGradient(colors:[Color(white:0.16),Color(white:0.065)],startPoint:.topLeading,endPoint:.bottomTrailing),in:RoundedRectangle(cornerRadius:19))
        .overlay(RoundedRectangle(cornerRadius:19).strokeBorder(.white.opacity(0.11),lineWidth:1))
        .shadow(color:.black.opacity(0.45),radius:12,x:0,y:10)
        .accessibilityElement(children:.contain)
    }
}
struct FeaturesView:View {
    @ObservedObject var model:StudioModel
    @State private var stage=0
    private let titles=["Birleştirilmiş","Tuş kapakları","Switchler ve üst kapak","Plate ve PCB"]
    private let descriptions=["Macropad’in tamamlanmış görünümü.","Tuş kapakları kaldırıldı; altlarındaki switchler görünüyor.","Switchler ve üst kapak kaldırıldı; altlarındaki plate görünüyor.","Plate de kaldırıldı; PCB ve bileşenlerini inceleyebilirsin."]
    var body:some View {
        VStack(alignment:.leading,spacing:16) {
            GlassCard {VStack(alignment:.leading,spacing:14) {
                HStack {SectionTitle(title:"İç yapıyı keşfet",subtitle:"Blender modelindeki parçalar · sürükleyerek döndür");Spacer();Text("\(stage+1) / 4").font(.caption.monospacedDigit()).foregroundStyle(.secondary)}
                MacropadModelView(model:model,clock:model.lightingClock,explodedStage:stage).frame(height:490).clipShape(RoundedRectangle(cornerRadius:16))
                HStack {
                    VStack(alignment:.leading,spacing:4){Text(LocalizedStringKey(titles[stage])).font(.headline);Text(LocalizedStringKey(descriptions[stage])).font(.caption).foregroundStyle(.secondary)}
                    Spacer()
                    if stage > 0 {Button("Geri"){stage-=1}}
                    Button(LocalizedStringKey(stage == 3 ? "Başa dön" : "Parçaları kaldır")){stage = stage == 3 ? 0 : stage+1}.buttonStyle(.borderedProminent)
                }
            }}
        }
    }
}
struct RateStatusCard:View {
    @ObservedObject var model:StudioModel
    private func metric(_ label:LocalizedStringKey,_ value:String,_ note:String)->some View {
        VStack(alignment:.leading,spacing:5){Text(label).font(.caption).foregroundStyle(.secondary);Text(value).font(.title2.monospacedDigit().weight(.semibold)).foregroundStyle(accent);Text(verbatim:note).font(.caption2).foregroundStyle(.secondary)}.frame(maxWidth:.infinity,alignment:.leading)
    }
    var body:some View {
        GlassCard {VStack(alignment:.leading,spacing:12){
            HStack {SectionTitle(title:"Canlı hız ve güç durumu",subtitle:"C6 ve S3 ölçümü · son yaklaşık 1 saniye");Spacer();if let rate=model.rateStatus {Text(LocalizedStringKey(rate.active ? "AKTİF" : "BEKLEMEDE")).font(.caption.bold()).foregroundStyle(rate.active ? accent : .secondary)}}
            if let rate=model.rateStatus,rate.stats_valid {
                let scanTarget=rate.active ? 1000 : 100
                let sendTarget=rate.active ? 1000 : (rate.app_open ? 100 : 10)
                HStack(alignment:.top,spacing:12){
                    metric("C6 tarama", "\(rate.scanRate) Hz", studioDisplay("Hedef")+": \(scanTarget) Hz")
                    metric("C6 gönderim", "\(rate.sendRate) Hz", studioDisplay("Hedef")+": \(sendTarget) Hz")
                    metric("S3 alım",model.bridgeStatus.map{"~\($0.rx) / s"} ?? "—",studioDisplay("Alınan paket"))
                    metric("Sıra boşluğu",model.bridgeStatus.map{String($0.sequence_gaps)} ?? "—",studioDisplay("Son aralık"))
                }
                HStack {Text(LocalizedStringKey(rate.app_open ? "Uygulama bildirimi: açık" : "Uygulama bildirimi: kapalı"));Spacer();Text(LocalizedStringKey(rate.gate_enabled ? "MOSFET kontrolü: açık" : "MOSFET kontrolü: kapalı"))}.font(.caption2).foregroundStyle(.secondary)
                if rate.gate_enabled,let on=rate.gate_on_us,rate.interval_ms>0 {
                    let percent=min(100.0,Double(on)/Double(rate.interval_ms*1000)*100)
                    HStack {Text("Komutlanan analog besleme");Spacer();Text(String(format:"%.1f%%",percent)).monospacedDigit().foregroundStyle(accent)}.font(.caption.weight(.medium))
                    ProgressView(value:percent,total:100).tint(accent)
                    Text("Bu oran GPIO açma süresidir; voltaj ve güç tüketimi ölçümü değildir.").font(.caption2).foregroundStyle(.secondary)
                }
                Text("C6 gönderimi radyo sürücüsüne iletilen örneklerdir; alınma garantisi değildir. MOSFET durumu enerji ölçümü değildir.").font(.caption2).foregroundStyle(.secondary)
            } else {Text("Kartın hız bilgisi bekleniyor.").font(.caption).foregroundStyle(.secondary)}
        }}
    }
}
struct DeviceConnectionBadge:View {
    @ObservedObject var model:StudioModel
    var body:some View {
        if !model.deviceVisible {
            Label("Cihaz bağlı değil",systemImage:"exclamationmark.triangle.fill")
                .font(.caption.bold()).foregroundStyle(.white)
                .padding(.horizontal,10).padding(.vertical,7)
                .background(Color.orange.opacity(0.9),in:Capsule())
                .padding(10).accessibilityLabel(Text("Cihaz bağlı değil"))
        } else if model.bluetoothHIDPresent && !model.freshSensor {
            Label("BLE klavye algılandı · sensör verisi yok",systemImage:"antenna.radiowaves.left.and.right")
                .font(.caption.bold()).foregroundStyle(.white)
                .padding(.horizontal,10).padding(.vertical,7)
                .background(Color.blue.opacity(0.85),in:Capsule()).padding(10)
        }
    }
}
struct BLEDeviceSlotsView:View {
    @ObservedObject var model:StudioModel
    @AppStorage("ble-slot-name-1") private var name1="Cihaz 1"
    @AppStorage("ble-slot-name-2") private var name2="Cihaz 2"
    @AppStorage("ble-slot-name-3") private var name3="Cihaz 3"
    var body:some View {
        GlassCard { VStack(alignment:.leading,spacing:10){
            HStack {SectionTitle(title:"Bluetooth cihazları",subtitle:"Adlar bu Mac’te düzenlenebilir etiketlerdir");Spacer();Button("Cihazı ara"){model.searchBluetooth()}.disabled(model.bleControlReady)}
            HStack(spacing:12){ForEach(0..<3){i in
                VStack(alignment:.leading,spacing:4){Text([name1,name2,name3][i]).font(.headline)
                    Text(LocalizedStringKey(model.bleSlotMask.map{($0 & (1<<i)) != 0 ? "Kayıtlı" : "Boş"} ?? "Durum bilinmiyor")).font(.caption)
                    if model.bleSlotIndex==i {Text("SEÇİLİ").font(.caption2.bold()).foregroundStyle(accent)}
                }.frame(maxWidth:.infinity,alignment:.leading).padding(12).background(Color.white.opacity(0.04),in:RoundedRectangle(cornerRadius:10))
            }}
            Text("Layer + alt sıradaki sol, orta veya sağ tuşla cihazı seç. Adları Ayarlar → Bluetooth bölümünde düzenle.").font(.caption).foregroundStyle(.secondary)
        }}
    }
}
struct LiveView:View {
    @ObservedObject var model:StudioModel
    @AppStorage("studio-view-3d") private var show3D=true
    var body:some View { VStack(spacing:16){RateStatusCard(model:model);BLEDeviceSlotsView(model:model);HStack(alignment:.top,spacing:16){GlassCard{VStack(alignment:.leading,spacing:14){HStack{SectionTitle(title:"Cihaz görünümü",subtitle:show3D ? "Sürükle: döndür · kaydır: yakınlaştır · tuşa tıkla" : "Bir tuşa tıklayarak kanalını seç");Spacer();Picker("Görünüm",selection:$show3D){Text("3B").tag(true);Text("2B").tag(false)}.labelsHidden().pickerStyle(.segmented).frame(width:100)};Group {if show3D {MacropadModelView(model:model,clock:model.lightingClock).frame(height:310).clipShape(RoundedRectangle(cornerRadius:16))} else {KeyPad(model:model).frame(maxWidth:.infinity)}}.overlay(alignment:.topTrailing){DeviceConnectionBadge(model:model)};HStack{Toggle("Simülasyon",isOn:$model.simulate).toggleStyle(.switch);Spacer();Toggle("Işık önizleme",isOn:$model.lightingPreview).toggleStyle(.switch)}.font(.caption);if model.simulate {HStack{Text("KEY \(model.selected) · demo").font(.caption);Slider(value:$model.demoDepth,in:0...100,step:1);Text("\(Int(model.demoDepth))%").font(.caption.monospacedDigit());Button(model.demoDepth > 0 ? "Bırak" : "Bas"){model.demoDepth = model.demoDepth > 0 ? 0 : 100}};Text("Simülasyon sensör verisini değiştirmez.").font(.caption2).foregroundStyle(.secondary)}}};VStack(spacing:16){GlassCard{VStack(alignment:.leading,spacing:12){HStack{SectionTitle(title:"Anlık basılma",subtitle:"KEY \(model.selected) · \(model.selected%4+1). satır / \(model.selected/4+1). sütun");Spacer();Text(LocalizedStringKey(model.values == nil ? "BAĞLANTI BEKLENİYOR" : "USB · CANLI")).font(.caption2.bold()).foregroundStyle(model.values == nil ? .orange : accent)};HStack(alignment:.bottom){Text(model.percent(model.selected).map{String(format:"%.1f%%",$0)} ?? "— %").font(.system(size:48,weight:.semibold,design:.rounded)).foregroundStyle(accent);Spacer();VStack(alignment:.trailing){Text("HAM ADC").font(.caption2).foregroundStyle(.secondary);Text(model.values.map{String($0[model.selected])} ?? "—").font(.title3.monospacedDigit())}};ProgressView(value:(model.percent(model.selected) ?? 0),total:100).tint(accent)}};GlassCard{VStack(alignment:.leading){SectionTitle(title:"Sinyal geçmişi",subtitle:"Ham ADC · seçili tuş · 0–4095");ZStack { Chart(model.history){p in AreaMark(x:.value("Örnek",p.id),y:.value("ADC",p.value)).foregroundStyle(LinearGradient(colors:[accent.opacity(0.22),accent.opacity(0.01)],startPoint:.top,endPoint:.bottom));LineMark(x:.value("Örnek",p.id),y:.value("ADC",p.value)).foregroundStyle(accent).lineStyle(StrokeStyle(lineWidth:2)).interpolationMethod(.linear)}.chartYScale(domain:0...4095).chartYAxis { AxisMarks(position:.trailing,values:[0,1000,2000,3000,4000]) { _ in AxisGridLine(); AxisValueLabel().foregroundStyle(.secondary) } };if model.history.isEmpty {VStack(spacing:6){Image(systemName:"waveform.path").font(.title2).foregroundStyle(accent.opacity(0.65));Text("Sinyal bekleniyor").font(.caption.weight(.medium));Text("Cihaz bağlandığında grafik burada görünür").font(.caption2).foregroundStyle(.secondary)}.padding(12).background(.ultraThinMaterial,in:RoundedRectangle(cornerRadius:12))}}.frame(height:160)}}}}
        HStack(alignment:.top,spacing:16){GlassCard{VStack(alignment:.leading,spacing:12){SectionTitle(title:"Kalibrasyon",subtitle:"Tuş başına serbest ve tam basılı değerleri");Text(verbatim:studioDisplay("Serbest: \(model.config.calibration[model.channelMap[model.selected]].free.map(String.init) ?? "—")  ·  Basılı: \(model.config.calibration[model.channelMap[model.selected]].pressed.map(String.init) ?? "—")")).font(.caption.monospacedDigit()).foregroundStyle(.secondary);Button("Tüm serbest konumları kaydet"){model.captureAll()}.disabled(model.values == nil);HStack{Button("Seçili: serbest"){model.capture(false)};Button("Seçili: tam basılı"){model.capture(true)}}.disabled(model.values == nil);Text("Bu değerler yalnızca bu Mac uygulamasında saklanır.").font(.caption2).foregroundStyle(.secondary)}};GlassCard{VStack(alignment:.leading,spacing:12){SectionTitle(title:"Kanal tanılama",subtitle:"Her tuş için bağlı olduğu ham ADC kanalını eşle");LazyVGrid(columns:Array(repeating:GridItem(.flexible()),count:3),spacing:7){ForEach(0..<12){position in let i=(position%3)*4+position/3;VStack(alignment:.leading){Text(verbatim:studioDisplay("\(position/3+1). sıra · \(position%3+1). sütun")).font(.caption2).foregroundStyle(.secondary);Menu {ForEach(0..<12){channel in Button("ADC \(channel)"){model.assignChannel(channel,toKey:i)}}} label:{Text(verbatim:studioDisplay("Eşle · ADC \(model.channelMap[i])")).font(.caption2)};Text(model.values.map{String($0[i])} ?? "—").font(.caption.monospacedDigit().bold()).foregroundStyle(model.values.map{($0[i]==0 || $0[i]==4095)} == true ? .orange : .primary);Text(model.ranges[i].map{"\($0.lowerBound) – \($0.upperBound)"} ?? "—").font(.caption2).foregroundStyle(.secondary)}.frame(maxWidth:.infinity,alignment:.leading).padding(7).background(.white.opacity(0.04),in:RoundedRectangle(cornerRadius:7))}};Button("Min / maks sıfırla"){model.ranges=Array(repeating:nil,count:12)}.font(.caption);Text("Aynı kanalı seçersen iki tuşun kanalları yer değiştirir. Kalibrasyon yokken tuş hareketi yaklaşık gösterilir; kesin derinlik için kalibrasyon yap.").font(.caption2).foregroundStyle(.secondary)}}}}
        SensorToolsView(model:model)
        SerialSelectionView(model:model)
        SessionLogsView()
    }
}
struct SensorToolsView:View {
    @ObservedObject var model:StudioModel
    @State private var applyAll=false
    var body:some View {
        HStack(alignment:.top,spacing:16) {
            GlassCard {VStack(alignment:.leading,spacing:10) {
                SectionTitle(title:"12 tuş kalibrasyonu",subtitle:"Ölçümler tamamlanana kadar mevcut değerler korunur")
                if !model.wizardActive {Button("Kalibrasyonu başlat"){model.startWizard()}.disabled(!model.freshSensor)}
                else if model.wizardFree == nil {Text("Tüm tuşları bırak.").font(.subheadline);Button("Serbest konumları ölç · 0,8 sn"){model.beginCapture(.free)}.disabled(model.sensorCapture != nil)}
                else if model.wizardStep < 12 {let pos=model.wizardStep;Text("\(pos/3+1). sıra · \(pos%3+1). sütun — sonuna kadar bas ve tut").font(.subheadline);ProgressView(value:Double(pos),total:12);Button("Basılı konumu ölç · 0,8 sn"){model.beginCapture(.pressed)}.disabled(model.sensorCapture != nil)}
                else {Text("Kaydetmeden önce tüm tuşları bırak.").font(.caption);Button("12 tuşu kaydet"){model.saveWizard()}.buttonStyle(.borderedProminent).disabled(!model.wizardAllReleased)}
                if model.wizardActive {Button("İptal"){model.cancelWizard()}.font(.caption)}
                if !model.wizardMessage.isEmpty {Text(model.wizardMessage).font(.caption).foregroundStyle(.secondary)}
            }}
            GlassCard {VStack(alignment:.leading,spacing:10) {
                SectionTitle(title:"Tepki eğrisi",subtitle:"Kalibrasyonlu sensör yüzdesine uygulanır")
                Toggle("Tüm tuşlara uygula",isOn:$applyAll).font(.caption)
                let binding=Binding<Double>(get:{model.curve(model.selected)},set:{model.setCurve($0,all:applyAll)})
                HStack{Text("KEY \(model.selected)");Slider(value:binding,in:0.4...2.5,step:0.05);Text(String(format:"%.2f×",model.curve(model.selected))).monospacedDigit()}
                HStack {Button("Erken tepki"){model.setCurve(0.65,all:applyAll)};Button("Doğrusal"){model.setCurve(1,all:applyAll)};Button("Yumuşak başlangıç"){model.setCurve(1.5,all:applyAll)}}.font(.caption)
                Text("1,00 doğrusal; düşük değer erken, yüksek değer geç tepki verir. Mevcut ayarlar varsayılan olarak değişmez.").font(.caption2).foregroundStyle(.secondary)
            }}
            GlassCard {VStack(alignment:.leading,spacing:10) {
                SectionTitle(title:"Boşta gürültü",subtitle:"Tüm tuşlar bırakılmışken 5 saniye ölç")
                Text(verbatim:studioDisplay("Seçili tuş tetik durumu: \(model.triggerStates[model.selected] ? "BASILI" : "SERBEST") · Mac testi")).font(.caption).foregroundStyle(model.triggerStates[model.selected] ? accent : .secondary)
                Button("Ölçümü başlat"){model.beginCapture(.noise)}.disabled(!model.freshSensor || model.sensorCapture != nil || model.wizardActive)
                if !model.noiseMessage.isEmpty {Text(model.noiseMessage).font(.caption).foregroundStyle(.secondary)}
                if let result=model.noiseResult {Text("\(result.samples) farklı örnek · \(String(format:"%.1f",result.elapsed)) sn").font(.caption.monospacedDigit());LazyVGrid(columns:Array(repeating:GridItem(.flexible()),count:3),spacing:5){ForEach(0..<12){position in let key=StudioModel.physicalOrder[position];let channel=model.channelMap[key];Text("K\(key): Δ\(result.spans[channel]) · σ\(String(format:"%.1f",result.deviations[channel]))").font(.caption2.monospacedDigit()).frame(maxWidth:.infinity,alignment:.leading)}};Text("Δ: en büyük–en küçük ADC; σ: standart sapma. Ölçüm tanı koymaz.").font(.caption2).foregroundStyle(.secondary)}
            }}
        }
    }
}
struct SerialSelectionView:View {
    @ObservedObject var model:StudioModel
    @AppStorage("test-serial-port") private var selectedPort=""
    @AppStorage("receiver-serial-port") private var receiverPort=""
    @State private var ports:[String]=[]
    var body:some View {
        GlassCard {VStack(alignment:.leading,spacing:10) {
            HStack {SectionTitle(title:"Sensör verisinin USB portu",subtitle:"Doğrudan C6 veya S3 üzerinden test için port seç");Spacer();Button("Portları yenile"){refresh()}}
            HStack {
                Picker("Sensör portu",selection:$selectedPort) {
                    Text("Otomatik (eski davranış)").tag("")
                    if !selectedPort.isEmpty && !ports.contains(selectedPort) {Text(verbatim:studioDisplay("\(selectedPort) · bağlı değil")).tag(selectedPort)}
                    ForEach(ports,id:\.self){port in Text(port).tag(port)}
                }.frame(maxWidth:460)
                Spacer()
                Text(model.sensorSerialPort.map{"Aktif: \($0)"} ?? "Sensör verisi bekleniyor").font(.caption).foregroundStyle(.secondary)
            }
            Text("Seçim uygulama yeniden açıldığında kullanılır. Doğrudan testte C6, kablosuz köprü testinde S3 portunu seç. Otomatik seçim ilk uygun portu seçer.").font(.caption).foregroundStyle(.secondary)
            Divider()
            HStack {
                Picker("S3 portu · hazırlık",selection:$receiverPort) {
                    Text("Seçilmedi").tag("")
                    if !receiverPort.isEmpty && !ports.contains(receiverPort) {Text("\(receiverPort) · görünmüyor").tag(receiverPort)}
                    ForEach(ports,id:\.self){port in Text(port).tag(port)}
                }.frame(maxWidth:460)
                Spacer()
                Text(receiverPort.isEmpty ? "S3 portu seçilmedi" : (ports.contains(receiverPort) ? "Port görünüyor · iletişim kurulmadı" : "Port görünmüyor"))
                    .font(.caption).foregroundStyle(.secondary)
            }
            if !receiverPort.isEmpty {Button("S3'ü sensör akışı olarak seç"){selectedPort=receiverPort}.font(.caption).disabled(!ports.contains(receiverPort))}
            Text("S3 seçimi yalnızca bu Mac'te saklanır; S3'e ayar göndermez. Kablosuz test için S3 üzerinde uyumlu sensör köprüsü gerekir. HID modundaki S3 seri port göstermeyebilir.").font(.caption).foregroundStyle(.secondary)
        }}.onAppear(perform:refresh)
    }
    private func refresh() {
        ports=((try? FileManager.default.contentsOfDirectory(atPath:"/dev")) ?? [])
            .filter{$0.hasPrefix("cu.usbmodem") || $0.hasPrefix("cu.usbserial")}
            .sorted().map{"/dev/"+$0}
    }
}
struct SessionLogSummary:Identifiable {
    let id:String;let bytes:Int64;let closed:Bool;let duration:Double?;let valid:Int?;let missing:Int?;let invalid:Int?;let completeness:String?
}
struct SessionLogsView:View {
    @State private var entries:[SessionLogSummary]=[]
    @State private var loading=false
    @State private var message="Henüz yüklenmedi."
    var body:some View {
        GlassCard {VStack(alignment:.leading,spacing:10) {
            HStack {SectionTitle(title:"Sinyal oturumları",subtitle:"Son sekiz yerel kayıt · yalnızca okuma");Spacer();Button("Yenile"){load()}.disabled(loading)}
            if loading {ProgressView().controlSize(.small)} else if entries.isEmpty {Text(LocalizedStringKey(message)).font(.caption).foregroundStyle(.secondary)}
            ForEach(entries) {entry in
                HStack {Text(entry.id).font(.caption.monospacedDigit());Spacer();if entry.closed {Text(verbatim:studioDisplay(String(format:"%.1f sn",entry.duration ?? 0)));Text(verbatim:studioDisplay("\(entry.valid ?? 0) örnek"));Text(verbatim:studioDisplay("\(entry.missing ?? 0) sıra boşluğu"));Text(verbatim:studioDisplay("\(entry.invalid ?? 0) geçersiz"));if entry.completeness == "unknown" {Text("Tamlık bilinmiyor").foregroundStyle(.orange)}} else {Text("Bitiş kaydı yok · açık veya kesilmiş").foregroundStyle(.secondary)}}.font(.caption.monospacedDigit())
            }
            Text("Sıra boşlukları yalnız tespit edilenleri gösterir; tüm verinin eksiksiz ulaştığını veya sensör doğruluğunu kanıtlamaz.").font(.caption2).foregroundStyle(.secondary)
        }}.onAppear {if entries.isEmpty && !loading {load()}}
    }
    private func load() {
        guard let project=Bundle.main.object(forInfoDictionaryKey:"TestProjectPath") as? String else {message="Kayıt yolu bulunamadı.";return}
        let directory=URL(fileURLWithPath:project).appendingPathComponent("logs",isDirectory:true)
        loading=true
        DispatchQueue.global(qos:.utility).async {
            let fm=FileManager.default
            let files=(try? fm.contentsOfDirectory(at:directory,includingPropertiesForKeys:[.fileSizeKey],options:[.skipsHiddenFiles])) ?? []
            let urls=Array(files.filter{$0.pathExtension == "jsonl"}.sorted{$0.lastPathComponent > $1.lastPathComponent}.prefix(8))
            let found:[SessionLogSummary]=urls.map { url in
                let size=Int64((try? url.resourceValues(forKeys:[.fileSizeKey]).fileSize) ?? 0)
                var ending:[String:Any]?=nil
                if let handle=try? FileHandle(forReadingFrom:url) {
                    defer {try? handle.close()}
                    do {try handle.seek(toOffset:UInt64(max(0,size-65536)));let data=try handle.readToEnd() ?? Data()
                        for line in String(decoding:data,as:UTF8.self).split(separator:"\n").reversed() {
                            if let bytes=String(line).data(using:.utf8),let object=(try? JSONSerialization.jsonObject(with:bytes)) as? [String:Any],object["event"] as? String == "end" {ending=object;break}
                        }
                    } catch {}
                }
                return SessionLogSummary(id:url.deletingPathExtension().lastPathComponent,bytes:size,closed:ending != nil,duration:ending?["signal_duration_s"] as? Double,valid:ending?["valid_packets"] as? Int,missing:ending?["missing_packets_detected"] as? Int,invalid:ending?["invalid_packets"] as? Int,completeness:ending?["completeness"] as? String)
            }
            DispatchQueue.main.async {entries=found;message=found.isEmpty ? "Henüz oturum kaydı yok." : "";loading=false}
        }
    }
}
struct LayerPreset:Identifiable {
    let id:String; let icon:String; let detail:String; let rows:[String]; let encoder:[String]
    var keys:[String] { (0..<12).map { index in rows[(index%4)*3+index/4] } }
}
struct SavedLayerProfile:Codable,Identifiable {
    var id:UUID = UUID()
    var name:String
    var layer:LayerConfig
    var hall:[HallConfig]? = nil
}
struct LayersView:View {
    @ObservedObject var model:StudioModel
    @State private var selectedPreset="Blender"
    @State private var undoLayer:LayerConfig?
    @State private var undoIndex:Int?
    @State private var undoHall:[HallConfig]?
    @State private var restoreHall=false
    @State private var restoreLight=true
    @State private var depthPreview=0.0
    @State private var profiles:[SavedLayerProfile]=[]
    @State private var profileName=""
    @State private var selectedProfile:UUID?
    @State private var confirmDelete=false
    private let profileStorageKey="native-layer-profiles-v1"
    @State private var keyboardModifiers:UInt8=0
    private let keyboardGroups:[(String,[(String,UInt8)])] = [
        ("Harfler",Array("ABCDEFGHIJKLMNOPQRSTUVWXYZ").enumerated().map{(String($0.element),UInt8(4+$0.offset))}),
        ("Rakamlar",Array("1234567890").enumerated().map{(String($0.element),UInt8(30+$0.offset))}),
        ("Temel",[("Enter",40),("Escape",41),("Backspace",42),("Tab",43),("Space",44),("Caps Lock",57),("Delete",76)]),
        ("Sembol tuşları",[("Eksi",45),("Eşittir",46),("Sol köşeli parantez",47),("Sağ köşeli parantez",48),("Ters eğik çizgi",49),("Noktalı virgül",51),("Kesme işareti",52),("Grave",53),("Virgül",54),("Nokta",55),("Eğik çizgi",56)]),
        ("Yön ve gezinme",[("Sağ ok",79),("Sol ok",80),("Aşağı ok",81),("Yukarı ok",82),("Home",74),("End",77),("Page Up",75),("Page Down",78)]),
        ("F tuşları",(0..<12).map{("F\($0+1)",UInt8(58+$0))}),
        ("Değiştiriciler",[("Sol Ctrl",224),("Sol Shift",225),("Sol Option",226),("Sol Command",227),("Sağ Ctrl",228),("Sağ Shift",229),("Sağ Option",230),("Sağ Command",231)])
    ]
    private let presets:[LayerPreset] = [
        .init(id:"Blender",icon:"cube.transparent",detail:"Modelleme ve görünüm",rows:["Taşı · G","Döndür · R","Ölçekle · S","Dışa uzat · E","İçe ekle · I","Kes · K","Nesne/Sekme · Tab","Kopyala · Shift D","Sil · X","Geri al · Cmd Z","Kaydet · Cmd S","Arama · F3"],encoder:["Yakınlaştır −","Yakınlaştır +","Görünüm sıfırla"]),
        .init(id:"OBS Studio",icon:"video",detail:"Yayın ve sahneler",rows:["Sahne 1","Sahne 2","Sahne 3","Mikrofon aç/kapat","Kamera aç/kapat","Masaüstü sesi","Yayın başlat","Yayın durdur","Kayıt başlat/durdur","Geçiş","Stüdyo modu","İşaretleyici"],encoder:["Ses azalt","Ses artır","Sesi kapat"]),
        .init(id:"Photoshop",icon:"paintbrush.pointed",detail:"Düzenleme araçları",rows:["Taşı · V","Seçim · M","Fırça · B","Silgi · E","Damlalık · I","Metin · T","El · H","Yakınlaştır · Z","Kırp · C","Geri al · Cmd Z","Kaydet · Cmd S","Katman ekle"],encoder:["Fırça küçült","Fırça büyüt","Yakınlaştır"]),
        .init(id:"Minecraft",icon:"square.grid.3x3.fill",detail:"Hareket ve envanter",rows:["Envanter","İleri · W","Etkileşim","Sol · A","Geri · S","Sağ · D","Eğil","Zıpla","Koş","Eşya 1","Eşya 2","Eşya 3"],encoder:["Önceki eşya","Sonraki eşya","Eşya kullan"]),
        .init(id:"FPS",icon:"scope",detail:"Genel oyun taslağı",rows:["Yetenek 1","İleri · W","Yetenek 2","Sol · A","Geri · S","Sağ · D","Eğil","Zıpla","Koş","Yeniden doldur","Etkileşim","Harita"],encoder:["Önceki silah","Sonraki silah","Ses kapat"]),
        .init(id:"Euro Truck Simulator 2",icon:"truck.box",detail:"Analog direksiyon · kamyon",rows:["Korna","İleri · W","Kamera","Sol · A","Geri · S","Sağ · D","Sol sinyal","Dörtlü","Sağ sinyal","Far","Park freni","Silecek"],encoder:["Kamera sola","Kamera sağa","Kamera değiştir"]),
        .init(id:"Assetto Corsa",icon:"car",detail:"Analog direksiyon · yarış",rows:["Vites küçült","İleri · W","Vites büyüt","Sol · A","Geri · S","Sağ · D","Debriyaj","El freni","Kamera","ABS","Çekiş kontrolü","Yeniden başlat"],encoder:["Fren ayarı −","Fren ayarı +","Kamera değiştir"]),
        .init(id:"Fusion 360",icon:"cube",detail:"CAD · modelleme",rows:["Çizim","Ekstrüzyon","Döndürerek oluştur","Taşı","Ölç","Birleştir","Dikdörtgen","Çember","Çizgi","Geri al","Kaydet","Görünümü sıfırla"],encoder:["Yakınlaştır −","Yakınlaştır +","Görünümü sıfırla"]),
        .init(id:"Discord",icon:"bubble.left.and.bubble.right",detail:"Sesli görüşme",rows:["Mikrofon aç/kapat","Kulaklık aç/kapat","Bas-konuş","Önceki kanal","Sonraki kanal","Bildirimler","Kamera aç/kapat","Ekran paylaşımı","Görüşmeden ayrıl","Sesi azalt","Sesi artır","Sessize al"],encoder:["Ses azalt","Ses artır","Sesi kapat"]),
        .init(id:"Spotify",icon:"music.note",detail:"Müzik ve medya",rows:["Önceki parça","Oynat / Duraklat","Sonraki parça","Sesi azalt","Sessize al","Sesi artır","Karıştır","Tekrar","Beğen","Çalma listesi","Arama","Kitaplık"],encoder:["Ses azalt","Ses artır","Oynat / Duraklat"]),
        .init(id:"DaVinci Resolve",icon:"film",detail:"Kurgu · zaman çizelgesi",rows:["Geri al","Oynat / Duraklat","İleri al","Kes","Böl","Seçim aracı","Önceki işaret","İşaret ekle","Sonraki işaret","Kaydet","Yakınlaştır −","Yakınlaştır +"],encoder:["Bir kare geri","Bir kare ileri","Oynat / Duraklat"]),
        .init(id:"Microsoft Flight Simulator",icon:"airplane",detail:"Analog uçuş · roll / pitch",rows:["İniş takımı","İleri · W","Kamera","Sol · A","Geri · S","Sağ · D","Fren","Kanatçık azalt","Kanatçık artır","Motor gücü −","Motor gücü +","Pilot görünümü"],encoder:["Motor gücü azalt","Motor gücü artır","Kamera değiştir"]),
        .init(id:"Farming Simulator",icon:"leaf",detail:"Analog direksiyon · tarım",rows:["Araç değiştir","İleri · W","Kamera","Sol · A","Geri · S","Sağ · D","Alet kaldır","Alet aç/kapat","Alet indir","Motor","Araç seç","Harita"],encoder:["Kamera sola","Kamera sağa","Kamera değiştir"])
    ]
    // These are descriptive drafts; actual host support depends on future firmware.
    private let quickActions:[(String,[String])] = [
        ("Medya oynatma",["Oynat / Duraklat","Önceki parça","Sonraki parça","Oynatmayı durdur","İleri sar","Geri sar"]),
        ("Ses",["Ses artır","Ses azalt","Sesi kapat / aç","Mikrofonu sessize al / aç","Ses çıkışını değiştir","Ses girişini değiştir"]),
        ("Ekran ve ışık",["Ekran parlaklığını artır","Ekran parlaklığını azalt","Ekranı kapat","Klavye ışığını artır","Klavye ışığını azalt","Klavye ışığını aç / kapat"]),
        ("macOS ve pencereler",["Ekranı kilitle","Uykuya geçir","Mission Control","Uygulama pencerelerini göster","Masaüstünü göster","Bildirim Merkezi","Denetim Merkezi","Spotlight","Rahatsız Etme aç / kapat"]),
        ("Ekran görüntüsü",["Tüm ekranın görüntüsü","Seçili alanın görüntüsü","Ekran kaydını başlat / durdur"]),
        ("Tarayıcı",["Geri git","İleri git","Sayfayı yenile","Ana sayfayı aç","Yeni sekme aç","Sekmeyi kapat","Önceki sekme","Sonraki sekme"]),
        ("Görüşme",["Mikrofonu aç / kapat","Kamerayı aç / kapat","Ekran paylaşımını aç / kapat","Görüşmeden ayrıl"])
    ]
    private var preset:LayerPreset { presets.first(where:{$0.id==selectedPreset}) ?? presets[0] }
    private var usingBLE:Bool { !model.freshSensor && (model.bleControlReady || model.bluetoothHIDPresent) }
    private func unsupportedOverBLE(_ item:LayerPreset)->Bool {
        usingBLE && ["Euro Truck Simulator 2", "Assetto Corsa", "Microsoft Flight Simulator", "Farming Simulator"].contains(item.id)
    }
    var body:some View {
        VStack(spacing:16) {
            GlassCard {VStack(alignment:.leading,spacing:14) {
                HStack {SectionTitle(title:"Katmanlarını düzenle",subtitle:"Dört ayrı taslak · önce düzenlenecek katmanı seç");Spacer();Text("YEREL TASLAK").font(.caption2.bold()).foregroundStyle(accent).padding(8).background(accent.opacity(0.1),in:Capsule())}
                Toggle("Mac klavye çıktısını etkinleştir",isOn:$model.macOutputEnabled).toggleStyle(.switch)
                if model.macOutputEnabled {
                    Text(AXIsProcessTrusted() ? "Kalibre edilmiş tuşlar seçili katmandaki desteklenen klavye görevlerini gönderir." : "Klavye çıktısı için Sistem Ayarları → Gizlilik ve Güvenlik → Erişilebilirlik bölümünden Wireless HE Studio’ya izin ver.").font(.caption).foregroundStyle(.secondary)
                    Text("Seçilen klavye tuşları, desteklenen kısayollar ve medya görevleri çalışır. Encoder verisi henüz gelmiyor.").font(.caption).foregroundStyle(.secondary)
                    Text("S3 USB klavye çıktısıyla birlikte açılırsa aynı tuş iki kez gönderilebilir.").font(.caption).foregroundStyle(.orange)
                }
                Picker("Katman",selection:$model.layer){ForEach(0..<4){Text("Layer \($0+1)").tag($0)}}.labelsHidden().pickerStyle(.segmented)
                HStack{Image(systemName:"square.stack.3d.up").foregroundStyle(accent);Text(model.config.layers[model.layer].name.isEmpty ? "Adsız katman" : model.config.layers[model.layer].name).font(.headline);Spacer();Text(verbatim:studioDisplay("\(model.config.layers[model.layer].keys.filter{!$0.isEmpty}.count) / 12 atanmış")).font(.caption).foregroundStyle(.secondary)}
            }}
            HStack(alignment:.top,spacing:16) {
                GlassCard {VStack(alignment:.leading,spacing:14) {
                    SectionTitle(title:"Tuş düzeni",subtitle:"Bir tuşa tıkla; sağ taraftan görevini seç")
                    KeyPad(model:model,mappings:true).frame(maxWidth:.infinity)
                    Text("Tuşların üzerindeki isimler taslak görevlerdir.").font(.caption).foregroundStyle(.secondary)
                }}
                GlassCard {VStack(alignment:.leading,spacing:14) {
                    HStack {SectionTitle(title:"Seçili tuş",subtitle:"\(model.selected%4+1). satır · \(model.selected/4+1). sütun");Spacer();Text("KEY \(model.selected)").font(.caption.bold()).foregroundStyle(accent).padding(8).background(accent.opacity(0.1),in:Capsule())}
                    TextField("Katman adı",text:$model.config.layers[model.layer].name)
                    Text("TUŞ DAVRANIŞI").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                    Picker("Davranış",selection:behaviorBinding(\.mode)){Text("Normal tuş").tag("normal");Text("İki kademeli").tag("twoStage")}.labelsHidden().pickerStyle(.segmented)
                    Text(LocalizedStringKey(behavior.mode == "twoStage" ? "İlk eşikte birinci görev, daha derinde ikinci görev de etkin olur." : "Tuş, tetikleme eşiği geçildiğinde tek görev için etkin olur.")).font(.caption).foregroundStyle(.secondary)
                    Text(LocalizedStringKey(behavior.mode == "twoStage" ? "1. KADEME GÖREVİ" : "TUŞUN GÖREVİ")).font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                    taskDisplay(model.config.layers[model.layer].keys[model.selected])
                    let selectedTask=model.config.layers[model.layer].keys[model.selected]
                    if !selectedTask.isEmpty {
                        let supported=MacKeyOutput.resolve(behavior.primaryKey,selectedTask) != nil || (behavior.primaryKey == nil && (MacKeyOutput.media[selectedTask] != nil || MacKeyOutput.presetKey(selectedTask,profile:model.config.layers[model.layer].taskProfile) != nil))
                        Text(LocalizedStringKey(supported ? "Mac çıktısı: destekleniyor" : "Mac çıktısı: bu görev henüz desteklenmiyor; klavye kısayolu seçebilirsin.")).font(.caption).foregroundStyle(supported ? accent : .orange)
                        if behavior.primaryKey == nil, let shortcut=MacKeyOutput.customPresetShortcutText(selectedTask,profile:model.config.layers[model.layer].taskProfile) {
                            Text(verbatim: UserDefaults.standard.string(forKey:"studio-language") == "en" ? "Setup required: assign \(shortcut) to ‘\(selectedTask)’ in the target app or game." : "Kurulum gerekli: Hedef uygulama veya oyunda ‘\(selectedTask)’ görevinin kısayolunu \(shortcut) olarak ayarla.").font(.caption).foregroundStyle(.orange)
                        }
                    }
                    HStack(spacing:8) {
                        Toggle("⌘",isOn:modifierBinding(0x08));Toggle("⌥",isOn:modifierBinding(0x04));Toggle("⌃",isOn:modifierBinding(0x01));Toggle("⇧",isOn:modifierBinding(0x02))
                    }.toggleStyle(.checkbox).help("Seçeceğin klavye tuşuyla birlikte kullanılacak değiştiriciler")
                    HStack {
                        keyboardMenu(second:false)
                        if !selectedTask.isEmpty {taskBindingMenu(second:false)}
                        Menu {Button("Görevi temizle"){setSpecialTask("",second:false)};Divider();ForEach(quickActions,id:\.0){group,actions in Menu(LocalizedStringKey(group)){ForEach(actions,id:\.self){action in Button(LocalizedStringKey(action)){setSpecialTask(action,second:false)}}}}} label:{Label("Hazır görev seç",systemImage:"plus.circle")}
                    }
                    if let assignment=behavior.primaryKey, assignment.label == model.config.layers[model.layer].keys[model.selected] {Text(verbatim:studioDisplay("Bağlı tuş: \(assignmentName(assignment))")).font(.caption2).foregroundStyle(.secondary)}
                    else if !model.config.layers[model.layer].keys[model.selected].isEmpty {Text("Mevcut görev açıklaması korunuyor; gerçek tuş kodu için listeden yeniden seç.").font(.caption2).foregroundStyle(.secondary)}
                    Text(LocalizedStringKey(model.bleControlReady && !model.freshSensor ? "Yazdır, seçili layer’ı C6’ya kaydeder. S3 kaydı ayrıdır; ESP-NOW’a geçince S3’teki eski profil C6’daki profili değiştirebilir. Analog oyun ve encoder ayarları henüz kaydedilmez." : "Yazdır, seçili layer’ın desteklenen klavye ve medya görevlerini S3’e kaydeder. Analog oyun ve encoder ayarları henüz kaydedilmez.")).font(.caption).foregroundStyle(.secondary)
                    if behavior.mode == "twoStage" {
                        Text("2. KADEME GÖREVİ").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                        taskDisplay(behavior.secondAction)
                        HStack {
                            keyboardMenu(second:true)
                            if !behavior.secondAction.isEmpty {taskBindingMenu(second:true)}
                            Menu {Button("Görevi temizle"){setSpecialTask("",second:true)};Divider();ForEach(quickActions,id:\.0){group,actions in Menu(LocalizedStringKey(group)){ForEach(actions,id:\.self){action in Button(LocalizedStringKey(action)){setSpecialTask(action,second:true)}}}}} label:{Label("İkinci görev seç",systemImage:"plus.circle")}
                        }
                        if let assignment=behavior.secondaryKey, assignment.label == behavior.secondAction {Text(verbatim:studioDisplay("Bağlı tuş: \(assignmentName(assignment))")).font(.caption2).foregroundStyle(.secondary)}
                        RangeControl(label:"2. kademe eşiği",value:behaviorBinding(\.secondPoint),bounds:1...100)
                        Text("1. kademe eşiği: %\(Int(model.config.hall[model.selected].point)) · Hall ayarlarından değiştirilebilir.").font(.caption).foregroundStyle(.secondary)
                        if behavior.secondPoint <= model.config.hall[model.selected].point {Text("2. kademe eşiği ilk eşikten büyük olmalı.").font(.caption).foregroundStyle(.orange)}
                        Divider()
                        Text("BASMA ÖNİZLEMESİ").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                        RangeControl(label:"Örnek basılma",value:$depthPreview)
                        HStack {stageIndicator("1. kademe",active:depthPreview >= model.config.hall[model.selected].point);stageIndicator("2. kademe",active:depthPreview >= behavior.secondPoint && depthPreview >= model.config.hall[model.selected].point)}
                        Text("Önizleme yalnızca eşikleri gösterir; gerçek tuş basışı veya Rapid Trigger davranışı oluşturmaz.").font(.caption).foregroundStyle(.secondary)
                    }
                    Divider()
                    SectionTitle(title:"Encoder",subtitle:"Her katman için döndürme ve basma görevleri")
                    Text("NORMAL").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                    encoderAction("Sola çevir",value:$model.config.layers[model.layer].left,rotation:true)
                    encoderAction("Sağa çevir",value:$model.config.layers[model.layer].right,rotation:true)
                    encoderAction("Encoder’a bas",value:$model.config.layers[model.layer].push)
                    Toggle("Layer basılıyken farklı görevler",isOn:encoderHoldBinding(\.enabled)).toggleStyle(.switch)
                    if encoderHold.enabled {
                        Text("LAYER BASILIYKEN").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                        encoderAction("Sola çevir",value:encoderHoldBinding(\.left),rotation:true)
                        encoderAction("Sağa çevir",value:encoderHoldBinding(\.right),rotation:true)
                        encoderAction("Encoder’a bas",value:encoderHoldBinding(\.push))
                        Button("Örnek: ses ve parlaklık") {
                            var layer=model.config.layers[model.layer]
                            layer.left="Ses azalt";layer.right="Ses artır"
                            var hold=layer.encoderHold ?? EncoderHold();hold.enabled=true
                            hold.left="Ekran parlaklığını azalt";hold.right="Ekran parlaklığını artır"
                            layer.encoderHold=hold;model.config.layers[model.layer]=layer
                        }.font(.caption)
                    }
                    Text("Encoder görevleri standart HID komutları için hazırlanır; ekran parlaklığı desteği bilgisayara bağlıdır. Cihaz aktarımı ve layer basılı davranışı henüz uygulanmadı.").font(.caption).foregroundStyle(.secondary)
                }.textFieldStyle(.roundedBorder)}
            }
            GlassCard {VStack(alignment:.leading,spacing:16) {
                SectionTitle(title:"Hazır profiller",subtitle:"Bir başlangıç seç, katmana uygula ve düzenleyip kendi profilin olarak kaydet")
                if usingBLE {Label("BLE'de analog oyun kumandası ve encoder görevleri desteklenmiyor. Gri profiller analog eksen gerektirir.",systemImage:"info.circle").font(.caption).foregroundStyle(.secondary)}
                LazyVGrid(columns:[GridItem(.adaptive(minimum:155),spacing:10)],spacing:10) {ForEach(presets){item in
                    Button {selectedPreset=item.id} label:{VStack(alignment:.leading,spacing:8){Image(systemName:item.icon).font(.title3).foregroundStyle(accent);Text(item.id).font(.subheadline.bold());Text(LocalizedStringKey(item.detail)).font(.caption).foregroundStyle(.secondary).lineLimit(1)}.frame(maxWidth:.infinity,minHeight:68,alignment:.leading).padding(12).background(selectedPreset == item.id ? accent.opacity(0.12) : .white.opacity(0.04),in:RoundedRectangle(cornerRadius:12)).overlay(RoundedRectangle(cornerRadius:12).strokeBorder(selectedPreset == item.id ? accent.opacity(0.6) : .white.opacity(0.06),lineWidth:1))}.buttonStyle(.plain).disabled(unsupportedOverBLE(item)).opacity(unsupportedOverBLE(item) ? 0.4 : 1).help(unsupportedOverBLE(item) ? "BLE analog oyun kumandası desteklemiyor." : "")
                }}
                if unsupportedOverBLE(preset) {Label("Bu profil BLE'de kullanılamaz; başka bir profil seç.",systemImage:"exclamationmark.triangle").font(.caption).foregroundStyle(.orange)}
                HStack(alignment:.top,spacing:20) {
                    VStack(alignment:.leading,spacing:8){Text(verbatim:studioDisplay("\(preset.id) · ÖNİZLEME")).font(.caption2.bold()).tracking(1).foregroundStyle(accent);LazyVGrid(columns:Array(repeating:GridItem(.flexible(),spacing:5),count:3),spacing:5){ForEach(0..<12){i in Text(verbatim:studioDisplay(preset.rows[i])).font(.caption2).lineLimit(1).frame(maxWidth:.infinity,minHeight:27).padding(.horizontal,4).background(.white.opacity(0.05),in:RoundedRectangle(cornerRadius:6))}}}.frame(maxWidth:440)
                    VStack(alignment:.leading,spacing:9){Text("Encoder").font(.caption.bold());Text(verbatim:"← "+studioDisplay(preset.encoder[0])+"  ·  → "+studioDisplay(preset.encoder[1])).font(.caption).foregroundStyle(.secondary);Text(verbatim:studioDisplay("Bas")+" · "+studioDisplay(preset.encoder[2])).font(.caption).foregroundStyle(.secondary);Text("Uygulamak seçili katmanın adını, tuşlarını, davranışlarını ve encoder görevlerini değiştirir; iki kademeyi ve encoder’ın layer basılı görevlerini sıfırlar, oyun modunu profile göre açar veya kapatır ve ışığı korur.").font(.caption).foregroundStyle(.secondary);HStack{Button(action:{applyPreset()}) {Text(verbatim:UserDefaults.standard.string(forKey:"studio-language") == "en" ? "Apply to Layer \(model.layer+1)" : "Layer \(model.layer+1)’e uygula")}.buttonStyle(.borderedProminent).disabled(unsupportedOverBLE(preset));if undoIndex == model.layer && undoLayer != nil {Button("Geri al"){undoPreset()}}}}
                }
                if let setup=MacKeyOutput.customPresetActions[preset.id],!setup.isEmpty {
                    VStack(alignment:.leading,spacing:5){
                        Text("Hedef uygulamada kısayol ataması gerekli").font(.caption.bold()).foregroundStyle(.orange)
                        Text("Aşağıdaki öneriler varsayılan kısayol değildir. Aynı görevleri hedef uygulamada bu tuşlara bağla; istersen görev bazında değiştirebilirsin.").font(.caption).foregroundStyle(.secondary)
                        ForEach(setup,id:\.self){task in
                            if let shortcut=MacKeyOutput.customPresetShortcutText(task,profile:preset.id) {Text(verbatim:"\(studioDisplay(task))  ·  \(shortcut)").font(.caption2.monospaced()).textSelection(.enabled)}
                        }
                    }
                }
                Text("Hazır görevler öneridir; uygulama ve oyun kısayolları kullandığın ayarlara göre değişebilir. OBS görevleri için ayrıca kısayol ataması gerekebilir.").font(.caption).foregroundStyle(.secondary)
            }}
            GlassCard {VStack(alignment:.leading,spacing:16) {
                HStack {SectionTitle(title:"Kendi profillerim",subtitle:"Seçili katmanın tuş, encoder ve ışık ayarlarını kaydet");Spacer();Image(systemName:"bookmark.square").font(.title2).foregroundStyle(accent)}
                HStack {TextField("Profil adı · örn. Blender kendi düzenim",text:$profileName);Button("Yeni profil kaydet"){saveNewProfile()}.buttonStyle(.borderedProminent).disabled(profileName.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty)}.textFieldStyle(.roundedBorder)
                if profiles.isEmpty {
                    Label("Henüz kayıtlı profil yok. Bir hazır profili uygula, düzenle ve buradan kaydet.",systemImage:"tray").font(.caption).foregroundStyle(.secondary).padding(.vertical,8)
                } else {
                    Picker("Kayıtlı profil",selection:$selectedProfile){Text("Profil seç").tag(Optional<UUID>.none);ForEach(profiles){p in Text(p.name).tag(Optional(p.id))}}
                    if let profile=profiles.first(where:{$0.id==selectedProfile}) {
                        HStack(alignment:.top,spacing:20) {
                            VStack(alignment:.leading,spacing:8) {
                                Text(profile.name.uppercased()+" · ÖNİZLEME").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                                LazyVGrid(columns:Array(repeating:GridItem(.flexible(),spacing:5),count:3),spacing:5){ForEach(0..<12){index in let key=(index%3)*4+index/3;Text(profile.layer.keys[key].isEmpty ? "—" : profile.layer.keys[key]).font(.caption2).lineLimit(1).frame(maxWidth:.infinity,minHeight:27).padding(.horizontal,4).background(.white.opacity(0.05),in:RoundedRectangle(cornerRadius:6))}}
                            }.frame(maxWidth:440)
                            VStack(alignment:.leading,spacing:10) {
                                Text("Encoder · ışık · iki kademe · oyun · SOCD dahil").font(.caption).foregroundStyle(.secondary)
                                Toggle("Kayıtlı ışığı uygula",isOn:$restoreLight)
                                if profile.hall?.count == 12 {Toggle("Ortak Hall ayarlarını da geri yükle",isOn:$restoreHall);Text("Hall ayarları tüm katmanları etkiler; geri alınabilir.").font(.caption).foregroundStyle(.secondary)} else {Text("Bu eski profilde Hall ayarı kaydı yok.").font(.caption).foregroundStyle(.secondary)}
                                Text("Uygulamak seçili katmanın ayarlarını bu profille değiştirir.").font(.caption).foregroundStyle(.secondary)
                                Button(action:{applySavedProfile(profile)}) {Text(verbatim:UserDefaults.standard.string(forKey:"studio-language") == "en" ? "Apply to Layer \(model.layer+1)" : "Layer \(model.layer+1)’e uygula")}.buttonStyle(.borderedProminent)
                                HStack {Button("Seçili katmanla güncelle"){updateProfile(profile.id)};Button("Profili sil",role:.destructive){confirmDelete=true}}
                                if undoIndex == model.layer && undoLayer != nil {Button("Katman değişikliğini geri al"){undoPreset()}}
                            }
                        }
                    }
                }
                Text("Profil güncelleme, kayıtlı profili seçili katmanın şu anki ayarlarıyla değiştirir. Oyun ve SOCD katmana özeldir. Hall ayarları yeni profillere kaydedilir ve istenirse geri yüklenir; kalibrasyonlar cihaza özeldir ve profile dahil değildir. Profiller cihazda uygulanmaz.").font(.caption).foregroundStyle(.secondary)
            }}
        }
        .onAppear(perform:loadProfiles).onReceive(NotificationCenter.default.publisher(for:Notification.Name("StudioProfilesRestored"))){_ in loadProfiles()}
        .confirmationDialog("Kayıtlı profil silinsin mi?",isPresented:$confirmDelete,titleVisibility:.visible){Button("Profili sil",role:.destructive){deleteProfile()};Button("Vazgeç",role:.cancel){}}
    }
    private func modifierBinding(_ bit:UInt8)->Binding<Bool> {
        Binding(get:{keyboardModifiers & bit != 0},set:{enabled in
            if enabled {keyboardModifiers |= bit} else {keyboardModifiers &= ~bit}
        })
    }
    private func keyboardMenu(second:Bool)->some View {
        Menu {
            ForEach(keyboardGroups,id:\.0){group,keys in
                Menu(LocalizedStringKey(group)){ForEach(keys,id:\.1){name,code in Button(LocalizedStringKey(name)){setKeyboardTask(name,code:code,second:second)}}}
            }
        } label:{Label("Klavye tuşu seç",systemImage:"keyboard")}
    }
    private func taskBindingMenu(second:Bool)->some View {
        Menu {
            ForEach(keyboardGroups,id:\.0){group,keys in
                Menu(LocalizedStringKey(group)){ForEach(keys,id:\.1){name,code in Button(LocalizedStringKey(name)){bindTask(code:code,second:second)}}}
            }
        } label:{Label("Göreve tuş bağla",systemImage:"link")}
        .help("Görev adını koruyarak gönderilecek klavye tuşunu seç")
    }
    private func assignmentName(_ assignment:KeyboardAssignment)->String {
        let names:[(UInt8,String)]=[(8,"⌘"),(4,"⌥"),(1,"⌃"),(2,"⇧")]
        let modifiers=names.filter{assignment.modifiers & $0.0 != 0}.map{$0.1}
        let name=keyboardGroups.flatMap{$0.1}.first(where:{$0.1 == assignment.keyCode})?.0 ?? "HID \(assignment.keyCode)"
        return (modifiers+[name]).joined(separator:" ")
    }
    private func bindTask(code:UInt8,second:Bool) {
        var layer=model.config.layers[model.layer]
        var values=layer.keyBehaviors?.count == 12 ? layer.keyBehaviors! : Array(repeating:KeyBehavior(),count:12)
        let label=second ? values[model.selected].secondAction : layer.keys[model.selected]
        guard !label.isEmpty else{return}
        let assignment=KeyboardAssignment(keyCode:code,modifiers:keyboardModifiers,label:label)
        if second {values[model.selected].secondaryKey=assignment} else {values[model.selected].primaryKey=assignment}
        layer.keyBehaviors=values;model.config.layers[model.layer]=layer
    }
    private func setKeyboardTask(_ name:String,code:UInt8,second:Bool) {
        let modifierNames:[(UInt8,String)]=[(8,"⌘"),(4,"⌥"),(1,"⌃"),(2,"⇧")]
        let prefix=modifierNames.filter{keyboardModifiers & $0.0 != 0}.map{ $0.1 }.joined(separator:" ")
        let label=prefix.isEmpty ? name : prefix+" "+name
        let assignment=KeyboardAssignment(keyCode:code,modifiers:keyboardModifiers,label:label)
        var layer=model.config.layers[model.layer]
        var values=layer.keyBehaviors?.count == 12 ? layer.keyBehaviors! : Array(repeating:KeyBehavior(),count:12)
        if second {values[model.selected].secondAction=label;values[model.selected].secondaryKey=assignment}
        else {layer.keys[model.selected]=label;values[model.selected].primaryKey=assignment}
        layer.keyBehaviors=values;model.config.layers[model.layer]=layer
    }
    private func setSpecialTask(_ name:String,second:Bool) {
        var layer=model.config.layers[model.layer]
        var values=layer.keyBehaviors?.count == 12 ? layer.keyBehaviors! : Array(repeating:KeyBehavior(),count:12)
        if second {values[model.selected].secondAction=name;values[model.selected].secondaryKey=nil}
        else {layer.keys[model.selected]=name;values[model.selected].primaryKey=nil}
        layer.keyBehaviors=values;model.config.layers[model.layer]=layer
    }
    private func taskDisplay(_ value:String)->some View {
        Text(verbatim:studioDisplay(value.isEmpty ? "Henüz görev seçilmedi" : value))
            .foregroundStyle(value.isEmpty ? .secondary : .primary)
            .frame(maxWidth:.infinity,alignment:.leading).padding(.horizontal,8).padding(.vertical,6)
            .background(.white.opacity(0.06),in:RoundedRectangle(cornerRadius:6))
            .textSelection(.disabled)
    }
    private var encoderHold:EncoderHold {model.config.layers[model.layer].encoderHold ?? EncoderHold()}
    private func encoderHoldBinding<T>(_ path:WritableKeyPath<EncoderHold,T>)->Binding<T> {
        Binding(get:{encoderHold[keyPath:path]},set:{value in
            var layer=model.config.layers[model.layer];var hold=layer.encoderHold ?? EncoderHold()
            hold[keyPath:path]=value;layer.encoderHold=hold;model.config.layers[model.layer]=layer
        })
    }
    // These choices have defined HID reports; host support still varies.
    private let encoderRotationActions:[(String,[String])] = [
        ("Ses",["Ses azalt","Ses artır"]),
        ("Ekran",["Ekran parlaklığını azalt","Ekran parlaklığını artır"]),
        ("Medya",["Önceki parça","Sonraki parça"]),
        ("Fare tekerleği",["Yukarı kaydır","Aşağı kaydır"])
    ]
    private let encoderPressActions:[(String,[String])] = [
        ("Medya",["Oynat / Duraklat","Önceki parça","Sonraki parça","Oynatmayı durdur"]),
        ("Ses",["Sesi kapat / aç"])
    ]
    private func encoderAction(_ title:String,value:Binding<String>,rotation:Bool=false)->some View {
        HStack {
            Text(LocalizedStringKey(title)).font(.caption).foregroundStyle(.secondary).frame(width:105,alignment:.leading)
            Menu {
                Button("Görevi temizle"){value.wrappedValue=""}
                Divider()
                if !rotation {
                    Menu("Klavye tuşları") {
                        ForEach(Array("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789").map(String.init),id:\.self){key in Button(key){value.wrappedValue=key}}
                    }
                }
                ForEach(rotation ? encoderRotationActions : encoderPressActions,id:\.0){group,actions in
                    Menu(LocalizedStringKey(group)){ForEach(actions,id:\.self){action in Button(LocalizedStringKey(action)){value.wrappedValue=action}}}
                }
            } label: {
                HStack {Text(verbatim:studioDisplay(value.wrappedValue.isEmpty ? "Görev seç" : value.wrappedValue)).lineLimit(1);Spacer()}.frame(maxWidth:.infinity,alignment:.leading)
            }
        }
    }
    private var behavior:KeyBehavior { let values=model.config.layers[model.layer].keyBehaviors;return values?.count == 12 ? values![model.selected] : KeyBehavior() }
    private func behaviorBinding<T>(_ path:WritableKeyPath<KeyBehavior,T>)->Binding<T> {Binding(get:{behavior[keyPath:path]},set:{value in var layer=model.config.layers[model.layer];var values=layer.keyBehaviors?.count == 12 ? layer.keyBehaviors! : Array(repeating:KeyBehavior(),count:12);values[model.selected][keyPath:path]=value;layer.keyBehaviors=values;model.config.layers[model.layer]=layer})}
    private func stageIndicator(_ title:String,active:Bool)->some View {Label(title,systemImage:active ? "checkmark.circle.fill" : "circle").font(.caption.bold()).foregroundStyle(active ? accent : .secondary).frame(maxWidth:.infinity,alignment:.leading)}
    private func applyPreset(){undoHall=nil;undoLayer=model.config.layers[model.layer];undoIndex=model.layer;var layer=model.config.layers[model.layer];layer.name=preset.id;layer.presetID=preset.id;layer.keys=preset.keys;layer.keyBehaviors=nil;layer.left=preset.encoder[0];layer.right=preset.encoder[1];layer.push=preset.encoder[2];layer.encoderHold=nil;if ["Minecraft","FPS","Euro Truck Simulator 2","Assetto Corsa","Microsoft Flight Simulator","Farming Simulator"].contains(preset.id) { var game=GameSettings();game.enabled=true;if ["Euro Truck Simulator 2","Assetto Corsa","Farming Simulator"].contains(preset.id) {game.kind="driving"};if preset.id == "Microsoft Flight Simulator" {game.kind="flight"};layer.game=game } else { var game=layer.game ?? GameSettings();game.enabled=false;layer.game=game };model.config.layers[model.layer]=layer}
    private func undoPreset(){if undoIndex == model.layer,let previous=undoLayer {model.config.layers[model.layer]=previous;if let hall=undoHall {model.config.hall=hall};undoHall=nil;undoLayer=nil;undoIndex=nil}}
    private func loadProfiles(){if let data=UserDefaults.standard.data(forKey:profileStorageKey),let saved=try? JSONDecoder().decode([SavedLayerProfile].self,from:data){profiles=saved.filter{$0.layer.keys.count==12}}}
    private func persistProfiles(){if let data=try? JSONEncoder().encode(profiles){UserDefaults.standard.set(data,forKey:profileStorageKey)}}
    private func saveNewProfile(){let name=profileName.trimmingCharacters(in:.whitespacesAndNewlines);guard !name.isEmpty else{return};let profile=SavedLayerProfile(name:name,layer:model.config.layers[model.layer],hall:model.config.hall);profiles.append(profile);selectedProfile=profile.id;profileName="";persistProfiles()}
    private func updateProfile(_ id:UUID){guard let index=profiles.firstIndex(where:{$0.id==id}) else{return};profiles[index].layer=model.config.layers[model.layer];profiles[index].hall=model.config.hall;persistProfiles()}
    private func deleteProfile(){guard let id=selectedProfile else{return};profiles.removeAll{$0.id==id};selectedProfile=nil;persistProfiles()}
    private func applySavedProfile(_ profile:SavedLayerProfile){undoLayer=model.config.layers[model.layer];undoIndex=model.layer;undoHall=restoreHall && profile.hall?.count == 12 ? model.config.hall : nil;var replacement=profile.layer;if !restoreLight {replacement.light=model.config.layers[model.layer].light};model.config.layers[model.layer]=replacement;if restoreHall,let hall=profile.hall,hall.count == 12 {model.config.hall=hall}}
}
struct GameProfileView:View {
    @ObservedObject var model:StudioModel
    private var game:GameSettings { model.config.layers[model.layer].game ?? GameSettings() }
    private func gameBinding<T>(_ path:WritableKeyPath<GameSettings,T>)->Binding<T> {
        Binding(get:{game[keyPath:path]},set:{value in
            let old=game
            if path == \GameSettings.runPoint, let proposed=value as? Double,
               let b=model.config.layers[model.layer].keyBehaviors,b.count == 12,
               [old.forward,old.left,old.back,old.right].contains(where:{b[$0].mode == "twoStage" && b[$0].secondAction == "Koş · Shift" && proposed <= model.config.hall[$0].point}) {return}
            var g=old;g[keyPath:path]=value
            var layer=model.config.layers[model.layer];layer.game=g
            let mappings=[(old.forward,g.forward,"İleri · W"),(old.left,g.left,"Sol · A"),(old.back,g.back,"Geri · S"),(old.right,g.right,"Sağ · D")]
            if Set([g.forward,g.left,g.back,g.right]).count == 4 {
                for (before,after,name) in mappings where before != after && layer.keys[before] == name && (layer.keys[after].isEmpty || layer.keys[after] == name) {layer.keys[before]="";layer.keys[after]=name}
            }
            let newKeys=[g.forward,g.left,g.back,g.right]
            let oldKeys=[old.forward,old.left,old.back,old.right]
            if var socd=layer.socdSettings {
                if socd.follows == nil && Set(oldKeys).count == 4 {
                    if socd.a == old.left && socd.b == old.right {socd.follows="ad"}
                    else if socd.a == old.forward && socd.b == old.back {socd.follows="ws"}
                }
                if Set(newKeys).count == 4 {
                    if socd.follows == "ad" {socd.a=g.left;socd.b=g.right}
                    else if socd.follows == "ws" {socd.a=g.forward;socd.b=g.back}
                }
                layer.socdSettings=socd
            }
            if var b=layer.keyBehaviors, b.count == 12, Set(newKeys).count == 4 {
                let owned=old.managedRunKeys ?? (Set(oldKeys).count == 4 && oldKeys.allSatisfy{b[$0].mode == "twoStage" && b[$0].secondAction == "Koş · Shift"} ? oldKeys : nil)
                if let owned=owned,owned.count == 4,owned.allSatisfy({(0..<12).contains($0)}),Set(owned).count == 4 {
                    let safe=owned.allSatisfy { b[$0].mode == "twoStage" && b[$0].secondAction == "Koş · Shift" } && newKeys.allSatisfy { key in
                        (owned.contains(key) || (b[key].secondAction.isEmpty && b[key].mode == "normal")) && g.runPoint > model.config.hall[key].point
                    }
                    if safe {
                        for key in owned where !newKeys.contains(key) && b[key].mode == "twoStage" && b[key].secondAction == "Koş · Shift" {b[key].mode="normal";b[key].secondAction=""}
                        for key in newKeys {b[key].mode="twoStage";b[key].secondAction="Koş · Shift";b[key].secondPoint=g.runPoint}
                        g.managedRunKeys=newKeys;layer.game=g;layer.keyBehaviors=b
                    }
                }
            }
            if path == \GameSettings.runPoint, var b=layer.keyBehaviors,b.count == 12 {
                for i in [g.forward,g.left,g.back,g.right] where b[i].mode == "twoStage" && b[i].secondAction == "Koş · Shift" {b[i].secondPoint=g.runPoint}
                layer.keyBehaviors=b
            }
            model.config.layers[model.layer]=layer
        })
    }
    private var driving:Bool {game.kind == "driving"}
    private var flying:Bool {game.kind == "flight"}
    private var analog:Bool {driving || flying}
    private var forward:Int {game.forward}; private var left:Int {game.left}; private var back:Int {game.back}; private var right:Int {game.right}
    private var deadzone:Double {game.deadzone}; private var runPoint:Double {game.runPoint}
    @State private var preview=false
    @State private var confirmDirections=false
    @State private var undoGameLayer:LayerConfig?
    @State private var undoGameIndex:Int?
    @State private var previewKey=4
    @State private var depth=45.0
    private var assignments:[Int] { [forward,left,back,right] }
    private var conflict:Bool { Set(assignments).count != 4 }
    private var directionsMismatch:Bool { let keys=model.config.layers[model.layer].keys;return [(forward,"İleri · W"),(left,"Sol · A"),(back,"Geri · S"),(right,"Sağ · D")].contains{keys[$0.0] != $0.1} }
    private var stageConflict:Bool { assignments.contains { runPoint <= model.config.hall[$0].point } }
    private var runStagesMismatch:Bool { let b=model.config.layers[model.layer].keyBehaviors;guard b?.count == 12 else{return true};return assignments.contains{b![$0].mode != "twoStage" || b![$0].secondAction != "Koş · Shift" || b![$0].secondPoint != runPoint} }
    private var stageActionConflict:Bool { let b=model.config.layers[model.layer].keyBehaviors;guard b?.count == 12 else{return false};return assignments.contains{!b![$0].secondAction.isEmpty && b![$0].secondAction != "Koş · Shift"} }
    private func amount(_ key:Int)->Double? { preview ? (previewKey == key ? depth : 0) : model.percent(key) }
    private var hasSignal:Bool { preview || assignments.allSatisfy { model.percent($0) != nil } }
    private var x:Double {
        let raw=((amount(right) ?? 0)-(amount(left) ?? 0))/100
        guard driving else{return raw}
        let zone=deadzone/100
        guard abs(raw)>zone else{return 0}
        return (raw < 0 ? -1 : 1)*min(1,(abs(raw)-zone)/(1-zone))
    }
    private var y:Double { ((amount(forward) ?? 0)-(amount(back) ?? 0))/100 }
    private var magnitude:Double { min(1,sqrt(x*x+y*y))*100 }
    private var maxKeyDepth:Double { assignments.map{amount($0) ?? 0}.max() ?? 0 }
    private var status:String { if !game.enabled {return "Oyun modu kapalı"}; if !hasSignal {return "Veri bekleniyor"}; if conflict {return "Yönleri ayır"}; if flying {return magnitude <= deadzone ? "Kumanda merkezde" : "Uçuş kumandası"}; if driving {return x == 0 ? "Direksiyon ortada" : x < 0 ? "Sola dönüyor" : "Sağa dönüyor"}; if magnitude <= deadzone {return "Bekliyor"}; return maxKeyDepth >= runPoint ? "Koşma eşiğinde" : "Yürüme" }
    var body:some View {
        HStack(alignment:.top,spacing:16) {
            GlassCard {VStack(alignment:.leading,spacing:18) {
                SectionTitle(title:"Katmana özel oyun ayarları",subtitle:"Seçili katman ve kayıtlı profille birlikte saklanır")
                Picker("Katman",selection:$model.layer){ForEach(0..<4){Text("Layer \($0+1)").tag($0)}}.labelsHidden().pickerStyle(.segmented)
                Toggle("Bu katmanda oyun modu",isOn:gameBinding(\.enabled))
                Text("Oyun modu yerel bir taslaktır; kapalıyken ayarlar korunur.").font(.caption).foregroundStyle(.secondary)
                Divider()
                SectionTitle(title:flying ? "Uçuş kontrolleri" : driving ? "Sürüş kontrolleri" : "Yön atamaları",subtitle:flying ? "Pitch ve roll eksenleri için tuş seç" : driving ? "Gaz, fren ve analog direksiyon için tuş seç" : "12 Hall tuşundan dört farklı tuş seç")
                HStack(spacing:12){ keyPicker(flying ? "Pitch ileri" : driving ? "Gaz" : "W · İleri",gameBinding(\.forward));keyPicker(flying ? "Roll sol" : driving ? "Direksiyon sol" : "A · Sol",gameBinding(\.left)) }
                HStack(spacing:12){ keyPicker(flying ? "Pitch geri" : driving ? "Fren" : "S · Geri",gameBinding(\.back));keyPicker(flying ? "Roll sağ" : driving ? "Direksiyon sağ" : "D · Sağ",gameBinding(\.right)) }
                if conflict {Label("Her yön için farklı bir tuş seç.",systemImage:"exclamationmark.triangle").font(.caption).foregroundStyle(.orange)}
                Divider()
                Text(flying ? "UÇUŞ KUMANDASI" : driving ? "DİREKSİYON AYARLARI" : "HAREKET EŞİKLERİ").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                RangeControl(label:"Ölü bölge",value:gameBinding(\.deadzone),bounds:0...50)
                if !analog {RangeControl(label:"Koşma başlangıcı",value:gameBinding(\.runPoint),bounds:1...100)
                if deadzone >= runPoint {Text("Koşma eşiği ölü bölgeden büyük olmalı.").font(.caption).foregroundStyle(.orange)}}
                Text("Oyun modu açıkken joystick yönleri bu seçimleri kullanır. Katman görevlerini değiştirmek için aşağıdaki düğmeyi kullan.").font(.caption).foregroundStyle(.secondary)
                if directionsMismatch {Label("Yön görevleri oyun yönleriyle eşleşmiyor. Otomatik taşıma yalnız boş hedeflerde yapılır.",systemImage:"exclamationmark.triangle").font(.caption).foregroundStyle(.orange)}
                Button("Yön görevlerini katmana uygula") { confirmDirections=true }.disabled(conflict)
                if !analog {
                if runStagesMismatch {Text("Yönlerin koşma görevleri bu eşikle tam eşleşmiyor.").font(.caption).foregroundStyle(.secondary)}
                Button("Yönlere koşma görevini ata") { applyRunStages() }.disabled(conflict || deadzone >= runPoint || stageConflict || stageActionConflict)
                if stageActionConflict {Text("Bir yönün ikinci kademesinde başka bir görev var; önce o görevi düzenle.").font(.caption).foregroundStyle(.orange)}
                if stageConflict {Text("Koşma eşiği, yön tuşlarının her birinin Hall aktivasyon eşiğinden büyük olmalı.").font(.caption).foregroundStyle(.orange)}
                Text("Koş · Shift ikinci kademe olarak atanır. Koşma eşiği değişirse bu görevlerin eşiği güncellenir. Hall ilk eşikleri ortaktır.").font(.caption).foregroundStyle(.secondary)
                HStack {Button("A/D çiftini SOCD’ye aktar") { setSOCD(left,right) };Button("W/S çiftini SOCD’ye aktar") { setSOCD(forward,back) }}.disabled(conflict)
                Text("SOCD seçili katmana aittir; seçilen çift bu katmandaki önceki çifti değiştirir.").font(.caption).foregroundStyle(.secondary)
                } else {Text(flying ? "Roll ve pitch karşılıklı tuşların basılma farkından hesaplanır. Eksen yönlerini oyunda ayarlayabilirsin. Motor gücü bu önizlemede analog eksen değildir." : "Direksiyon = sağ basılma − sol basılma; ölü bölgeden sonra tam aralığa ölçeklenir. Gaz ve fren ayrı tuşlardır; test köprüsü etkinken eksen olarak gönderilir.").font(.caption).foregroundStyle(.secondary)}
                if undoGameIndex == model.layer && undoGameLayer != nil {Button("Son işlemi geri al") {undoGameChange()}}
                Label("Ayarlar bu Mac’te taslaktır.",systemImage:"internaldrive").font(.caption).foregroundStyle(.secondary)
            }}
            GlassCard {VStack(alignment:.leading,spacing:16) {
                HStack{SectionTitle(title:flying ? "Uçuş kumandası önizlemesi" : driving ? "Direksiyon önizlemesi" : "Hareket önizlemesi",subtitle:preview ? "Sanal tuş ve basılma" : "Kalibre edilmiş sensör verileri");Spacer();Text("DEMO").font(.caption2.bold()).foregroundStyle(accent).padding(7).background(accent.opacity(0.12),in:Capsule())}
                Picker("Kaynak",selection:$preview){Text("Simülasyon").tag(true);Text("Canlı veri").tag(false)}.labelsHidden().pickerStyle(.segmented)
                if driving {
                    Toggle("CrossOver’a analog test verisi gönder",isOn:$model.driveOutputEnabled).toggleStyle(.switch)
                    Text(model.driveBridgeStatus).font(.caption).foregroundStyle(model.driveBridgeStatus == "Köprüye canlı veri gönderiliyor" ? accent : .secondary)
                    Text("Dört tuş kalibre edilmeli. Köprü test aşamasındadır; bu gösterge oyunun kumandayı algıladığını doğrulamaz.").font(.caption2).foregroundStyle(.secondary)
                }

                ZStack {
                    RoundedRectangle(cornerRadius:16).fill(Color.black.opacity(0.18))
                    Circle().stroke(.white.opacity(0.10),lineWidth:1).frame(width:150,height:150)
                    Circle().stroke(.white.opacity(0.08),lineWidth:1).frame(width:90,height:90)
                    Rectangle().fill(.white.opacity(0.08)).frame(width:1,height:160)
                    Rectangle().fill(.white.opacity(0.08)).frame(width:160,height:1)
                    Circle().fill(accent.opacity(0.18)).frame(width:38,height:38).overlay(Circle().fill(accent).frame(width:12,height:12)).offset(x:hasSignal && !conflict && game.enabled && (driving || magnitude > deadzone) ? x*65 : 0,y:hasSignal && !conflict && game.enabled && magnitude > deadzone ? -y*65 : 0)
                }.frame(height:205)
                HStack {VStack(alignment:.leading,spacing:4){Text(LocalizedStringKey(status)).font(.title2.bold()).foregroundStyle(accent);Text(verbatim:studioDisplay(flying ? (hasSignal ? String(format:"Roll %+.0f%% · Pitch %+.0f%%",x*100,y*100) : "Roll — · Pitch —") : driving ? (hasSignal ? String(format:"%+.0f%% · direksiyon", x*100) : "— · direksiyon") : "\(hasSignal ? String(format:"%.0f%%",magnitude) : "—") · hareket miktarı")).font(.caption).foregroundStyle(.secondary)};Spacer();Image(systemName:flying ? "airplane" : driving ? "steeringwheel" : status == "Koşma eşiğinde" ? "figure.run" : "figure.walk").font(.title).foregroundStyle(accent.opacity(0.75))}
                if driving {HStack {Text("Gaz: \(amount(forward).map{String(format:"%.0f%%",$0)} ?? "—")");Spacer();Text("Fren: \(amount(back).map{String(format:"%.0f%%",$0)} ?? "—")")}.font(.caption).foregroundStyle(.secondary)}
                if preview {
                    Picker("Yön",selection:$previewKey){Text("W").tag(forward);Text("A").tag(left);Text("S").tag(back);Text("D").tag(right)}.labelsHidden().pickerStyle(.segmented).disabled(conflict)
                    RangeControl(label:"Örnek basılma",value:$depth)
                    Text(flying ? "Simülasyon tek tuşu gösterir. Canlı modda her eksen iki tuşun farkını kullanır." : driving ? "Simülasyon tek tuşu gösterir. Canlı modda iki direksiyon tuşunun farkı alınır." : "Simülasyon tek yönü gösterir. Koşma eşiği her tuşun derinliğine ayrı bakar; gerçek görevin atandığını garanti etmez.").font(.caption).foregroundStyle(.secondary)
                } else {Text("Canlı önizleme için dört yönün de kalibre edilmesi gerekir.").font(.caption).foregroundStyle(.secondary)}
                Text("Analog çıkış yalnız CrossOver test köprüsü etkinken denenir; oyunda eksenlerin ayrıca atanması gerekir.").font(.caption).foregroundStyle(.secondary)
            }}.frame(minWidth:280,idealWidth:355,maxWidth:410)
        }.onAppear {if !assignments.contains(previewKey) {previewKey=forward}}.onChange(of:assignments){_,new in if !new.contains(previewKey) {previewKey=forward}}
        .confirmationDialog("Yön görevleri güncellensin mi?",isPresented:$confirmDirections,titleVisibility:.visible) {Button("Görevleri uygula"){applyDirections()};Button("Vazgeç",role:.cancel){}} message:{Text("Seçili tuşların görevleri W/A/S/D ile değiştirilir; eski aynı yön görevleri temizlenir.")}
    }
    private func snapshotGameChange(){undoGameLayer=model.config.layers[model.layer];undoGameIndex=model.layer}
    private func undoGameChange(){if undoGameIndex == model.layer,let layer=undoGameLayer {model.config.layers[model.layer]=layer;undoGameLayer=nil;undoGameIndex=nil}}
    private func setSOCD(_ a:Int,_ b:Int) {snapshotGameChange();model.config.layers[model.layer].socdSettings=SOCDSettings(enabled:true,a:a,b:b,follows:(a == left && b == right) ? "ad" : "ws")}
    private func applyDirections() {
        snapshotGameChange()
        var layer=model.config.layers[model.layer]
        let mappings=[(forward,"İleri · W"),(left,"Sol · A"),(back,"Geri · S"),(right,"Sağ · D")]
        let names=Set(mappings.map{$0.1})
        for i in layer.keys.indices where names.contains(layer.keys[i]) {layer.keys[i]=""}
        for (key,name) in mappings {layer.keys[key]=name}
        model.config.layers[model.layer]=layer
    }
    private func applyRunStages() {
        snapshotGameChange()
        var layer=model.config.layers[model.layer]
        var behaviors=layer.keyBehaviors?.count == 12 ? layer.keyBehaviors! : Array(repeating:KeyBehavior(),count:12)
        for key in assignments {behaviors[key].mode="twoStage";behaviors[key].secondPoint=runPoint;behaviors[key].secondAction="Koş · Shift"}
        var updated=game;updated.managedRunKeys=assignments;layer.game=updated
        layer.keyBehaviors=behaviors;model.config.layers[model.layer]=layer
    }
    func keyPicker(_ title:String,_ value:Binding<Int>)->some View { VStack(alignment:.leading,spacing:6){Text(LocalizedStringKey(title)).font(.caption.bold()).foregroundStyle(.secondary);Picker(title,selection:value){ForEach(0..<12){Text("KEY \($0)").tag($0)}}.labelsHidden().frame(maxWidth:.infinity)} }
}
struct HallView:View {
    @ObservedObject var model:StudioModel
    @State private var previewDepth=35.0
    @State private var allKeys=false
    @State private var previousHall:[HallConfig]?
    var body:some View {
        HStack(alignment:.top,spacing:16) {
            GlassCard {
                VStack(alignment:.leading,spacing:18) {
                    HStack { SectionTitle(title:"Tetikleme ayarları",subtitle:"Yazdır ile seçili katmanın görevlerine kaydedilir"); Spacer(); if !allKeys {Picker("Tuş",selection:$model.selected){ForEach(0..<12){Text("KEY \($0)").tag($0)}}.frame(width:145)} }
                    Picker("Uygulama alanı",selection:$allKeys){Text("Seçili tuş").tag(false);Text("Tüm tuşlar").tag(true)}.labelsHidden().pickerStyle(.segmented)
                    Text(allKeys ? "Bir ayarı değiştirdiğinde o değer tüm 12 tuşa uygulanır. Gösterilen değerler KEY \(model.selected) tuşuna aittir; diğer tuşların değerleri farklı olabilir." : "Değişiklikler yalnızca seçili tuşa uygulanır.").font(.caption).foregroundStyle(.secondary)
                    Picker("Tetikleme",selection:hallBinding(\.mode)){Text("Geleneksel").tag("Geleneksel");Text("Rapid Trigger").tag("Rapid Trigger")}.pickerStyle(.segmented)
                    RangeControl(label:"Aktivasyon noktası",value:hallBinding(\.point))
                    RangeControl(label:"Histerezis",value:hallBinding(\.hysteresis),bounds:0...50)
                    Divider()
                    Text("RAPID TRIGGER").font(.caption2.bold()).tracking(1.1).foregroundStyle(accent)
                    RangeControl(label:"Basma hassasiyeti",value:hallBinding(\.press),bounds:1...100)
                    RangeControl(label:"Bırakma hassasiyeti",value:hallBinding(\.release),bounds:1...100)
                    RangeControl(label:"Alt sınır",value:hallBinding(\.min),bounds:0...99)
                    RangeControl(label:"Üst sınır",value:hallBinding(\.max),bounds:1...100)
                    if model.config.hall[model.selected].min>=model.config.hall[model.selected].max {Text("Alt sınır üst sınırdan küçük olmalı.").foregroundStyle(.orange).font(.caption)}
                    Divider()
                    HStack {Button("Seçili tuşun tüm ayarlarını 12 tuşa kopyala"){previousHall=model.config.hall;model.config.hall=Array(repeating:model.config.hall[model.selected],count:12)};if previousHall != nil {Button("Geri al"){if let saved=previousHall {model.config.hall=saved;previousHall=nil}}}}
                    Text("Kopyalama diğer tuşların Hall ayarlarını değiştirir; kalibrasyon değerlerine dokunmaz.").font(.caption).foregroundStyle(.secondary)
                }
            }
            GlassCard {
                VStack(alignment:.leading,spacing:16) {
                    SectionTitle(title:"Hareket önizlemesi",subtitle:"Seçili tuş · örnek basılma")
                    HStack(alignment:.center,spacing:20) {
                        ZStack(alignment:.bottom) {
                            Capsule().fill(.white.opacity(0.07)).frame(width:28,height:180)
                            Capsule().fill(LinearGradient(colors:[accent,accent.opacity(0.25)],startPoint:.top,endPoint:.bottom)).frame(width:28,height:max(4,180*previewDepth/100))
                        }
                        VStack(alignment:.leading,spacing:8) {
                            Text("KEY \(model.selected)").font(.caption.bold()).tracking(1).foregroundStyle(accent)
                            Text("\(Int(previewDepth))%").font(.system(size:42,weight:.semibold,design:.rounded)).monospacedDigit()
                            Text("Örnek hareket").font(.caption).foregroundStyle(.secondary)
                            Divider()
                            Text("Aktivasyon  ·  %\(Int(model.config.hall[model.selected].point))").font(.caption)
                            Text(previewDepth >= model.config.hall[model.selected].point ? "Eşiğin üzerinde" : "Eşiğin altında").font(.caption.bold()).foregroundStyle(previewDepth >= model.config.hall[model.selected].point ? accent : .secondary)
                        }
                    }.frame(maxWidth:.infinity,alignment:.center).padding(.vertical,10)
                    Slider(value:$previewDepth,in:0...100,step:1).tint(accent)
                    Text("Görsel yalnızca konumu ve aktivasyon eşiğini karşılaştırır; Rapid Trigger davranışını simüle etmez. Yüzdeler fiziksel mm değildir.").font(.caption).foregroundStyle(.secondary)
                }
            }.frame(minWidth:260,idealWidth:340,maxWidth:390)
        }
    }
    private func hallBinding<T>(_ path:WritableKeyPath<HallConfig,T>)->Binding<T> {
        Binding(get:{model.config.hall[model.selected][keyPath:path]},set:{value in
            var settings=model.config.hall
            if allKeys {for i in settings.indices {settings[i][keyPath:path]=value}} else {settings[model.selected][keyPath:path]=value}
            model.config.hall=settings
        })
    }
}
struct LightView:View {
    @ObservedObject var model:StudioModel
    @State private var color=Color(red:0.65,green:0.94,blue:0.8)
    @State private var show3D=true
    @AppStorage("custom-light-colors-v1") private var savedColorsJSON="[]"
    private let swatches:[(String,String)] = [("Mint","#A6F0CA"),("Buz","#88CFFF"),("Lavanta","#BC9BFF"),("Mercan","#FF8F86"),("Amber","#FFC36A"),("Beyaz","#FFFFFF")]
    var body:some View {
        HStack(alignment:.top,spacing:16) {
            GlassCard {
                VStack(alignment:.leading,spacing:14) {
                    HStack {SectionTitle(title:"Işık önizlemesi",subtitle:"Model üzerinde · yalnızca bu Mac’te");Spacer();Picker("Görünüm",selection:$show3D){Text("3B").tag(true);Text("2B").tag(false)}.labelsHidden().pickerStyle(.segmented).frame(width:100)}
                    Group {
                        if show3D {
                            MacropadModelView(model:model,clock:model.lightingClock).frame(height:345).clipShape(RoundedRectangle(cornerRadius:16))
                        } else {
                            KeyPad(model:model,showLabels:false).frame(maxWidth:.infinity,minHeight:345).padding(.vertical,8)
                        }
                    }.overlay(alignment:.topTrailing){DeviceConnectionBadge(model:model)}
                    Label { Text(LocalizedStringKey(show3D ? "Sürükle: döndür" : "Tuşa tıkla: önizle")) } icon: { Image(systemName:show3D ? "rotate.3d" : "hand.tap") }.font(.caption).foregroundStyle(.secondary)
                    Text("Renk ve efektler görsel taslaktır; gerçek LED parlaklığını temsil etmez.").font(.caption).foregroundStyle(.secondary)
                }
            }
            GlassCard {
                VStack(alignment:.leading,spacing:18) {
                    SectionTitle(title:"Aydınlatma stüdyosu",subtitle:"Katman ayarları cihaza canlı gönderilir")
                    if let device=model.deviceLayer {Text(verbatim:UserDefaults.standard.string(forKey:"studio-language") == "en" ? "Device: Layer \(device.layer+1) · Editing: Layer \(model.layer+1)" : "Cihaz: Layer \(device.layer+1) · Düzenlenen: Layer \(model.layer+1)").font(.caption).foregroundStyle(device.layer == model.layer ? Color.secondary : Color.orange)}
                    Picker("Katman",selection:$model.layer){ForEach(0..<4){Text("Layer \($0+1)").tag($0)}}.labelsHidden().pickerStyle(.segmented)
                    Picker("Efekt",selection:effectBinding){ForEach(["Kapalı","Sabit","Nefes","Dalga","Gökkuşağı","Kayan ışık","Starlight","Yılan","Tepkisel","Basılan yerden dalga"],id:\.self){Text(LocalizedStringKey($0)).tag($0)}}
                    if ["Kayan ışık","Basılan yerden dalga","Tepkisel","Nefes","Dalga","Yılan"].contains(model.config.layers[model.layer].light.baseEffect) {
                        Toggle("Rengârenk",isOn:colorfulBinding).toggleStyle(.switch)
                        if model.config.layers[model.layer].light.colorful {
                            if model.config.layers[model.layer].light.baseEffect == "Kayan ışık" {
                                Picker("Renk düzeni",selection:sweepStyleBinding){Text("Karışık").tag("karisik");Text("Her tur tek renk").tag("tur")}.pickerStyle(.segmented)
                                Text(LocalizedStringKey(model.config.layers[model.layer].light.sweepColorStyle == "tur" ? "Son ışık söndükten sonra yeni renkle sonraki tur başlar." : "Renkler ışık boyunca birlikte değişir.")).font(.caption).foregroundStyle(.secondary)
                            } else if model.config.layers[model.layer].light.baseEffect == "Yılan" {Text("Son ışık söndükten sonra renk değişir.").font(.caption).foregroundStyle(.secondary)
                            } else if model.config.layers[model.layer].light.baseEffect == "Starlight" {Text("Her parlamada renk değişir.").font(.caption).foregroundStyle(.secondary)
                            } else {Text(LocalizedStringKey(["Nefes","Dalga"].contains(model.config.layers[model.layer].light.baseEffect) ? "Her tur tamamlandığında renk değişir." : model.config.layers[model.layer].light.baseEffect == "Tepkisel" ? "Her basış farklı renkte yanar." : "Her basış farklı renkte bir dalga oluşturur.")).font(.caption).foregroundStyle(.secondary)}
                        }
                    }
                    if model.config.layers[model.layer].light.baseEffect == "Starlight" {Text("Dört farklı renk, farklı zamanlarda parlar.").font(.caption).foregroundStyle(.secondary)}
                    if ["Tepkisel","Basılan yerden dalga"].contains(model.config.layers[model.layer].light.baseEffect) {
                        Text("Basılınca seçilen parlaklıkta yanar. Modelde bir tuşa tıklayarak deneyebilirsin.").font(.caption).foregroundStyle(.secondary)
                    }
                    if model.config.layers[model.layer].light.baseEffect != "Kapalı" {
                        Divider()
                        HStack {Text("Renk").font(.subheadline); Spacer();ColorPicker("Renk",selection:$color).labelsHidden().onChange(of:color){_,newColor in saveColor(newColor)}}.disabled(model.config.layers[model.layer].light.colorful)
                        HStack(spacing:10){ForEach(swatches,id:\.1){name,hex in Button {setColor(hex)} label: {Circle().fill(colorFor(hex)).frame(width:26,height:26).overlay(Circle().strokeBorder(.white.opacity(0.35),lineWidth:1))}.buttonStyle(.plain).help(Text(LocalizedStringKey(name))).accessibilityLabel(LocalizedStringKey(name))}}.disabled(model.config.layers[model.layer].light.colorful)
                        HStack {Text("Özel renkler").font(.subheadline.weight(.medium));Spacer();Button("Bu rengi kaydet"){saveCustomColor()}.font(.caption).disabled(model.config.layers[model.layer].light.colorful || customColors.contains(model.config.layers[model.layer].light.color.uppercased()))}
                        if customColors.isEmpty {Text("Kaydettiğin renkler tüm katmanlarda görünür.").font(.caption).foregroundStyle(.secondary)}
                        else {LazyVGrid(columns:Array(repeating:GridItem(.flexible(),spacing:6),count:6),spacing:8){ForEach(customColors,id:\.self){hex in VStack(spacing:3){Button {setColor(hex)} label:{Circle().fill(colorFor(hex)).frame(width:26,height:26).overlay(Circle().strokeBorder(.white.opacity(0.35),lineWidth:1))}.buttonStyle(.plain).disabled(model.config.layers[model.layer].light.colorful).accessibilityLabel(Text(verbatim:studioDisplay("Özel renk \(hex)")));Button {removeCustomColor(hex)} label:{Image(systemName:"xmark.circle.fill").font(.caption2).foregroundStyle(.secondary)}.buttonStyle(.plain).help(Text(verbatim:studioDisplay("\(hex) rengini kaldır"))).accessibilityLabel(Text(verbatim:studioDisplay("\(hex) rengini kaldır")))}}}}
                        RangeControl(label:"Parlaklık",value:$model.config.layers[model.layer].light.brightness)
                        Text(verbatim:studioDisplay("Etkin: %\(Int(model.effectiveLightBrightness)) · genel üst sınır: %\(Int(model.config.brightness))")).font(.caption).foregroundStyle(.secondary)
                        if model.config.layers[model.layer].light.baseEffect != "Sabit" {RangeControl(label:"Hız",value:speedBinding,bounds:1...100)}
                    }
                    Label("Canlı gönderim · kalıcı cihaz kaydı henüz yok",systemImage:"antenna.radiowaves.left.and.right").font(.caption).foregroundStyle(.secondary)
                }
            }.frame(minWidth:270,idealWidth:330,maxWidth:390)
        }.onAppear{loadColor()}.onChange(of:model.layer){_,_ in loadColor()}
    }
    private var effectBinding:Binding<String> {Binding(get:{model.config.layers[model.layer].light.baseEffect},set:{value in
        var light=model.config.layers[model.layer].light
        var speeds=light.effectSpeeds ?? [:]
        speeds[light.baseEffect]=light.speed
        light.effect=value;light.depthSensitive=false
        light.speed=speeds[value] ?? 50
        light.effectSpeeds=speeds
        model.config.layers[model.layer].light=light
    })}
    private var speedBinding:Binding<Double> {Binding(get:{model.config.layers[model.layer].light.speed},set:{value in
        var light=model.config.layers[model.layer].light
        light.speed=value
        var speeds=light.effectSpeeds ?? [:];speeds[light.baseEffect]=value;light.effectSpeeds=speeds
        model.config.layers[model.layer].light=light
    })}
    private var colorfulBinding:Binding<Bool> {Binding(get:{model.config.layers[model.layer].light.colorful},set:{value in var light=model.config.layers[model.layer].light;switch light.baseEffect {case "Kayan ışık":light.rainbowSweep=value;case "Basılan yerden dalga":light.rainbowWave=value;case "Tepkisel":light.rainbowReactive=value;case "Nefes":light.rainbowBreath=value;case "Dalga":light.rainbowFlow=value;case "Starlight":light.rainbowStarlight=value;case "Yılan":light.rainbowSnake=value;default:break};model.config.layers[model.layer].light=light})}
    private var sweepStyleBinding:Binding<String> {Binding(get:{model.config.layers[model.layer].light.sweepColorStyle ?? "karisik"},set:{value in model.config.layers[model.layer].light.sweepColorStyle=value})}
    private var customColors:[String] {(try? JSONDecoder().decode([String].self,from:Data(savedColorsJSON.utf8))) ?? []}
    private func saveCustomColor(){let hex=model.config.layers[model.layer].light.color.uppercased();var items=customColors;guard !items.contains(hex) else{return};items.append(hex);if let data=try? JSONEncoder().encode(items),let value=String(data:data,encoding:.utf8){savedColorsJSON=value}}
    private func removeCustomColor(_ hex:String){let items=customColors.filter{$0 != hex};if let data=try? JSONEncoder().encode(items),let value=String(data:data,encoding:.utf8){savedColorsJSON=value}}
    func colorFor(_ hex:String)->Color {let n=Int(hex.trimmingCharacters(in:CharacterSet(charactersIn:"#")),radix:16) ?? 0;return Color(red:Double((n>>16)&255)/255,green:Double((n>>8)&255)/255,blue:Double(n&255)/255)}
    func setColor(_ hex:String){model.config.layers[model.layer].light.color=hex;color=colorFor(hex)}
    func loadColor(){color=colorFor(model.config.layers[model.layer].light.color)}
    func saveColor(_ value:Color){if let c=NSColor(value).usingColorSpace(.sRGB){model.config.layers[model.layer].light.color=String(format:"#%02X%02X%02X",Int(c.redComponent*255),Int(c.greenComponent*255),Int(c.blueComponent*255))}}
}
struct PowerView:View {
    @ObservedObject var model:StudioModel
    var modeDescription:String {switch model.config.mode {case "Dijital":return "Dijital algılama ile uyanma ve düşük güçte bekleme hedeflenir.";case "Agresif":return "Analog sensörler sürekli açık tutulur; tam veri paketi hedeflenir.";default:return "Dijital algılama analog beslemeyi açar; hareketsizlikte uykuya dönülür."}}
    var packetDescription:String {switch model.config.mode {case "Dijital":return "5 bayt";case "Agresif":return "23 bayt";default:return "5 / 23 bayt"}}
    var body:some View {
        VStack(spacing:16) {RateStatusCard(model:model)
        HStack(alignment:.top,spacing:16) {
            GlassCard {VStack(alignment:.leading,spacing:20) {
                SectionTitle(title:"Güç davranışı",subtitle:"Algoritma taslağı · cihazda uygulanmaz")
                Picker("Mod",selection:$model.config.mode){ForEach(["Dijital","Hibrit","Agresif"],id:\.self){Text(LocalizedStringKey($0)).tag($0)}}.labelsHidden().pickerStyle(.segmented)
                HStack(alignment:.top,spacing:12){Image(systemName:"bolt.circle.fill").font(.title2).foregroundStyle(accent);VStack(alignment:.leading,spacing:5){Text(verbatim:studioDisplay(model.config.mode)+" "+studioDisplay("mod")).font(.headline);Text(LocalizedStringKey(modeDescription)).font(.caption).foregroundStyle(.secondary)}}.padding(16).frame(maxWidth:.infinity,alignment:.leading).background(accent.opacity(0.07),in:RoundedRectangle(cornerRadius:14))
                RangeControl(label:"Hibrit hareketsizlik süresi",value:$model.config.idle,bounds:1...240,unit:" dk")
                RangeControl(label:"Temel parlaklık",value:$model.config.brightness)
                Text("Katman parlaklığı için üst sınırdır; 2B ve 3B önizlemede uygulanır.").font(.caption).foregroundStyle(.secondary)
                Text("Seçimler yerel taslak olarak saklanır. Pil seviyesi ve güç tüketimi ölçülmüyor.").font(.caption).foregroundStyle(.secondary)
            }}
            GlassCard {VStack(alignment:.leading,spacing:16) {
                SectionTitle(title:"Planlanan sistem",subtitle:"Paylaşılan C6 algoritmasına göre")
                HStack {Label("Paket",systemImage:"shippingbox");Spacer();Text(LocalizedStringKey(packetDescription)).font(.headline.monospacedDigit()).foregroundStyle(accent)}
                Text("Hibrit modda dijital bekleme ve analog aktif durum için farklı paketler planlanıyor.").font(.caption).foregroundStyle(.secondary)
                Divider()
                Text("PİL BİLDİRİMLERİ").font(.caption2.bold()).tracking(1).foregroundStyle(accent)
                ForEach([(50,"6 LED uyarısı"),(25,"3 LED · parlaklık sınırı"),(15,"Katman LED uyarısı"),(5,"Kritik kapanma")],id:\.0){threshold,detail in HStack{Text("%\(threshold)").font(.caption.bold().monospacedDigit()).frame(width:42,alignment:.leading);Text(LocalizedStringKey(detail)).font(.caption).foregroundStyle(.secondary);Spacer()}.padding(.vertical,4)}
                Text("Bunlar hedef davranışlardır; mevcut test platformunda pil okunmaz.").font(.caption).foregroundStyle(.secondary)
            }}.frame(minWidth:270,idealWidth:330,maxWidth:390)
        }}
    }
}
struct AdvancedView:View { @ObservedObject var model:StudioModel
    private var game:GameSettings {model.config.layers[model.layer].game ?? GameSettings()}
    private var socd:SOCDSettings {model.config.layers[model.layer].socdSettings ?? SOCDSettings(enabled:model.config.socd,a:model.config.socdA,b:model.config.socdB)}
    private func socdBinding<T>(_ path:WritableKeyPath<SOCDSettings,T>)->Binding<T> {Binding(get:{socd[keyPath:path]},set:{var value=socd;value[keyPath:path]=$0;if path == \SOCDSettings.a || path == \SOCDSettings.b {value.follows=nil};model.config.layers[model.layer].socdSettings=value})}
    private func axisBinding(_ path:WritableKeyPath<Config,Int>,_ gamePath:KeyPath<GameSettings,Int>)->Binding<Int> {
        Binding(get:{game.enabled ? game[keyPath:gamePath]+1 : model.config[keyPath:path]},set:{if !game.enabled {model.config[keyPath:path]=$0}})
    }
    var body:some View {VStack(spacing:16){GlassCard{VStack(alignment:.leading,spacing:14){SectionTitle(title:"Snap Tap / SOCD",subtitle:"İki tuşa basıldığında son basılana öncelik verilmesi hedeflenir");Picker("Katman",selection:$model.layer){ForEach(0..<4){Text("Layer \($0+1)").tag($0)}}.pickerStyle(.segmented);Toggle("SOCD etkin",isOn:socdBinding(\.enabled));HStack{keyPicker("Birinci tuş",socdBinding(\.a));keyPicker("İkinci tuş",socdBinding(\.b))};if socd.enabled && socd.a==socd.b {Text("İki farklı tuş seç.").font(.caption).foregroundStyle(.orange)}}};GlassCard{VStack(alignment:.leading,spacing:14){SectionTitle(title:"Joystick eksenleri",subtitle:game.enabled ? "Seçili katmanın oyun yönleri kullanılıyor" : "Oyun modu kapalı · genel taslak");Picker("Katman",selection:$model.layer){ForEach(0..<4){Text("Layer \($0+1)").tag($0)}}.pickerStyle(.segmented);HStack{axis("X−",axisBinding(\.joyXM,\.left));axis("X+",axisBinding(\.joyXP,\.right));axis("Y−",axisBinding(\.joyYM,\.back));axis("Y+",axisBinding(\.joyYP,\.forward))}.disabled(game.enabled);if game.enabled {Text("Yönleri Oyun profili bölümünden değiştir. Genel eksen taslağı oyun modu kapanınca korunur.").font(.caption).foregroundStyle(.secondary)}}};GlassCard{VStack(alignment:.leading,spacing:10){SectionTitle(title:"Makro taslağı",subtitle:"Metin taslaktır; makro kodu veya HID raporu oluşturmaz");TextEditor(text:$model.config.macro).font(.body).frame(height:100).scrollContentBackground(.hidden).padding(8).background(.black.opacity(0.2),in:RoundedRectangle(cornerRadius:8))}};GlassCard{VStack(alignment:.leading,spacing:10){SectionTitle(title:"Demo taslağı",subtitle:"Yalnızca bu uygulamanın yerel ayarları");Button("Taslağı JSON olarak dışa aktar…"){exportConfig()}}}} }
    func exportConfig() { let panel=NSSavePanel(); panel.nameFieldStringValue="wireless-he-demo.json"; panel.begin { result in if result == .OK, let url=panel.url { do { let encoder=JSONEncoder(); encoder.outputFormatting=[.prettyPrinted,.sortedKeys]; try encoder.encode(model.config).write(to:url,options:.atomic) } catch { let alert=NSAlert(error:error); alert.runModal() } } } }
    func keyPicker(_ title:String,_ v:Binding<Int>)->some View {Picker(title,selection:v){ForEach(0..<12){Text("KEY \($0)").tag($0)}}}
    func axis(_ title:String,_ v:Binding<Int>)->some View {Picker(title,selection:v){Text("Yok").tag(0);ForEach(0..<12){Text("KEY \($0)").tag($0+1)}}}
}
struct StudioBackup:Codable {
    var version=1
    var config:Config
    var profiles:[SavedLayerProfile]
    var game:[String:Double]
    var preferences:[String:String]
    var customColors:[String]? = nil
}
struct StudioSettingsView:View {
    @AppStorage("ble-slot-name-1") private var bleSlot1="Cihaz 1"
    @AppStorage("ble-slot-name-2") private var bleSlot2="Cihaz 2"
    @AppStorage("ble-slot-name-3") private var bleSlot3="Cihaz 3"
    @AppStorage("studio-appearance") private var appearance="system"
    @AppStorage("studio-start-page") private var startPage="Canlı test"
    @AppStorage("studio-language") private var language="tr"
    var body:some View {
        Form {
            Section("Görünüm") {
                Picker("Dil / Language",selection:$language){Text("Türkçe").tag("tr");Text("English").tag("en")}
                Picker("Tema",selection:$appearance){Text("Sistemle aynı").tag("system");Text("Açık").tag("light");Text("Koyu").tag("dark")}
                Picker("Başlangıç sayfası",selection:$startPage){ForEach(["Canlı test","Katmanlar","Oyun profili","Hall ayarları","Aydınlatma","Güç ve pil","Gelişmiş"],id:\.self){Text(LocalizedStringKey($0)).tag($0)}}
            }
            Section("Bluetooth") {
                TextField("Sol · cihaz adı",text:$bleSlot1)
                TextField("Orta · cihaz adı",text:$bleSlot2)
                TextField("Sağ · cihaz adı",text:$bleSlot3)
                Text("Bu adlar bu Mac’te belirlediğin etiketlerdir; cihazlardan otomatik okunmaz.").font(.caption).foregroundStyle(.secondary)
                Text("BLE modunda Layer + KEY 0 bağlantı türünü değiştirir. Layer + alt sıradaki tuşlar üç cihazı seçer.").font(.caption)
                Text("BLE üzerinden ayar yazımı uyumlu C6 yazılımı gerektirir; canlı sensör grafiği için S3 USB bağlantısı gerekir.").font(.caption).foregroundStyle(.secondary)
                Button("Bluetooth ayarlarını aç") {if let url=URL(string:"x-apple.systempreferences:com.apple.BluetoothSettings") {NSWorkspace.shared.open(url)}}
            }
            Section("Test ortamı") {
                LabeledContent("Bağlantı",value:"USB sensör veri akışı")
                LabeledContent("Yerel sunucu",value:"127.0.0.1:18768")
                Text("Bu sürüm yerel test ortamına ve ESP-IDF Python kurulumuna bağlıdır.").font(.caption).foregroundStyle(.secondary)
            }
        }.formStyle(.grouped).frame(minWidth:480,minHeight:440).environment(\.locale,Locale(identifier:language)).preferredColorScheme(appearance == "dark" ? .dark : appearance == "light" ? .light : nil)
    }
}
final class AppDelegate:NSObject,NSApplicationDelegate,NSWindowDelegate,NSMenuDelegate {
    var window:NSWindow!;var model:StudioModel!;var settingsWindow:NSWindow?;var activePanel:NSSavePanel?
    private var statusItem:NSStatusItem?
    private var statusLine:NSMenuItem?
    let gameKeys=["game-w","game-a","game-s","game-d","game-deadzone","game-run"]
    func applicationDidFinishLaunching(_ notification:Notification){
        model=StudioModel();window=NSWindow(contentRect:NSRect(x:0,y:0,width:1300,height:900),styleMask:[.titled,.closable,.miniaturizable,.fullSizeContentView],backing:.buffered,defer:false)
        setupStatusMenu()
        window.isReleasedWhenClosed=false;window.title="Wireless HE Studio";window.titlebarAppearsTransparent=true;window.contentView=NSHostingView(rootView:MainView(model:model));window.center();window.delegate=self;window.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
    }
    private func setupStatusMenu() {
        let status=NSStatusBar.system.statusItem(withLength:NSStatusItem.variableLength)
        if let button=status.button {
            if let url=Bundle.main.url(forResource:"studio-grid",withExtension:"svg"),let icon=NSImage(contentsOf:url) {
                icon.size=NSSize(width:21,height:21)
                icon.isTemplate=false
                icon.accessibilityDescription="Wireless HE Studio"
                button.image=icon
            }
            button.toolTip="Wireless HE Studio"
        }
        let menu=NSMenu();menu.delegate=self;menu.autoenablesItems=false
        let title=NSMenuItem(title:"Wireless HE Studio",action:nil,keyEquivalent:"");title.isEnabled=false;menu.addItem(title)
        let line=NSMenuItem(title:"Bağlantı kontrol ediliyor…",action:nil,keyEquivalent:"");line.isEnabled=false;menu.addItem(line);statusLine=line
        menu.addItem(.separator())
        let open=NSMenuItem(title:"Uygulamayı Aç",action:#selector(showMainWindow(_:)),keyEquivalent:"");open.target=self;menu.addItem(open)
        let quit=NSMenuItem(title:"Wireless HE Studio’dan Çık",action:#selector(quitFromStatus(_:)),keyEquivalent:"");quit.target=self;menu.addItem(quit)
        status.menu=menu;statusItem=status
    }
    func menuWillOpen(_ menu:NSMenu) {
        guard menu === statusItem?.menu else{return}
        if model.error != nil || (model.task != nil && model.task?.isRunning == false) {statusLine?.title="Yerel sunucu çalışmıyor"}
        else {statusLine?.title = model.values == nil ? "USB test cihazı: bağlı değil" : "USB test cihazı: bağlı"}
    }
    @objc private func showMainWindow(_ sender:Any?) {
        if window.isMiniaturized {window.deminiaturize(nil)}
        window.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
    }
    @objc private func quitFromStatus(_ sender:Any?) {NSApp.terminate(nil)}
    func windowShouldClose(_ sender:NSWindow)->Bool {
        if sender === window {sender.orderOut(nil);return false}
        return true
    }
    func applicationShouldHandleReopen(_ sender:NSApplication,hasVisibleWindows flag:Bool)->Bool {
        if !flag {showMainWindow(nil)}
        return true
    }
    @objc func showSettings(_ sender:Any?){
        if settingsWindow == nil {let w=NSWindow(contentRect:NSRect(x:0,y:0,width:520,height:460),styleMask:[.titled,.closable,.miniaturizable],backing:.buffered,defer:false);w.title="Ayarlar";w.isReleasedWhenClosed=false;w.contentView=NSHostingView(rootView:StudioSettingsView());w.center();settingsWindow=w}
        settingsWindow?.makeKeyAndOrderFront(nil);NSApp.activate(ignoringOtherApps:true)
    }
    @objc func showAbout(_ sender:Any?){NSApp.orderFrontStandardAboutPanel(nil)}
    @objc func showHelp(_ sender:Any?){let a=NSAlert();a.messageText="Wireless HE Studio";a.informativeText="Canlı test USB test platformunu izler. Katmanlar, oyun ve aydınlatma ayarları bu Mac’te taslaktır; cihaza gönderilmez. Profilleri ve ayarları Dosya menüsünden yedekleyebilirsin.";a.addButton(withTitle:"Tamam");a.runModal()}
    @objc func openProject(_ sender:Any?){if let url=URL(string:"https://github.com/mcskny/wireless-he-macropad"){NSWorkspace.shared.open(url)}}
    @objc func navigate(_ sender:NSMenuItem){if let page=sender.representedObject as? String {model.page=page;window.makeKeyAndOrderFront(nil)}}
    @objc func toggleModelView(_ sender:Any?){let d=UserDefaults.standard;let current=(d.object(forKey:"studio-view-3d") as? Bool) ?? true;d.set(!current,forKey:"studio-view-3d");model.page="Canlı test"}
    func makeBackup() throws -> StudioBackup {
        let defaults=UserDefaults.standard
        let profiles: [SavedLayerProfile]
        if let data=defaults.data(forKey:"native-layer-profiles-v1") {profiles=try JSONDecoder().decode([SavedLayerProfile].self,from:data)} else {profiles=[]}
        var game:[String:Double]=[:]
        for key in gameKeys {if let value=defaults.object(forKey:key) as? NSNumber {game[key]=value.doubleValue}}
        var preferences:[String:String]=[:]
        for key in ["studio-appearance","studio-start-page"] {if let value=defaults.string(forKey:key){preferences[key]=value}}
        let colorsText=defaults.string(forKey:"custom-light-colors-v1") ?? "[]"
        let colors=try JSONDecoder().decode([String].self,from:Data(colorsText.utf8))
        return StudioBackup(config:model.config,profiles:profiles,game:game,preferences:preferences,customColors:colors)
    }
    @objc func exportBackup(_ sender:Any?){
        let panel=NSSavePanel();panel.nameFieldStringValue="wireless-he-yedek.json";activePanel=panel
        panel.beginSheetModal(for:window) { result in defer {self.activePanel=nil};guard result == .OK,let url=panel.url else{return};do {let encoder=JSONEncoder();encoder.outputFormatting=[.prettyPrinted,.sortedKeys];try encoder.encode(self.makeBackup()).write(to:url,options:.atomic)} catch {NSAlert(error:error).runModal()} }
    }
    func valid(_ b:StudioBackup)->Bool {
        func hallOK(_ h:HallConfig)->Bool {
            [h.point,h.hysteresis,h.press,h.release,h.min,h.max].allSatisfy{(0...100).contains($0)} && ["Geleneksel","Rapid Trigger"].contains(h.mode)
        }
        func layerOK(_ l:LayerConfig)->Bool {
            let light=l.light
            guard (0...100).contains(light.brightness), (1...100).contains(light.speed), light.color.count == 7, light.color.first == "#", Int(light.color.dropFirst(),radix:16) != nil else{return false}
            if let speeds=light.effectSpeeds, !speeds.values.allSatisfy({(1...100).contains($0)}) {return false}
            if let b=l.keyBehaviors, !b.allSatisfy({["normal","twoStage"].contains($0.mode) && (1...100).contains($0.secondPoint)}) {return false}
            guard l.keys.count==12 && (l.keyBehaviors == nil || l.keyBehaviors!.count==12) else{return false}
            if let socd=l.socdSettings, !(0..<12).contains(socd.a) || !(0..<12).contains(socd.b) {return false}
            if let socd=l.socdSettings, let follows=socd.follows, !["ad","ws"].contains(follows) {return false}
            guard let g=l.game else{return true}
            if let kind=g.kind, !["driving","flight"].contains(kind) {return false}
            if let keys=g.managedRunKeys, (keys.count != 4 || Set(keys).count != 4 || !keys.allSatisfy({(0..<12).contains($0)})) {return false}
            return [g.forward,g.left,g.back,g.right].allSatisfy{(0..<12).contains($0)} && (0...50).contains(g.deadzone) && (1...100).contains(g.runPoint)
        }
        if let map=b.config.testChannelMap, (map.count != 12 || Set(map) != Set(0..<12)) {return false}
        if let curves=b.config.responseCurves, (curves.count != 12 || !curves.allSatisfy({$0.isFinite && (0.4...2.5).contains($0)})) {return false}
        guard b.version==1,b.config.layers.count==4,b.config.layers.allSatisfy(layerOK),b.config.hall.count==12,b.config.hall.allSatisfy(hallOK),b.config.calibration.count==12,b.config.calibration.allSatisfy({($0.free == nil || (0...4095).contains($0.free!)) && ($0.pressed == nil || (0...4095).contains($0.pressed!))}), (0...100).contains(b.config.brightness), (1...240).contains(b.config.idle),b.profiles.count<=1000,b.profiles.allSatisfy({layerOK($0.layer) && ($0.hall == nil || ($0.hall!.count == 12 && $0.hall!.allSatisfy(hallOK)))}), (0..<12).contains(b.config.socdA),(0..<12).contains(b.config.socdB), [b.config.joyXM,b.config.joyXP,b.config.joyYM,b.config.joyYP].allSatisfy({(0...12).contains($0)}) else{return false}
        for key in ["game-w","game-a","game-s","game-d"] {if let n=b.game[key], n.rounded() != n || n<0 || n>11{return false}}
        for key in ["game-deadzone","game-run"] {if let n=b.game[key], n<0 || n>100{return false}}
        if let theme=b.preferences["studio-appearance"], !["system","dark","light"].contains(theme){return false}
        if let page=b.preferences["studio-start-page"], !["Canlı test","Katmanlar","Oyun profili","Hall ayarları","Aydınlatma","Güç ve pil","Gelişmiş"].contains(page){return false}
        if let colors=b.customColors, (colors.count>256 || colors.contains(where:{ $0.count != 7 || $0.first != "#" || Int($0.dropFirst(),radix:16) == nil })) {return false}
        return true
    }
    @objc func importBackup(_ sender:Any?){
        let panel=NSOpenPanel();panel.canChooseDirectories=false;panel.allowsMultipleSelection=false;panel.message="Wireless HE Studio yedek dosyasını seç.";activePanel=panel
        panel.beginSheetModal(for:window) { result in defer {self.activePanel=nil};guard result == .OK,let url=panel.url else{return};do {
            let data=try Data(contentsOf:url);guard data.count<5_000_000 else {throw NSError(domain:"StudioBackup",code:1,userInfo:[NSLocalizedDescriptionKey:"Yedek dosyası çok büyük."])}
            let backup=try JSONDecoder().decode(StudioBackup.self,from:data);guard self.valid(backup) else {throw NSError(domain:"StudioBackup",code:2,userInfo:[NSLocalizedDescriptionKey:"Yedek geçersiz veya desteklenmiyor."])}
            let alert=NSAlert();alert.messageText="Yedek geri yüklensin mi?";alert.informativeText="Mevcut katmanlar, Hall ayarları, profiller ve oyun ayarları yedekteki değerlerle değiştirilecek.";alert.addButton(withTitle:"Geri yükle");alert.addButton(withTitle:"Vazgeç");guard alert.runModal() == .alertFirstButtonReturn else{return}
            let profiles=try JSONEncoder().encode(backup.profiles);let defaults=UserDefaults.standard
            self.model.config=backup.config;defaults.set(profiles,forKey:"native-layer-profiles-v1")
            for key in self.gameKeys {if let value=backup.game[key] {defaults.set(value,forKey:key)} else {defaults.removeObject(forKey:key)}}
            self.model.migrateLegacyGame()
            for key in ["studio-appearance","studio-start-page"] {if let value=backup.preferences[key]{defaults.set(value,forKey:key)} else {defaults.removeObject(forKey:key)}}
            if let colors=backup.customColors {let data=try JSONEncoder().encode(colors);if let value=String(data:data,encoding:.utf8){defaults.set(value,forKey:"custom-light-colors-v1")}}
            NotificationCenter.default.post(name:Notification.Name("StudioProfilesRestored"),object:nil)
        } catch {NSAlert(error:error).runModal()} }
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender:NSApplication)->Bool {false}
    func applicationWillTerminate(_ notification:Notification){model?.stop()}
}
@main struct StudioApplication {
    static func main() {
        let app=NSApplication.shared
        let delegate=AppDelegate();app.delegate=delegate;app.setActivationPolicy(.regular)
        let menu=NSMenu()
        func addMenu(_ title:String)->NSMenu {let item=NSMenuItem();let sub=NSMenu(title:studioDisplay(title));item.submenu=sub;menu.addItem(item);return sub}
        func item(_ m:NSMenu,_ title:String,_ action:Selector,_ key:String="",target:AnyObject?=nil){let i=NSMenuItem(title:studioDisplay(title),action:action,keyEquivalent:key);i.target=target;m.addItem(i)}
        let appMenu=addMenu("Wireless HE Studio")
        item(appMenu,"Wireless HE Studio Hakkında",#selector(AppDelegate.showAbout(_:)),target:delegate);appMenu.addItem(.separator())
        item(appMenu,"Ayarlar…",#selector(AppDelegate.showSettings(_:)),",",target:delegate);appMenu.addItem(.separator())
        item(appMenu,"Wireless HE Studio’yu Gizle",#selector(NSApplication.hide(_:)),"h");item(appMenu,"Diğerlerini Gizle",#selector(NSApplication.hideOtherApplications(_:)));item(appMenu,"Tümünü Göster",#selector(NSApplication.unhideAllApplications(_:)));appMenu.addItem(.separator());item(appMenu,"Wireless HE Studio’dan Çık",#selector(NSApplication.terminate(_:)),"q")
        let file=addMenu("Dosya");item(file,"Yedeği Dışa Aktar…",#selector(AppDelegate.exportBackup(_:)),"s",target:delegate);item(file,"Yedeği Geri Yükle…",#selector(AppDelegate.importBackup(_:)),"o",target:delegate);file.addItem(.separator());item(file,"Pencereyi Kapat",#selector(NSWindow.performClose(_:)),"w")
        let edit=addMenu("Düzenle");item(edit,"Geri Al",Selector(("undo:")),"z");let redo=NSMenuItem(title:studioDisplay("Yinele"),action:Selector(("redo:")),keyEquivalent:"Z");redo.keyEquivalentModifierMask=[.command,.shift];edit.addItem(redo);edit.addItem(.separator());item(edit,"Kes",#selector(NSText.cut(_:)),"x");item(edit,"Kopyala",#selector(NSText.copy(_:)),"c");item(edit,"Yapıştır",#selector(NSText.paste(_:)),"v");item(edit,"Tümünü Seç",#selector(NSText.selectAll(_:)),"a")
        let view=addMenu("Görünüm")
        for (index,page) in ["Canlı test","Katmanlar","Oyun profili","Hall ayarları","Aydınlatma","Güç ve pil","Gelişmiş"].enumerated(){let i=NSMenuItem(title:studioDisplay(page),action:#selector(AppDelegate.navigate(_:)),keyEquivalent:String(index+1));i.target=delegate;i.representedObject=page;view.addItem(i)}
        view.addItem(.separator());item(view,"2B / 3B Değiştir",#selector(AppDelegate.toggleModelView(_:)),"d",target:delegate)
        let windowMenu=addMenu("Pencere");item(windowMenu,"Simge Durumuna Küçült",#selector(NSWindow.performMiniaturize(_:)),"m");app.windowsMenu=windowMenu
        let help=addMenu("Yardım");item(help,"Kullanım Bilgisi",#selector(AppDelegate.showHelp(_:)),target:delegate);item(help,"Proje Sayfası",#selector(AppDelegate.openProject(_:)),target:delegate);app.helpMenu=help
        app.mainMenu=menu;app.run()
    }
}
