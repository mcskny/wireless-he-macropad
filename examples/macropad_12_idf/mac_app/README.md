> **Studio 0.1.1:** Canlı test ve aydınlatmadaki 2B/3B modeller bağlantı durumunu gösterir. BLE klavye varlığı Mac'in HID aygıtları üzerinden algılanır; bu, sensör telemetrisi veya ayar yazma anlamına gelmez. BLE üzerinden canlı sensör, profil ve aydınlatma yapılandırması henüz uygulanmadı.

> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [bütünleşik rehbere](../../../Docs/Proje_Rehberi.md) bakın.

# Wireless HE Studio — macOS test uygulaması

> **Güncel not — 2 Kasım 2026:** Aşağıdaki eski bölümlerde bazı özellikler yalnızca taslak olarak anlatılabilir. Belgenin sonundaki Mac klavye çıktısı ve sensör kalibrasyonu bölümleri bu eski açıklamaları günceller. Projenin tüm bileşenleri ve doğrulama durumu için [birleşik PDF](../../../output/pdf/Wireless_HE_Proje_Rehberi.pdf) de kullanılabilir.

> **Durum: yerel test ve tasarım uygulaması.** Bu belge, mevcut macOS uygulamasının ne yaptığını, neleri henüz yapmadığını ve uygulama üzerinde çalışacak bir geliştirici veya yapay zekânın dikkat etmesi gerekenleri açıklar. Kod her zaman güncel davranış için asıl kaynaktır. Son gözden geçirme: **31 Ekim 2026**.

## Amaç ve kapsam

Wireless HE Studio, 12 Hall sensörlü bir macropad için yerel macOS arayüzüdür. Geliştirme sırasında klasik ESP32 DevKit, MCP3202 ADC ve CD74HC4067 çoklayıcıdan gelen 12 analog kanalı USB/seri bağlantı üzerinden görüntülemek için kullanılır. Uygulama ayrıca gerçek macropadın modelini, tuş hareketini, katman, oyun ve aydınlatma ayarlarını **yerel önizleme veya taslak** olarak gösterir.

Ürün için düşünülen C6 gönderici ve S3 alıcı bu test akışının parçası değildir. Uygulamada bunlara bağlanan veya ayar gönderen bir uygulama iletişimi yoktur. Kablosuz test şu an kapsam dışıdır. Donanım bağlı olmadığında canlı sensör davranışı, kablo bağlantıları ve uçtan uca veri akışı doğrulanmış kabul edilmemelidir.

**En önemli kural:** Arayüzde bir seçeneğin bulunması, onun donanımda, firmware'de veya oyunda uygulandığı anlamına gelmez. Aşağıdaki bölümlerde canlı veri ve taslak özellikler ayrı belirtilmiştir.

## Projenin fiziksel görünümü ve kanal sırası

Klavye, üç sütun ve dört satırda toplam **12 analog tuş**, sol üstte ayrı bir **mekanik layer tuşu**, dört layer LED'i ve sağ üstte bir **encoder** içerir. 12 analog kanal şemada sütun sütun numaralandırılır:

```text
KEY 0   KEY 4   KEY 8
KEY 1   KEY 5   KEY 9
KEY 2   KEY 6   KEY 10
KEY 3   KEY 7   KEY 11
```

Uygulamada `i / 4` sütunu, `i % 4` satırı verir. 2B görünümün satır bazlı indeksinden kanal numarasına dönüşüm `(idx % 3) * 4 + idx / 3` şeklindedir. İlk satır ikinci sütun **KEY 4**'tür. Layer tuşu, layer LED'leri ve encoder fiziksel cihazı temsil etmek için çizilir; mevcut USB test paketinde bunların gerçek durumları bulunmaz. Seçili katman göstergesi uygulamadaki taslak katmandır, cihazdan okunmuş durum değildir.

## Başlatma, derleme ve bağımlılıklar

Uygulama bu Mac için oluşturulmuş SwiftUI/AppKit ve SceneKit uygulamasıdır; web sayfasının uygulamaya sarılmış hali değildir. Swift Charts grafik için kullanılır. macOS 26 ve üzerinde `GlassCard` Liquid Glass API'sini kullanır; eski sürüm için materyal tabanlı bir geri dönüş yolu vardır. Bunun dışında desteklenen en düşük macOS sürümü ve diğer Mac'lerde çalışma durumu kapsamlı olarak doğrulanmamıştır.

`examples/macropad_12_idf` dizininden:

```bash
./mac_app/build_app.sh
open 'mac_app/Wireless HE Studio.app'
```

Derleme betiği `Native.swift` ve `Model3D.swift` kaynaklarını `swiftc` ile derler; `macropad.usdz` dosyasını paketler, `studio-grid.svg` kaynağından macOS için `AppIcon.icns` üretir ve `Info.plist` oluşturur. `mac_app/Studio.swift` bu derleme komutunda **yer almaz**; uygulamayı değiştirirken asıl giriş noktası olarak yanlışlıkla kullanılmamalıdır.

Betik şu Python yorumlayıcısının varlığını şart koşar:

```text
~/.espressif/python_env/idf6.1_py3.13_env/bin/python
```

