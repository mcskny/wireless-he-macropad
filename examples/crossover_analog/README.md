> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [ana README’ye](../../Readme.md) bakın.

# CrossOver analog test köprüsü

Bu deney, eski ESP32'nin seri sensör verisini değiştirmez veya ESP32'yi HID aygıtına çevirmez. Mac Studio yalnız kullanıcı etkinleştirdiğinde kalibre edilmiş direksiyon/gaz/fren değerlerini `127.0.0.1:27861` UDP üzerinden yollar. CrossOver'ın **ayrı kopyasındaki** SDL köprüsü bunları Wine içinde sanal gamepad eksenlerine aktarır. Orijinal `/Applications/CrossOver.app` değiştirilmedi. Bu kopya `~/Library/Application Support/Wireless HE Studio/CrossOver Runtime/CrossOver.app` içindedir. Steam şişesindeki `winebus` için `Enable SDL=1` ayarı açılmıştır.

## Başlatma

1. Önce CrossOver içindeki Steam'i ve o şişede açık oyunları kapat.
2. `launch_steam.command` dosyasını aç. Bu, **Steam şişesini test CrossOver kopyasıyla** başlatır. Normal CrossOver uygulamasından açmak köprüyü yüklemez. Script çalışan şişeyi zorla kapatmaz.
3. Wireless HE Studio → Katmanlar → Assetto Corsa → Oyun profili. Gaz, fren, sol ve sağ tuşları hem serbest hem tam basılı kalibre et. Bu dört tuş farklı olmalıdır.
4. `CrossOver’a analog test verisi gönder` seçeneğini aç. `Köprüye canlı veri gönderiliyor` mesajı UDP alındığını gösterir; oyunun joystick'i doğru okuduğunu tek başına kanıtlamaz.
5. Assetto Corsa ayarlarında kumandayı/eksenleri eşle. SDL eksenleri: sol çubuk X = direksiyon, sol tetik = gaz, sağ tetik = fren. Oyun/girdi katmanı adları farklı gösterebilir. Ölçek ve yönleri oyunda test edip ayarla.

Sensör verisi 300 ms tazelenmezse veya dört tuş kalibre değilse uygulama nötr yollar. Köprü 250 ms veri almazsa direksiyonu merkeze, gaz ve freni serbest konuma getirir. Köprü kapalıysa, başka layer'a geçildiyse veya UDP alıcısı yoksa çalıştığı varsayılmaz. Açıkken ağ yalnız localhost'tur; UDP protokolü kimlik doğrulamaz ve aynı Mac'teki diğer süreçler portu kullanabilir.

## Durum ve sınırlar

- x86_64 SDL köprüsü yerel denemede test edildi: eksen aktarıldı ve zaman aşımında sıfırlandı. CrossOver test şişesinde ve Steam şişesinde Wine sanal HID aygıtını oluşturdu; UDP yanıtı alındı.
- 2 Kasım 2026'da gerçek sensör → uygulama → Wine → Assetto zincirinde dört eksen denendi; kullanıcı direksiyon, gaz ve frenin oyun içinde çalıştığını bildirdi. Oyun ayarlarında `wheel / custom` seçilip eksenler eşlendi. Bu, başka oyunlarda veya C6/S3 donanımında doğrulama anlamına gelmez.
- Diğer tuşlar, vites, encoder, fiziksel layer, otomatik profil ve cihaza yazma bu köprüde uygulanmaz. Test platformu sadece sensör verisi gönderir.
- Profil uygulama/kaydetme Mac Studio'nun yerel profil sistemidir. Sağ üstteki cihaz `Yazdır` düğmesi hâlâ desteklenmemektedir.
- CrossOver veya SDL güncellemesinden sonra test kopyası eskimiş olabilir. Kopya bağımsızdır; orijinal CrossOver uygulaması güncellenince otomatik güncellenmez. Orijinal CrossOver SDL dosyası köprü tarafından çalışma anında kullanılır.
