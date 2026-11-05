# C6 doğrudan BLE — güncel durum (5 Kasım 2026)

Güncel mimari, fiziksel denemeler, Layer + KEY 0 kullanımı ve sınırlamalar [bütünleşik rehberde](Proje_Rehberi.md) açıklanır. C6 doğrudan Mac ile BLE HID bağlantısı kurdu ve kullanıcı tuş çıktısını doğruladı. Eşleşme anahtarları için NVS kalıcılığı sonradan açıldı; yeniden başlatma ve mod dönüşü testleri henüz tamamlanmadı. ESP-NOW profil aktarımı CRC ve MAC kontrolüne sahiptir fakat kimlik doğrulamalı değildir. Just Works MITM koruması sağlamaz.

Entegre kod `examples/c6_s3_sensor_bridge/c6`, ayrı laboratuvar `examples/c6_ble_hid_lab` altındadır. İkisini karıştırmayın.