Bu ortamda `pyserial` gerekir. Betik, derleme anındaki test proje dizinini ve Python yolunu uygulamanın `Info.plist` dosyasına yazar (`TestProjectPath`, `TestPythonPath`). Uygulama bu yollardan `server.py` çalıştırır. Bu nedenle kaynak dizin veya Python ortamı taşınırsa betik uyarlanıp uygulama yeniden derlenmelidir. `server.py` ayrıca başlangıçta `main/index.html` ve `main/studio.html` dosyalarını okur. Uygulama bağımsız bir dağıtım paketi veya imzalanmış/noter onaylı sürüm değildir; bu projede başka Mac'lere dağıtım şu an öncelik değildir.

Uygulama içinde `Ayarlar` penceresi `⌘,` ile açılır. Tema sistem/açık/koyu, başlangıç sayfası seçilebilir. Ana pencere 1300 × 900 boyutunda sabittir. Üst menüde Hakkında, Dosya, Düzenle, Görünüm, Pencere ve Yardım bulunur. `⌘1`–`⌘7` sayfaları, `⌘D` canlı testte 2B/3B görünümü değiştirir. Ana pencere kapatılınca uygulama menü çubuğundaki 3×3 simgeyle arka planda çalışmaya devam eder. Menüde USB test bağlantı durumu, uygulamayı açma ve tamamen çıkma seçenekleri vardır. Çıkışta kendi başlattığı sunucuya kesme sinyali gönderir.

## Çalışma mimarisi ve veri akışı

```text
ESP32 DevKit test firmware'i
    │ USB seri, 115200 baud; 12 ADC kanalı
    ▼
server.py + pyserial
    │ 127.0.0.1:18768 /state  (native uygulamanın sunucusu)
    ▼
StudioModel.poll() → SwiftUI grafik / 2B / SceneKit 3B
```

- `StudioModel`, uygulama başlarken `server.py --http-port 18768` işlemini açar. Standart çıktı ve hata `nullDevice`'a yönlendirilir; bu yüzden hata ayıklarken sunucuyu ayrı terminalde başlatmak daha fazla bilgi verir.
- Native istemci bağlantı varken ve pencere görünürken `/state` uç noktasını yaklaşık **20 ms** aralıklarla sorgular; bağlantı yokken veya pencere görünür değilken aralık yaklaşık **250 ms** olur. Aynı anda birden fazla istek başlatmamak için `busy` kullanır. Bu, donanım örnekleme hızı veya uçtan uca gecikme garantisi değildir.
- `server.py` normalde ayrı başlatılırsa varsayılan web portu **8765**'tir. Yerel arayüz `/`, demo web arayüzü `/demo`, anlık durum `/state`, web için Server-Sent Events akışı `/events` üzerindedir. Native uygulama web sayfasını kullanmaz ve `/state` sorgular.
- Otomatik seri port seçimi macOS'ta `/dev/cu.usbserial*` ve `/dev/cu.usbmodem*` listesini sıralayıp ilkini kullanır. Sunucu ayrıca `--port` kabul eder; native uygulama şu anda bu parametreyi geçmez. Birden çok uygun port varsa yanlış cihaz seçilebilir.
- Aynı seri portu aynı anda iki sunucu veya seri monitör kullanamaz. `8765` web örneği ile `18768` native sunucusu ayrı portlardır ama aynı fiziksel seri port için yine çakışabilir.
- Geçerli sensör paketi yaklaşık iki saniye gelmezse sunucu bağlantıyı kapatıp yeniden arar ve bağlantısız durum yayımlar. Uygulama yalnızca geçerli, `connected: true` ve 12 elemanlı `values` dizisini kabul eder.

### Seri paket ve durum biçimi

Güncel test firmware'i satır başına şu biçimi üretir; her ADC değeri `0...4095`, sıra numarası `UInt32` aralığındadır:

```text
K,sıra,kanal0,kanal1,kanal2,kanal3,kanal4,kanal5,kanal6,kanal7,kanal8,kanal9,kanal10,kanal11
```

Eski sürümdeki `K,kanal0,...,kanal11` sıra numarasız biçim de desteklenir. Diğer seri satırlar sensör paketi olarak değerlendirilmez. Geçerli durum örneği:

```json
{"connected": true, "values": [1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800, 1800]}
```

`1800` yalnızca örnektir, beklenen zorunlu değer değildir. Bağlantı yokken durum `{"connected": false}` olur. Uygulama `values` dizisini yorumlar; bu protokol basılı dijital tuş, layer LED'i, encoder, pil veya gerçek NeoPixel durumunu içermez.

### Otomatik günlük

`server.py`, `K,` ile başlayan ilk satır geldiğinde — satırın geçerli olduğu doğrulanmadan önce de olabilir — `examples/macropad_12_idf/logs/` altında JSONL oturumu açar. Satırlar `start`, `sample`, `invalid_packet`, `gap`, `sequence_restart_or_duplicate` ve `end` olaylarını içerebilir. `end` kaydı oturum sonundaki nedeni, oturumun başlangıcından son geçerli pakete kadar hesaplanan süreyi, geçerli/geçersiz/sıralı/sırasız örnek sayılarını ve saptanan sıra boşluklarını içerir.

