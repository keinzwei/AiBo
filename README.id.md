# AiBo 4.3.3 — Artificial Intelligence Bot

**Bot Counter-Strike 1.6 berbasis YaPB dengan Human Aim, navigasi hibrida dan pembelajaran lokal.**

**Pembuat dan pengelola proyek AiBo: K9_einzwei** (GitHub: [keinzwei](https://github.com/keinzwei)).

[English](README.md) · [Unduh installer](https://github.com/keinzwei/AiBo/releases/tag/v4.3.3) · [Panduan build](docs/human-copycat/BUILD.md) · [Kredit](CREDITS.md)

AiBo adalah fork komunitas independen yang menggabungkan fondasi YaPB dengan Human Copycat. Pemrosesan perilaku dan pembelajaran berjalan lokal saat pertandingan, tanpa GPU, Python saat bermain, LLM, atau langganan API.

## Kelebihan

- **Navigasi hibrida:** `.GRAPH`, jalur manusia yang diamati, dan eksplorasi geometri dengan pemeriksaan lantai, ruang serta koneksi. Versi ini tetap memakai graph.
- **Patroli CT Dust2:** pencarian musuh ke Tunnel, T-Spawn dan Pit saat keadaan memungkinkan; tugas bom dan kontak musuh didahulukan.
- **Human Aim:** reaksi, batas kecepatan putar, variasi aim, recoil, tapping, burst, spray, counter-strafe, preaim dan scope AWP.
- **42 profil pemain:** preferensi AWP/rifle dan bobot kemampuan simulasi dari peringkat tahunan HLTV yang dicatat dalam dokumentasi.
- **Enam mekanisme belajar:** reinforcement learning, imitation, self-play, lifelong learning, continual adaptation dan multi-agent learning berbasis tabel/ringkasan lokal.
- **Variasi taktik:** formasi pembukaan, smoke di sekitar objective, boost fisik bila collision memungkinkan, memori tempat dan strategi.
- **Kepribadian simulasi:** persaingan skor, suasana emosi simulasi dan chat prosedural dalam 16 bahasa profil.
- **Installer Windows:** pemeriksaan folder game, penggabungan konfigurasi plugin, backup bernomor dan pemulihan saat gagal.
- **Source lengkap:** kode C++, dependensi vendored, tes, konfigurasi dan kode installer.

## Instalasi

1. Unduh `AiBo-4.3.3-Setup.exe` dari halaman Releases.
2. Tutup CS dan HLDS, lalu jalankan installer.
3. Pilih folder utama Counter-Strike atau folder `cstrike`.
4. Periksa console: `aibo version`, `aibo_human_aim_status`, `aibo_learning_status` dan `aibo_final_status`.

Konfigurasi utama: `cstrike/addons/AiBo/conf/AiBo.cfg`. Default Human Aim adalah level `100`; gunakan `aibo_human_aim_level -1` untuk mode adaptif. Server 32 slot dapat menampung 31 bot bersama satu manusia. Boost memerlukan collision pemain yang sesuai dengan pengaturan Semiclip.

File yang diganti dicadangkan sebagai `.bak`, `.bak.1`, dan seterusnya. Graph custom dipertahankan. `autobuy.txt` dan `listenserver.cfg` bawaan paket ikut dipasang; periksa pengaturannya bila server memiliki konfigurasi khusus.

## Pengujian dan batas kemampuan

Pembelajaran memakai tabel Q, prior demonstrasi, estimasi pola lawan dan bandit formasi. Ini bukan neural network dan tidak menjanjikan bot selalu bertambah kuat. Self-play berjalan hanya saat pertandingan bot melawan bot berlangsung.

Nama pemain profesional dan peringkat HLTV adalah referensi simulasi, bukan dukungan resmi atau jaminan kemampuan setara pemain asli. Hit dan kill tetap ditentukan engine game.

Catatan paket menyebut 293 tes kode dan 11 skenario installer lulus pada Linux, dengan host simulasi dan core installer. Pertandingan CS 1.6 Windows dan UI installer belum termasuk dalam pengujian tersebut. Rincian: [docs/VALIDATION.md](docs/VALIDATION.md).

## Dokumentasi dan lisensi

Lihat [README utama](README.md) untuk pengaturan, perintah build dan indeks panduan. Laporan bug serta kontribusi diterima di repository ini.

Dibuat dan dikelola oleh **K9_einzwei** (GitHub: [keinzwei](https://github.com/keinzwei)), dengan kredit kepada YaPB/jeefo, tim YaPB, PODBot/Count Floyd, RealBot Human Copycat, pembuat graph dan kontributor dependensi. Fork gabungan memakai GPL-2.0-or-later sebagaimana dinyatakan dalam pemberitahuan paket; setiap komponen mempertahankan lisensinya. Lihat [CREDITS.md](CREDITS.md), [LICENSE](LICENSE) dan [NOTICE-AiBo.md](NOTICE-AiBo.md).
