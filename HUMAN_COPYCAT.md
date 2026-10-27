# AiBo Human Copycat 4.3.3

AiBo menggabungkan navigasi/taktik native, input aim yang dibatasi, pembelajaran
lokal dan percakapan prosedural. Tidak memerlukan layanan AI, Python saat bermain,
model unduhan atau koneksi internet. Asal kode dan lisensi ada di NOTICE-AiBo.md.

## Perilaku aktif

- CT Dust2 menyisir Tunnel, T-Spawn dan Pit melalui beberapa titik berbeda untuk
  memburu musuh. Sebagian CT menjaga objective. Kontak sah, bom dan tugas mendesak
  memiliki prioritas. Rute harus terhubung dan bisa dijalani secara fisik.
- 42 profil memakai peran AWP/rifle dan bobot kemampuan dari peringkat tahunan
  HLTV yang terverifikasi. Lihat PRO-PROFILES-4.3.3-ID.md untuk data dan sumber.
- HUMAN AIM mengendalikan sudut pandang dan tombol tembak sebelum RunPlayerMove.
  Reaksi, kecepatan putaran, noise, recoil, tapping/burst/spray, scope dan momentum
  tetap dapat menghasilkan kesalahan. Engine menentukan spread, cooldown dan hit.
- Visibilitas melewati trace/FOV/flash/smoke. Suara memakai event yang benar-benar
  terdengar; informasi memiliki umur dan ketidakpastian. Koordinat musuh tersembunyi
  tidak diperbarui untuk mengarahkan wallbang. Lihat HUMAN-AIM-4.3.3-ID.md.
- Navigasi memakai GRAPH, overlay jalur manusia dan eksplorasi geometri yang
  memeriksa lantai, ruang, koneksi dan kemajuan. Input jump/duck/boost tidak
  memberikan teleport atau velocity tambahan. Detail ada di EXPLORE-CHAT-ID.md.
- Pembelajaran Q lokal, demonstrasi, self-play, pengalaman berkelanjutan, adaptasi
  pola teramati dan formasi tim dijelaskan di LEARNING-ID.md. Ini metode lokal
  terbatas, bukan pemahaman atau kehendak bebas manusia.
- Memori lokasi, strategi berganti, persaingan skor dan emosi simulasi membentuk
  variasi perilaku. Chat memakai 16 bahasa menurut profil dan balasan antarbot.
  Lobby simulasi mendukung hingga 31 bot pada server 32 slot.

## Pengaturan dasar

| Pengaturan | Default | Fungsi |
|---|---:|---|
| aibo_human_aim_level | 100 | Batas Pro; -1 memilih mode otomatis |
| aibo_human_aim_skill | 100 | Aim dasar |
| aibo_human_reaction_skill | 100 | Reaksi dasar |
| aibo_human_pro_profiles | 1 | Peran senjata profil |
| aibo_human_hltv_skill | 1 | Bobot kemampuan individual |
| aibo_human_ct_patrol | 1 | Penyisiran CT Dust2 |

Konfigurasi lengkap: cfg/addons/AiBo/conf/AiBo.cfg. Perintah console:
`aibo version`, `aibo_human_aim_status`, `aibo_final_status`, `aibo_human_save`.

## Data pembelajaran

| Format/file | Isi |
|---|---|
| .rbcc / RBCC0003 | Demonstrasi input dan statistik |
| .hnav / HCNAV001 | Koneksi jalur manusia yang teramati |
| .hlearn / HCLEARN1 | Nilai pembelajaran lokal |
| .hexplore / HCEXPL01 | Eksplorasi geometri dan kunjungan |
| .hfinal / HCFINAL1 | Memori tempat serta pengalaman strategi |
| .haim / HCAIM001 | Kalibrasi manusia dan latihan motor bot |

Data terikat map/fisika dan diperiksa ketika dimuat. File rusak/tidak cocok ditolak.
Dataset pribadi tidak dibundel. Fixture tests/fixtures/ merupakan data sintetis.
Identifier wire format tetap kompatibel meskipun nama produk sudah AiBo.

## Build dan distribusi

README.md menjelaskan pemasangan EXE dalam paket lengkap. Source hanya rilis aktif;
tidak berisi histori rilis, patch lama, implementasi lama atau hasil kompilasi.
Untuk GitHub, isi folder source/ menjadi root repository. Panduan build ada di
docs/human-copycat/BUILD.md; validasi dan keterbatasan ada di docs/VALIDATION.md.
Lisensi gabungan GPL-2.0-or-later dan atribusi dependensi tetap disertakan.