Sıra numarasız herhangi bir paket varsa tamamlık `unknown` olarak işaretlenir; tüm paketler sıralıysa sonuç sadece **tespit edilen sıra boşlukları** ile ilgilidir. Bu sayılar elektriksel doğruluk, sensör çalışması veya tüm verinin gerçekten ulaştığının kesin kanıtı değildir. Günlük dosyalarını listeleyen veya tekrar oynatan bir native ekran şu anda yoktur.

## Mevcut sayfalar: çalışan özellik ve sınırlar

### Canlı test

- Geçerli USB verisi geldiğinde 12 ham ADC değeri, seçili kanalın grafiği ve her kanal için oturumdaki minimum/maksimum değer gösterilir. Grafik son en fazla 120 sorgu örneğini tutar; zaman ekseni gerçek ölçülmüş milisaniye olarak sunulmaz.
- Uygulama içinde seçili tuş veya tüm tuşların serbest değeri ve seçili tuşun tam basılı değeri kaydedilebilir. İki değer arasındaki mutlak fark en az 10 ADC birimi ise yaklaşık basılma yüzdesi hesaplanır; sonuç `0...100` ile sınırlandırılır. Kayıtlı değerler yalnızca Mac'tedir.
- Yüzde için kullanılan formül `clamp((serbest - anlık) / (serbest - basılı) * 100, 0, 100)` biçimindedir; ters yönde değişen sensörler de uygun kalibrasyonla hesaplanabilir. Yüzde gerçek mm veya mT ölçümü değildir.
- Simülasyon, seçili tuşun görsel derinliğini değiştirir; ham ADC'yi, seri akışı veya günlükleri değiştirmez. Gerçek değer alanları ve grafik sahte veri olarak doldurulmaz.
- Ekranda `0` veya `4095` uyarı rengiyle görülebilir; bunlar tek başına elektriksel arıza teşhisi değildir.

### 2B ve 3B görünüm

- `KeyPad`, etkileşimli 2B modeldir. `MacropadModelView` ve `MacropadSceneView`, `assets/macropad.usdz` içindeki modelin SceneKit görünümüdür. Tuşa tıklamak kanal seçer ve tepkisel ışık önizlemesini tetikleyebilir.
- Blender kaynak dosyasından birinci kare dışa aktarılmıştır; kaynak modelin geçmişteki konumu `/Users/bain/Documents/Blender/blender documents/macropadv1.blend` olarak kullanılmıştır. Bu yolun başka ortamda mevcut olduğu varsayılmamalı. `export_model.py`, `MACROPAD_MODEL_EXPORT` ortam değişkenini kullanır; 12 kapağı `HE_KEY_00`–`HE_KEY_11` olarak adlandırır. Betik Blender `bpy` ortamı içindir. Kaynak dosyayı değiştirmeden ve tuş–kanal eşleşmesini doğrulayarak çalışılmalıdır.
- 3B sahne gerektiğinde çizilir; animasyon görünürken en fazla yaklaşık 24 kare/saniye hedeflenir ve pencere görünür değilken animasyon sayacı durur. Bağlantı ve görüntü yenilemeleri enerji tüketimini azaltmak için ayrı yönetilir.
- 3B modelde kapakların hareketi yaklaşık **3,2 mm** görsel animasyondur; kalibre edilmiş fiziksel tuş yolu ölçüsü değildir. Döndürme/yakınlaştırma kamera ile yapılır; model sabit kalır, kamera açı ve mesafe sınırları vardır. Büyük düz zemin kullanılır.
- Işık, kasanın üst yüzeyinin biraz altında tuş aralığına göre yerleştirilmiş basit renkli yüzeylerle temsil edilir. Işın izleme, gerçek LED geometrisi veya ölçülmüş ışık yayılımı değildir. Kasa üstünde istenmeyen yansıma/halka eklemek önceki tasarım kararlarıyla çelişebilir.
- 2B/3B görünümler birebir aynı render motoru değildir; görsel efektler ve zamanlamaları tam senkronizasyon garantisi vermez. Erişilebilirlik etiketleri kısmen vardır, tam VoiceOver ve klavye erişimi testi yapılmamıştır.

### Katmanlar ve profiller — yerel taslak

