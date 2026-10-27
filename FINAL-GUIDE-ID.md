# Panduan FINAL 4.3.3

Tambahan 4.3.3: **HUMAN-AIM-4.3.3-ID.md** menjelaskan level otomatis, teknik tembakan dan boost dua tahap pada box tinggi.

## Pasang dan mulai

1. Tutup CS 1.6/HLDS, ekstrak seluruh ZIP, jalankan `AiBo-4.3.3-Setup.exe`, lalu pilih folder `cstrike` permainan. Installer mencadangkan berkas yang ditimpa dan menonaktifkan entri AiBo/RealBot lama di Metamod. Pengalaman belajar serta GRAPH yang sudah ada dipertahankan.
2. Pilih **Max Players = 32** sebelum membuat server. Untuk HLDS melalui command line, gunakan `-game cstrike +maxplayers 32 +map de_dust2` bersama opsi server milik Anda. Kuota bot tidak dapat memperbesar server 10 atau 16 slot yang sudah berjalan.
3. Pastikan hanya satu AiBo aktif dengan `meta list`, lalu jalankan `aibo_human_status` dan `aibo_final_status`. Versi harus 4.3.3 FINAL. Bot masuk bertahap setelah penundaan awal server dan selama masih tersedia spawn point.
4. Biarkan beberapa ronde berjalan agar overlay geometri, pengalaman, dan memori lokasi berkembang. File GRAPH dasar tetap diperlukan untuk membuat bot dan tugas native, tetapi tidak menjadi batas wilayah eksplorasi.

Dengan 32 slot, satu manusia dapat bermain bersama 31 bot. Jika dua manusia sudah berada di server, kapasitas bot menjadi 30. Satu manusia + 31 bot mengisi semua slot; manusia berikutnya memerlukan slot kosong atau kebijakan reservasi server. Untuk selalu menyisakan satu slot tambahan pada kondisi itu, pakai `aibo_autovacate_keep_slots 2` sehingga bot maksimal 30. Tidak ada klaim bahwa tambahan manusia dapat menembus server yang penuh.

## Konfigurasi aktif

| Console/cvar | Default paket | Dampak |
|---|---:|---|
| `aibo_quota` | `31` | Target bot, dibatasi slot/manusia/spawn |
| `aibo_quota_mode` | `normal` | Kuota menghitung bot |
| `aibo_final_brain` | `1` | Perilaku FINAL dan integrasi Human Copycat |
| `aibo_final_variety` | `1` | Memori tempat, tujuan baru, batas camping |
| `aibo_final_emotions` | `1` | Risiko/agresivitas dari keadaan emosi simulasi |
| `aibo_final_memory` | `1` | Muat/simpan memori lokasi `.hfinal` |
| `aibo_final_boost` | `1` | Koordinasi boost fisik dua bot satu tim |
| `aibo_final_lobby` | `1` | Kapasitas 31 serta jeda join/leave |
| `aibo_final_join_min` / `aibo_final_join_max` | `0.5` / `2.0` | Jeda antarbot yang masuk otomatis |
| `aibo_rotate_bots` | `1` | Aktifkan rotasi keluar-masuk |
| `aibo_rotate_stay_min` / `aibo_rotate_stay_max` | `180` / `1800` | Masa tinggal acak dalam detik |
| `aibo_show_latency` | `2` | Ping simulasi di scoreboard |
| `aibo_explore` / `aibo_explore_percent` | `1` / `40` | Eksplorasi geometri; persentase bot scout per ronde |
| `aibo_human_chat_language` | `profile` | Bahasa tetap sesuai profil masing-masing |
| `aibo_final_chat_gap` | `1.8` | Jeda minimum global chat dalam detik |

Pengaturan ditaruh di `cstrike/addons/AiBo/conf/AiBo.cfg`. Gunakan kuota lebih kecil jika FPS server turun. Jumlah spawn pada map/plugin juga membatasi jumlah bot yang dapat diisi otomatis; DLL tidak membuat spawn palsu atau memaksa 31 bot pada semua map.

## Mengamati perilaku

- `aibo_final_status`: strategi, mood, risiko, rank dan frags setiap bot, jumlah keputusan lokasi, fallback pengulangan, boost selesai/gagal, serta jumlah masuk/keluar sejak map dimuat. Rank/mood diperbarui berkala; validitas klaim top fragger diperiksa lagi sebelum chat dikirim.
- `aibo_explore_status`: ukuran overlay dan cakupan eksplorasi. Peta geometri tumbuh bertahap; bot tidak mengetahui seluruh BSP secara ajaib.
- `aibo_human_save`: simpan demonstrasi, overlay, pengalaman dan memori FINAL yang berubah. Autosave memakai `aibo_human_autosave`.
- `aibo_human_reload`: muat ulang data map/fisika yang sedang aktif.

Lokasi pilihan bot tidak harus berbeda setiap frame. Setelah memilih tujuan, bot mengikuti rute yang koheren. Memori menekan pengulangan tempat berhenti/tujuan; bot tetap dapat melewati jalur penghubung, kembali ke tujuan objektif, dan memakai cover darurat.

## Boost dan Semiclip

Bot mencari permukaan datar dengan kenaikan sekitar 60–132 unit di dekat pasangan yang siap. Maksimal satu pasangan per tim aktif, dengan cooldown untuk menghindari percobaan terus-menerus. Ruang pendaratan dan lintasan diperiksa. Helper crouch, climber melompat; tahap lanjutan hanya dimulai setelah engine menyatakan climber berdiri di atas helper.

Jika memakai Semiclip, atur plugin tersebut agar tabrakan vertikal yang dibutuhkan untuk boost tetap bekerja, atau nonaktifkan no-block untuk pengujian boost. Paket ini tidak mengedit plugin AMXX Anda. Tanpa kontak fisik, percobaan akan dibatalkan. Bila ingin mematikan percobaan: `aibo_final_boost 0`.

Pemilihan AWP mengikuti peran pada PRO-PROFILES-4.3.3-ID.md. Tidak semua bot menjadi sniper; penahanan sudut tidak menjamin posisi yang sama dipilih setiap ronde. Arah sniper diperiksa memakai geometri/pandangan. Penyisiran CT juga mengenali batas wilayah Dust2 untuk mencapai daerah yang jarang dikunjungi.

## Data dan sifat generatif

File baru `.hfinal` memakai format `HCFINAL1`, terikat nama map dan fingerprint BSP/fisika. Ada batas 128 profil, 32 tempat per profil dan checksum. Muatan rusak atau tidak cocok ditolak tanpa menimpa memori aktif. Berkas lama `.rbcc`, `.hnav`, `.hlearn` dan `.hexplore` tetap dipakai oleh subsistemnya masing-masing. Paket tidak memuat rekaman pribadi atau model pralatih.

Chat merupakan komposisi prosedural lokal dengan konteks HP, uang, senjata, frags, rank, mood dan aktivitas. Tidak membutuhkan API atau internet. Bahasa Rusia/Arab/Asia memerlukan font/klien yang mampu menampilkan UTF-8; DLL tidak memasang font atau mengubah encoding klien. Tanggapan antarbot dibatasi satu balasan untuk satu percakapan agar tidak membuat lingkaran chat.

Rujukan batas slot: [Steam Support — Half-Life Dedicated Server](https://help.steampowered.com/en/faqs/view/081A-106F-B906-1A7A). Acuan fisika: [ReGameDLL_CS movement code](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/pm_shared/pm_shared.cpp). Bukti pengujian build ini ada di `VALIDATION.md`; referensi engine bukan bukti pertandingan Dust2 telah dijalankan.
