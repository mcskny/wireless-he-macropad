import Foundation
import CoreBluetooth

// Control connection only. HID input remains under macOS control.
final class BLEStudio: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let service=CBUUID(string:"C49B0001-6A38-46AF-A8F1-2996DF5A00A1")
    private let command=CBUUID(string:"C49B0002-6A38-46AF-A8F1-2996DF5A00A1")
    private let slotsUUID=CBUUID(string:"C49B0003-6A38-46AF-A8F1-2996DF5A00A1")
    private let hid=CBUUID(string:"1812")
    private var manager:CBCentralManager!
    private var peripheral:CBPeripheral?
    private var characteristic:CBCharacteristic?
    private var slotsCharacteristic:CBCharacteristic?
    var slotStatus:((Int,Int,Int)->Void)?
    private var pending:[Data]=[]
    private var completion:((Bool)->Void)?
    private var timer:Timer?
    private var sequence:UInt16=0
    private var batchID:UInt64=0
    private var enabled=false
    func setEnabled(_ value:Bool){enabled=value;if value {find()} else if let p=peripheral {manager.cancelPeripheralConnection(p);reset()} }
    var changed:((Bool)->Void)?
    var ready:Bool {peripheral?.state == .connected && characteristic != nil}
    override init(){super.init();manager=CBCentralManager(delegate:self,queue:.main,options:[CBCentralManagerOptionShowPowerAlertKey:false]);timer=Timer.scheduledTimer(withTimeInterval:4,repeats:true){[weak self] _ in self?.find()}}
    func centralManagerDidUpdateState(_ central:CBCentralManager){if central.state == .poweredOn {find()} else {reset()}}
    private func find(){
        if ready,let p=peripheral,let c=slotsCharacteristic {p.readValue(for:c)}
        guard enabled,manager.state == .poweredOn,peripheral == nil else{return}
        if let p=manager.retrieveConnectedPeripherals(withServices:[service,hid]).first(where:{$0.name == "Wireless HE"}) {attach(p);return}
        // Scanning is intentionally bounded; no keyboard events are observed.
        manager.scanForPeripherals(withServices:nil,options:nil)
        DispatchQueue.main.asyncAfter(deadline:.now()+3){[weak self] in self?.manager.stopScan()}
    }
    private func attach(_ p:CBPeripheral){guard peripheral == nil else{return};manager.stopScan();peripheral=p;p.delegate=self;manager.connect(p)}
    func centralManager(_ central:CBCentralManager,didDiscover peripheral:CBPeripheral,advertisementData:[String:Any],rssi RSSI:NSNumber){if peripheral.name == "Wireless HE" {attach(peripheral)}}
    func centralManager(_ central:CBCentralManager,didConnect p:CBPeripheral){p.discoverServices([service])}
    func centralManager(_ central:CBCentralManager,didFailToConnect p:CBPeripheral,error:Error?){reset()}
    func centralManager(_ central:CBCentralManager,didDisconnectPeripheral p:CBPeripheral,error:Error?){reset()}
    private func reset(){let wasReady=ready;characteristic=nil;slotsCharacteristic=nil;peripheral=nil;pending.removeAll();completion?(false);completion=nil;if wasReady {changed?(false)}}
    func peripheral(_ p:CBPeripheral,didDiscoverServices error:Error?){guard error == nil,let svc=p.services?.first(where:{$0.uuid==service}) else {manager.cancelPeripheralConnection(p);return};p.discoverCharacteristics([command,slotsUUID],for:svc)}
    func peripheral(_ p:CBPeripheral,didDiscoverCharacteristicsFor service:CBService,error:Error?){guard error == nil,let c=service.characteristics?.first(where:{$0.uuid==command}) else {manager.cancelPeripheralConnection(p);return};characteristic=c;slotsCharacteristic=service.characteristics?.first(where:{$0.uuid==slotsUUID});if let slots=slotsCharacteristic {p.readValue(for:slots)};changed?(true)}
    func peripheral(_ p:CBPeripheral,didUpdateValueFor characteristic:CBCharacteristic,error:Error?){guard error==nil,characteristic.uuid==slotsUUID,let d=characteristic.value,d.count==4,d[0]==1 else{return};slotStatus?(Int(d[1]),Int(d[2]),Int(d[3]))}
    func peripheral(_ p:CBPeripheral,didWriteValueFor characteristic:CBCharacteristic,error:Error?){guard completion != nil else{return};if error != nil {pending.removeAll();let cb=completion;completion=nil;cb?(false);return};sendNext()}
    private func sendNext(){guard let p=peripheral,let c=characteristic,ready else {let cb=completion;completion=nil;pending.removeAll();cb?(false);return};guard !pending.isEmpty else {let cb=completion;completion=nil;cb?(true);return};p.writeValue(pending.removeFirst(),for:c,type:.withResponse)}
    private func send(_ packets:[[UInt8]],done:@escaping(Bool)->Void){
        guard ready,completion == nil else{done(false);return}
        batchID &+= 1;let id=batchID;pending=packets.map{Data($0)};completion=done;sendNext()
        DispatchQueue.main.asyncAfter(deadline:.now()+15){[weak self] in
            guard let self=self,self.batchID==id,self.completion != nil else{return}
            if let p=self.peripheral {self.manager.cancelPeripheralConnection(p)};self.reset()
        }
    }
    private func nextID()->UInt16 {sequence &+= 1;if sequence==0{sequence=1};return sequence}
    private func crc(_ bytes:[UInt8])->UInt16 {var c:UInt16=0xffff;for b in bytes{c ^= UInt16(b)<<8;for _ in 0..<8{c=(c & 0x8000 != 0) ? (c &<< 1)^0x1021 : c &<< 1}};return c}
    private func commandFrame(_ op:UInt8,_ layer:Int,_ payload:[UInt8])->[UInt8] {let id=nextID();var b:[UInt8]=[87,1,op,UInt8(layer),UInt8(id & 255),UInt8(id>>8)]+payload;let c=crc(b);b += [UInt8(c & 255),UInt8(c>>8)];return b}
    func sendLights(_ profiles:[[Int]],done:@escaping(Bool)->Void){guard profiles.count==4,profiles.allSatisfy({$0.count==7 && $0.allSatisfy{(0...255).contains($0)}}) else{done(false);return};send(profiles.enumerated().map{commandFrame(2,$0.offset,$0.element.map(UInt8.init))},done:done)}
    func selectLayer(_ layer:Int,done:@escaping(Bool)->Void){guard (0..<4).contains(layer) else{done(false);return};send([commandFrame(1,layer,Array(repeating:0,count:7))],done:done)}
    func writeLayer(_ layer:Int,bytes:[UInt8],done:@escaping(Bool)->Void){guard (0..<4).contains(layer),bytes.count==192 else{done(false);return};let tx=nextID();let lo=UInt8(tx&255),hi=UInt8(tx>>8);var frames:[[UInt8]]=[[0x10,lo,hi,UInt8(layer)]];for i in 0..<24{frames.append([0x11,lo,hi,UInt8(i)]+Array(bytes[i*8..<i*8+8]))};let c=crc(bytes);frames.append([0x12,lo,hi,UInt8(c&255),UInt8(c>>8)]);send(frames,done:done)}
}