- Dört katman düzenlenebilir. On üç hazır profilin 156 tuş görevi USB klavye/medya koduna çözümlenir. Belgelenmiş varsayılanı bilinmeyen görevler için Control+Option/Alt+Shift+F1–F12 önerileri kullanılır; bunlar varsayılan değildir ve hedef uygulamada ilgili göreve tek tek atanmalıdır. Önizleme ve seçili tuş bu koşulu ve kombinasyonu gösterir; kullanıcı kendi tuşunu atayabilir. Encoder görevleri henüz cihaza aktarılmaz ve oyunlara özel analog çıkış bu hazır profil eşlemesinin parçası değildir. Kısayol çözümlenmesi uygulama/oyun içinde çalışma veya kullanıcı ayarlarıyla uyum testi değildir.
- Bir tuş `normal` veya `twoStage` seçilebilir. İki kademede ilk görev Hall aktivasyon eşiğinde, ikinci görev daha derin eşikte **önizleme mantığında** birinciye eklenir. Görevler açıklama olarak saklanır; gerçek tuş, makro veya HID çıktısı üretilmez. İkinci eşik ilk eşikten büyük değilse uyarı gösterilir. İlk Hall eşiği katmanlar arasında ortaktır.
- Hazır profil seçilip önizlenebilir; uygulanınca seçili katmanın adı, tuşları, tuş davranışları ve encoder görevleri değişir; katmanın ışık ayarı korunur. Son uygulama için görünüm içi geri alma vardır; genel sınırsız undo geçmişi değildir.
- **Kendi profillerim**, seçili katmanın kopyasını isimle kaydeder; profiller seçili katmana uygulanabilir, güncellenebilir veya silinebilir. Kayıtlı özel profil tuşlar, ikinci kademe, encoder, SOCD ve katmana ait ışığı içerir. Profili uygularken kayıtlı ışığın uygulanması seçilebilir. Katmana özel oyun ayarları da özel profilde saklanır. Yeni özel profiller Hall ayarlarının kopyasını da içerir; bunlar isteğe bağlı geri yüklenir ve tüm katmanları etkiler. Kalibrasyon profile dahil edilmez.
- Tuşlara veya encoder'a uygulama başlatma, kısayol gönderme, gerçek makro atama, işletim sistemi kontrolü veya donanıma yazma henüz uygulanmış değildir.

### Oyun profili — katmana özel önizleme

Her katmanda bağımsız etkinlik, W/A/S/D yönleri, ölü bölge ve koşma eşiği vardır. Minecraft, FPS, Euro Truck Simulator 2, Assetto Corsa, Microsoft Flight Simulator ve Farming Simulator hazır profilleri oyun modunu açar ve KEY 4/1/5/9 yönlerini kullanır; diğer hazır profiller mevcut oyun değerlerini koruyarak oyun modunu kapatır. Oyun ayarları özel katman profiline dahil edilir. Eski kayıtlarda katmana özel oyun ayarı yoksa eski genel ayarlar katmanlara taşınır ve oyun modu kapalı başlar; eski kayıtlı katman profilleri ayar yoksa varsayılan kapalı modu kullanır.

Oyun yönlerini katman görevlerine aktarma, koşma eşiğini yönlerin ikinci kademesine uygulama ve W/S veya A/D çiftini katmanın SOCD ayarına aktarma düğmeleri kullanıcı tarafından seçilir. Koşma düğmesi boş veya aynı koşma görevine sahip ikinci kademelere Koş · Shift atar; başka bir görev varsa onu ezmez. Koşma eşiği sonraki değişikliklerde atanan eşikleri günceller; bağlı bir tuşun ilk Hall eşiğine eşit veya altında değer kabul edilmez. Uygulamanın koşma ataması yön değişiminde hedef uygun olduğunda taşınır; farklı görevler korunur ve uyumsuzluk gösterilir. Yön, koşma ve SOCD aktarma işlemleri seçili katmanda geri alınabilir. İkinci eşik tüm seçili yönlerin ortak Hall ilk eşiğinden büyük değilse aktarma engellenir. SOCD katman başınadır ve özel profillere kaydedilir; eski ortak ayar ilk geçişte her katmana taşınır. Oyun ekranından seçilen W/S veya A/D çifti yönleri takip eder; gelişmiş ekranda el ile değiştirilen çift bağımsızdır. Oyun modu açık katmanda Gelişmiş bölümündeki joystick yönleri oyun yönlerinden gösterilir; kapatılınca eski genel joystick taslağı korunur. ETS2, Assetto Corsa ve Farming Simulator sürüş önizlemesinde sol/sağ tuşların kalibre edilmiş basılma farkı analog direksiyon olarak gösterilir; gaz ve fren ayrı değerlerdir. Flight Simulator önizlemesi basılma farklarıyla X roll ve Y pitch gösterir; motor gücü ayrı analog eksen değildir. Giriş ayarları oyunda ve gerçek giriş katmanında ayrıca eşleştirilmelidir. Oyun önizlemesi gerçek HID çıktısı üretmez; birleşik hareket büyüklüğünü gösterir ve koşma eşiğini yön tuşlarının her birinde ayrı karşılaştırır, fiziksel koşma komutu veya cihaz yapılandırması değildir. Canlı mod için dört yön kalibrasyonu gerekir.

### Hall ayarları — yerel taslak

Geleneksel/Rapid Trigger seçimi, aktivasyon, histerezis, basma/bırakma hassasiyetleri ve RT alt/üst sınırları saklanır. `Seçili tuş` yalnız o kanalı, `Tüm tuşlar` bir kontrol değiştiğinde ilgili alanı 12 kanalın hepsini günceller. Ayrıca seçili tuşun tüm Hall ayarları 12 tuşa kopyalanıp geri alınabilir; kalibrasyonlar kopyalanmaz. Gösterilen değerler seçili tuşundur; tüm tuşlarda daha önce farklı değerler olabilir. Eşik önizlemesi yalnızca değer karşılaştırmasıdır; gerçek Rapid Trigger durum makinesi uygulanmış veya donanımda test edilmiş değildir.

### Aydınlatma — yerel önizleme

