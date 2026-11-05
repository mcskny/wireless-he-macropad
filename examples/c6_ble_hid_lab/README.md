> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [bütünleşik rehbere](../../Docs/Proje_Rehberi.md) bakın.

# C6 BLE HID laboratory

This isolated ESP-IDF 6.1 build explores the HID-over-GATT transport for the
ESP32-C6. It is **not** the working ESP-NOW sensor firmware and must not be
flashed over it for normal use.

The laboratory defines keyboard and consumer-control report maps and offers
report-send functions that require an encrypted BLE link. It advertises as
`Wireless HE Lab` and enables bonding using Secure Connections / Just Works.
Just Works does not authenticate the peer against MITM attacks; pairing policy,
user-controlled pairing and host compatibility remain to be designed and tested.

The physical keys, C6 configuration storage, selected profile, layer switch,
mode switching, power policy, disconnect handling and sensor sampling are not
connected to this experiment. No device has been tested. It has no measured
latency or power claims. NVS initialization never erases existing flash on error.

From this directory, in an ESP-IDF 6.1 environment:

```sh
idf.py set-target esp32c6
idf.py build
```

Build does not flash a board. Do not assume this standalone image preserves or
uses the working bridge's settings; it uses a different partition layout.

Transport-independent prototypes and tests are in
`../c6_s3_sensor_bridge/shared/` and `../c6_s3_sensor_bridge/tests/`:
the Layer + KEY 1 state machine, report encoding, and validated profile staging.
These are not integrated with either radio yet.