Katmana bağlı renk, parlaklık ve hız ayarı ile `Kapalı`, `Sabit`, `Nefes`, `Dalga`, `Gökkuşağı`, `Kayan ışık`, `Tepkisel` ve `Basılan yerden dalga` seçenekleri vardır. Gökkuşağının renkleri zamanla yumuşak biçimde değişir. Kapalı modda renk, parlaklık ve hız kontrolleri; Sabit modda hız kontrolü gösterilmez. Tepkisel ve basılan yerden dalga efektlerinde isteğe bağlı derinlik duyarlılığı açılabilir; sırasıyla tuş parlaklığı veya yayılan dalganın şiddeti basılma derinliğine göre ayarlanır. Dalga o basışta ulaşılan en yüksek derinliği kullanır. Nefes, Dalga, Kayan ışık, Tepkisel ve Basılan yerden dalga için ayrı `Rengârenk` seçenekleri vardır; Nefes en sönük noktada, Dalga ise her tuşun kendi en sönük noktasında renk değiştirir. Tepkisel modda her basış farklı renk alır; basılan yerden dalga seçeneğinde her basış yeni renk oluşturur ve kesişen dalgaların renkleri karıştırılır. Kayan ışıkta `Karışık` ve her tur tek renk kullanan `Her tur tek renk` düzenleri bulunur. Tek renkli turda son LED söndükten sonra sonraki renk sıfır parlaklıktan başlar. Eski derinliğe duyarlı efekt adları, kayıtlı profillerle uyumluluk için uygulamada yorumlanır. Aydınlatma sayfasında derinlik için kalibre edilmiş canlı klavye verisi kullanılır; burada simülasyon kontrolü yoktur. 2B ve 3B önizlemeler arasında geçiş yapılabilir; animasyon fiziksel LED çıktısı değildir.

Normal tepkisel modlar model tuşuna tıklanarak denenebilir. 2B ışık yüzeyleri SwiftUI yerleşimini her karede yenilemeden yerel çizim katmanlarıyla güncellenir. Tepkisel ve basılan yerden dalga önizlemesi yalnız aktif efekt varken animasyon karesi ister; olaylar bitince yeniden çizim durur. Simülasyonda seçili tuş %10'a ulaşınca; canlı modda kalibre edilmiş tuş yüzdesi %10'u geçince bir olay oluşturulur ve tekrar tetikleme için %5 altına inilmesi beklenir. Bunlar uygulamanın görsel önizleme eşikleridir; seçili Hall aktivasyon eşiklerini, cihaz firmware'ini veya gerçek anahtar davranışını temsil etmez. Dalga önizlemeleri ayrı basışları sınırlı bir olay listesinde saklar, aynı anda devam eden dalgaların yoğunluklarını toplar ve görüntüde en fazla seçilen parlaklık değerine sınırlar. Basınç duyarlı önizleme canlı kalibre edilmiş veri gerektirir; bağlantı yoksa ışığın yanmaması beklenir.

### Güç ve pil / Gelişmiş — yerel taslak

Temel parlaklık 2B ve 3B önizlemede seçili katman parlaklığının üst sınırıdır. Dijital, Hibrit ve Agresif güç modu tercihleri, zaman ve parlaklık alanları; planlanan 5/23 bayt paket bilgileri ve pil uyarı eşikleri yalnızca arayüzde saklanan/gösterilen taslaklardır. Mevcut USB protokolü pil, şarj veya güç tüketimi okumaz. SOCD, joystick eksenleri ve metin makrosu da yalnızca taslaktır; HID raporu veya cihaz davranışı üretmez.

## Yerel kayıt ve yedekleme

Bundle kimliği: `local.wirelesshe.studio`. Ana uygulama verileri `UserDefaults` içinde tutulur:

| Anahtar | İçerik |
|---|---|
| `native-config-v1` | `Config` JSON verisi: 4 katman, 12 Hall ayarı, 12 kalibrasyon ve diğer demo alanları |
| `native-layer-profiles-v1` | `SavedLayerProfile` dizisi: kullanıcının kaydettiği özel katman profilleri |
| `game-w`, `game-a`, `game-s`, `game-d`, `game-deadzone`, `game-run` | Önceki sürümden geçiş ve eski yedekler için genel oyun ayarları; güncel ayarlar katman içinde |
| `studio-appearance`, `studio-start-page`, `studio-view-3d` | Tema, açılış sayfası ve 2B/3B tercihi |
| `custom-light-colors-v1` | Tüm katmanlarda ortak özel renk paleti (JSON dizisi metni) |

`Config`, `LayerConfig`, `KeyBehavior`, `HallConfig`, `Cal` ve `SavedLayerProfile` tipleri `Native.swift` içindedir. `LayerConfig.keyBehaviors` eskiden kaydedilmiş verilere uyum için isteğe bağlıdır. Kalıcı veri biçimi değiştirilirse eski kayıtlarla uyumluluk, varsayılan değerler ve `Codable` geçişi dikkatle ele alınmalıdır. Başlangıçta `Config` çözümlenemez veya temel dizi sayıları uyuşmazsa varsayılan yapı kullanılır; şema değişiklikleri kullanıcı verisini görünmez kılabilir.

Dosya menüsündeki **Yedeği Dışa Aktar / Yedeği Geri Yükle**, sürüm 1 `StudioBackup` JSON'u kullanır: `config`, `profiles`, `game`, `preferences`. Bunlar ana yapılandırma, özel profiller, oyun ayarları, tema, başlangıç sayfası ve özel renk paletini içerir. Eski yedeklerde renk alanı bulunmayabilir; böyle bir yedek mevcut paleti değiştirmez. `studio-view-3d`, simülasyonun geçici durumu, oturum günlükleri ve 3B kaynak dosyalar bu yedeğe dahil değildir. İçe aktarma temel yapı/sınır kontrolleri yapar ve mevcut veriler değiştirilmeden önce onay ister; geri yükleme var olan bu alanları değiştirir. `Gelişmiş` sayfasındaki eski **Taslağı JSON olarak dışa aktar** işlemi ise sadece `Config` yazar; tam yedekle aynı format değildir ve tam yedek içe aktarıcısına uygun olduğu varsayılmamalıdır.

Yedek kaydetme penceresi açılarak kontrol edilmiştir; bu belgenin hazırlandığı sırada tam **dışa aktar → yeniden başlat → içe aktar → veri karşılaştır** testi ve bozuk dosya senaryoları henüz uçtan uca doğrulanmış olarak raporlanmamıştır. Test öncesi kullanıcının mevcut profil ve ayarlarını koruyun.

## Dosya haritası

| Yol | Görev |
|---|---|
| `mac_app/Native.swift` | SwiftUI ekranları, uygulama modeli, `UserDefaults`, yerel sunucu istemcisi, menüler, ayarlar, yedekleme ve uygulama giriş noktası |
| `mac_app/Model3D.swift` | SceneKit modeli, malzemeler, kamera, tuş hit testi ve ışık/tuş önizlemeleri |
| `mac_app/assets/macropad.usdz` | Uygulamada kullanılan dışa aktarılmış model |
| `mac_app/assets/studio-grid.svg` | Dock, uygulama başlığı ve menü çubuğu için şeffaf 3×3 logo kaynağı |
| `mac_app/assets/app-icon.png` ve `app-icon-v2.png` | Önceki simge taslakları |
| `mac_app/export_model.py` | Blender içinden model dışa aktarma betiği |
| `mac_app/build_app.sh` | Yerel `.app` oluşturma, simge ve model paketleme |
| `server.py` | Seri okuma, JSONL kayıt ve localhost HTTP sunucusu |
| `main/*.c` | USB test platformunun ESP-IDF test firmware'i; macOS arayüzü değildir |
| `README.md` | Üst test örneğinin bağlantı ve çalıştırma bilgileri |

Buradaki yollar `examples/macropad_12_idf` dizinine göredir. Üretilen `.app` derleme çıktısıdır; kaynak düzenlemesi doğrudan `Native.swift`, `Model3D.swift` ve ilgili varlıklarda yapılmalıdır.

## Sonraki geliştirici için dikkat noktaları

1. Kullanıcının şu anki önceliği **yerel, kablolu test**. C6/S3, ESP-NOW, cihazlara yazma veya üretim özelliklerini istenmeden etkinleştirmeyin. Donanım yoksa cihaz testi yapılmış gibi yazmayın.
2. Görünüm iyileştirmeleri canlı ADC akışını, kalibrasyonları, mevcut profilleri, 2B/3B kanal sırasını ve kayıtları korumalı. Tuş görevleri veya ışık ayarlarını çalışıyormuş gibi sunmayın.
3. Seri/HTTP protokolü değiştirilirse firmware, `server.py` ve native istemciyi birlikte ele alın; eski sıra numarasız biçim ve kayıt uyumluluğu için karar verin. Grafik yenileme aralığı, veri kaybı ve gerçek örnekleme hızı farklı kavramlardır.
4. `UserDefaults`/JSON şemasında değişiklik öncesi eski veriyi okumayı ve yedek geri yüklemeyi test edin. Kayıtlı özel profillerin ve ortak Hall ayarlarının farklı kapsamları vardır.
5. Öncelikli doğrulama: yedekleme/geri yükleme uçtan uca, bozuk ve eski veriyle sınamalar; ardından donanım bağlandığında 12 kanal, kalibrasyon, seri port seçimi, kesinti ve JSONL kayıtları. Gerçek cihaz yokken simülasyon yalnızca arayüzü sınar.
6. Model/aydınlatma çalışmasında `HE_KEY_*` isimlerine, 3×4 fiziksel sıraya, sabit cihaz ve sınırlı kameraya dikkat edin. Işıklar mevcut tasarımda tuşların altında birleşen yüzeylerdir; düz görsel efektleri fiziksel ray tracing diye tanımlamayın.
7. Ekranlar ve yedekleme menüleri görsel olarak incelenmiştir; diğer tüm sayfalarda, açık/koyu temada, VoiceOver ile ve gerçek donanımla kapsamlı regresyon testi tamamlanmış sayılmamalıdır.

### Encoder görevlerinin gerçek cihaza aktarımı

Encoder seçimleri halen yalnızca uygulamada kaydedilir; test firmware’i ve S3 dongle bunları henüz uygulamaz. Dönüş menüsü uygulanabilirliği belirli HID davranışlarıyla sınırlandırılmıştır: ses azalt/artır için Consumer Volume Decrement/Increment, ekran parlaklığı için Consumer Display Brightness Decrement/Increment, parça değişimi için Consumer Scan Previous/Next Track ve dikey kaydırma için fare tekerleği raporu. Encoder basma menüsündeki medya ve ses görevleri Consumer HID komutlarına, harf ve rakamlar klavye HID raporlarına karşılık gelir. Her encoder adımı için doğru rapor ve bırakma durumu üretilmelidir; fare kaydırma işareti gerçek macOS cihazında test edilmelidir.

HID standardında bir komutun bulunması işletim sisteminin veya bağlı ekranın onu destekleyeceği anlamına gelmez. Özellikle ekran parlaklığında harici ekran desteği değişebilir. Rapor tanımlayıcıları, ESP-NOW iletişimi, bu seçimlerin kimlikli cihaz yapılandırmasına dönüştürülmesi ve S3 üzerinde işlenmesi henüz mevcut değildir. Layer basılıyken encoder kullanımı için encoder hareketinin layer basışını tüketmesi, bırakmada yanlışlıkla katman değiştirmemesi ve bağlantı kaybında durumların temizlenmesi üretim kodunda ayrıca uygulanıp test edilmelidir. Eski profillerdeki uygulamaya özel encoder metinleri korunur, ancak otomatik olarak HID görevi sayılmaz.

### Tuş ve kısayol kayıtları

Katman düzenleyicisinden seçilen yeni klavye görevleri, görünür açıklamaya ek olarak `KeyBehavior.primaryKey` veya `secondaryKey` içinde USB HID tuş kodu ve Ctrl/Shift/Option/Command modifier bitleriyle yerel olarak kaydedilir. Listede seçilen semboller karakter değil fiziksel tuş konumudur; çıkan karakter işletim sisteminin klavye düzenine bağlıdır. Bu bilgiler profiller ve normal yapılandırma yedekleriyle saklanır; mevcut USB sensör protokolü değişmemiştir.

Eski profillerdeki serbest metinler otomatik yorumlanmaz ve korunur. Kullanıcı listeden tekrar seçince yapılandırılmış görev oluşturulur. Diğer arayüz işlemleri eski açıklamayı değiştirirse yapılandırılmış kaydın `label` alanıyla güncel açıklama eşleşmesi kontrol edilmeden hiçbir cihaza aktarılmamalıdır. Hazır görevler, eski hazır profiller, encoder ve layer basılı davranışı henüz uçtan uca yapılandırılmış HID eşlemesi veya cihaza yazma mekanizması değildir. Makro, NKRO, cihaz senkronizasyonu ve fiziksel cihaz doğrulaması bu değişiklikle uygulanmış sayılmaz.

### Test düzeneği: 1 Kasım fiziksel kanal ölçümü

ESP32 test platformunda fiziksel tuşlara tek tek basılarak ham mux kanal sırası ölçüldü; satırlar soldan sağa `1, 0, 2 / 3, 4, 5 / 6, 7, 8 / 9, 10, 11`. Native uygulama bunları görselin sütun-sıralı KEY indekslerine çevirmek için `[1,3,6,9,0,4,7,10,2,5,8,11]` tablosunu kullanır. Bu sadece mevcut test düzeneği eşlemesidir; nihai PCB/C6 şeması ya da farklı bir kablolama için geçerli olduğu varsayılmamalıdır. Test sunucusu ve JSONL ham kanal sırası değişmez. Mevcut kalibrasyonlar ham kanal indeksinde tutulur, native görünüm bu indekslere eşleme üzerinden erişir; dolayısıyla eski kayıtlar yer değiştirilmeden korunur.

Kanal eşleme kullanıcı tarafından Canlı test → Kanal tanılama bölümünde tuş başına değiştirilir. Seçilen kanal başka bir tuşa aitse iki kanal yer değiştirir; 12 kanal birer kez atanır. Eşleme Mac'te yapılandırmayla ve yedekle birlikte saklanır; ham seri verisi veya test firmware'i değişmez. Kalibrasyonlar ham kanallara bağlı kalır. Tam kalibrasyon yokken model, bağlantı sırasında gözlenen en yüksek ADC değerine ve mıknatıs yaklaşınca değerin düştüğü varsayımına dayanarak yaklaşık hareket gösterir; bu yüzde/gerçek mesafe ölçümü değildir ve tuşlar serbestken bağlantı ile kalibrasyon tercih edilmelidir.

### macOS klavye çıkışı (sensör test platformu)

Katmanlar ekranındaki “Mac klavye çıktısını etkinleştir” seçeneği, kalibre edilmiş sensör basışlarını macOS klavye olaylarına dönüştürür. Varsayılan olarak kapalıdır ve macOS Erişilebilirlik izni gerektirir. Elle seçilmiş HID klavye görevleri ve `Taşı · G`, `Geri al · Cmd Z` gibi açık kısayol içeren görevler desteklenir. Desteklenen medya görevleri (çal/duraklat, önceki/sonraki, ses ve ekran parlaklığı) ve bazı standart macOS kısayolları da gönderilir; Minecraft Java varsayılan tuşlarının bir bölümü tanımlıdır. Diğer açıklama görevleri, encoder ve fiziksel layer düğmesi henüz çalışmaz. Katmanlar ekranında seçili görevin destek durumu gösterilir. Kullanıcı klavyeyi kalibre etmelidir; veri kesildiğinde veya 300 ms boyunca örnek yenilenmediğinde çıkıştaki basılı tuşlar bırakılır. Bu özellik analog sanal oyun kumandası üretmez ve CrossOver köprüsünden bağımsızdır.

Spotify profilindeki karıştır, tekrar, beğen, çalma listesi, arama ve kitaplık; Discord profilindeki mikrofon, kulaklık ve kanal geçişi için ilgili uygulamanın varsayılan Mac kısayolları tanımlıdır. Bu uygulamaya özel kısayollar yalnızca ilgili uygulama öndeyken gönderilir; değiştirilmiş uygulama kısayolları otomatik algılanmaz. Medya tuşları ayrı olarak macOS'a gönderilir. Diğer görevlerin birçoğu hâlâ uygulanmamıştır ve seçili tuşta destek durumu gösterilir.

Yerel analog oyun kumandası için CoreHID `HIDVirtualDevice` API'si araştırıldı. Mevcut imzasız test sürecinde basit cihaz örneğinin oluşturulması başarısız oldu; Apple sanal HID yetkilendirmesi belgeliyor. Bu nedenle yerel analog oyun kumandası henüz sağlanmaz; CrossOver test köprüsü bundan bağımsızdır.

### Sensör kalibrasyonu ve Mac tetikleme testi

Canlı test altındaki 12 tuş sihirbazı önce 0,8 saniye tüm kanallarda serbest örnek toplar; ardından satır sırasıyla her tuşun 0,8 saniye tam basılı ölçümünü ister. Yalnız farklı `sample_id` örnekleri sayılır; örnek yoksa, bağlantı koparsa veya kanal eşlemesi değişirse ilgili ölçüm iptal edilir. Çok değişken ölçüm veya 30 ADC'den küçük aralık reddedilir. Bu sınırlar arıza teşhisi değildir. Sonuçlar geçici tutulur; 12 tuş tamamlanıp tümü serbestken kullanıcı Kaydet'e basana kadar mevcut kalibrasyon değiştirilmez. Sihirbaz başladığında güvenlik için Mac klavye ve CrossOver sürüş çıkışları kapatılır; kullanıcı isterse sonrasında yeniden açar.

Tepki eğrisi fiziksel ADC kanalı başına saklanan 0,4–2,5 arası üstür: `pow(clamp((free-raw)/(free-pressed),0,1), exponent)`. Eski kayıtlarda alan yoktur ve 1,0 doğrusal kullanılır; çıkış, grafik önizlemesi ve Mac tetikleyicisi bu yüzdeden yararlanır. Eğri ayarı manyetik sensörün doyduğu bir bölgede kaybolan bilgiyi geri getirmez. Yeni isteğe bağlı alan normal JSON yedeğine dahildir; içe aktarma uzunluk ve sınırları doğrular.

Boşta gürültü ölçümü, yaklaşık 5 saniyelik farklı sunucu örnekleri üzerinden ham ADC min–maks farkı ve standart sapmasını hesaplar. Tuşlar serbest olmalıdır; sonuç kesin donanım teşhisi veya ADC'nin tüm örneklerinin ölçümü değildir.

Geleneksel tetikleme ve Rapid Trigger durumu kalibre edilmiş yerel sensör yüzdesinden hesaplanır; katman başına seçili SOCD çifti son basılana öncelik verir, aynı örnekte eşzamanlı ilk basışta nötre döner. Bu yerel durum desteklenen Mac klavye çıktısını sürer; C6/S3 ayarını değiştirmez, analog CrossOver sürüş çıkışına SOCD uygulamaz. Bağlantı / eşleme / katman / Hall ayarı değişiminde durum sıfırlanır. Gerçek fiziksel tuş basışlarıyla ve uygulama arayüzüyle uçtan uca henüz test edilmedi.

Canlı test sayfasındaki Sinyal oturumları kartı en yeni sekiz `.jsonl` dosyasının yalnızca son 64 KiB'ını arka planda inceler. Kapanmış oturumların süre, geçerli/geçersiz paket ve tespit edilen sıra boşluklarını `end` kaydından gösterir. Bitiş kaydı bulunmayan dosyayı kesin olarak açık saymaz; açık veya beklenmedik biçimde kesilmiş olabileceğini belirtir. Sırasız paket içeriyorsa tamlık bilinmiyor uyarısı gösterilir. Kayıtlar değiştirilmez.

### S3 üzerinden sensör köprüsü hazırlığı

Canlı testte USB sensör portu olarak C6'nın doğrudan portu veya bağımsız `examples/c6_s3_sensor_bridge` S3 seri portu seçilebilir. S3 portunu not etmek cihaz kimliğini veya haberleşmeyi doğrulamaz; sensör akışı olarak seçilirse değişiklik uygulama yeniden açıldığında etkinleşir. Köprü örneği şifresiz test yayınıdır, S3 HID veya ayar yazma sağlamaz ve gerçek kartlarla henüz doğrulanmamıştır. Ürün C6/S3 firmware'iyle aynı protokol değildir.
