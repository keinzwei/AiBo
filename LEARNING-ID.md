# Enam kemampuan belajar — AiBo 4.3.3

Semua kemampuan baru aktif secara default dalam DLL permainan dan AiBo.cfg.
Ini implementasi pembelajaran online lokal yang ringan: tabel Q, prior demonstrasi
manusia, model pola lawan, dan bandit formasi tim. Tidak memerlukan Python,
GPU, internet, API, atau model unduhan pada komputer yang menjalankan CS.
Bot belajar selama pertandingan biasa berlangsung. Paket tidak membawa
pengalaman pribadi pemain atau model yang sudah dilatih.

| Kemampuan | Implementasi yang berjalan dalam game |
| --- | --- |
| Reinforcement Learning | Q-learning memilih gerakan native, menahan posisi, strafe kiri, strafe kanan, atau mundur. Damage, kill, kematian, team damage, dan hasil ronde memberikan reward/penalti. |
| Imitation Learning | Perintah manusia sungguhan membentuk prior pilihan gerak. Rekaman RBCC, model aim, dan graf rute manusia dari versi sebelumnya tetap bekerja. Bot lain tidak dimasukkan sebagai demonstrasi manusia. |
| Self-Play Learning | Transisi melawan bot ikut memperbarui kebijakan ketika pertandingan bot melawan bot dijalankan. Kedua sisi belajar dari pertandingan yang sama; jumlah pembaruan ditandai terpisah. |
| Lifelong Learning | Nilai Q cepat/lambat, jumlah pengalaman, prior manusia, pola lawan, dan hasil taktik disimpan ke file `.hlearn`, lalu dibaca kembali pada map dan kondisi fisika yang sama. |
| Continual Adaptation | Pengamatan lawan yang benar-benar terlihat memperbarui pola mendekat/menahan posisi. Perbandingan estimasi baru dan lama mendeteksi perubahan; pola itu menjadi bagian state dan bias pilihan gerak yang kecil. |
| Multi-Agent Learning | Bot satu tim memakai kebijakan pengalaman bersama. Formasi pembukaan split, tekanan sayap A, atau tekanan sayap B belajar dari hasil ronde. Reward tim juga diberikan kepada aksi tempur yang sedang berjalan. |

## Cara memakai

1. Tutup CS/HLDS, ekstrak seluruh paket, lalu jalankan `AiBo-4.3.3-Setup.exe`.
   Metamod harus sudah aktif. Pemasang mencadangkan file yang ditimpa,
   mempertahankan data belajar lama, dan menonaktifkan baris plugin AiBo/RealBot lama.
2. Mulai map, tambahkan bot di kedua tim, lalu bermain seperti biasa.
   Bermain sendiri melawan bot juga menghasilkan demonstrasi manusia.
3. Console: `aibo_human_status` untuk versi dan ringkasan; `aibo_learning_status`
   untuk angka pembelajaran. Angka mulai kecil/nol pada map baru adalah normal.
4. Data otomatis disimpan mengikuti `aibo_human_autosave` (default 120 detik),
   saat pergantian map, atau penutupan normal server. `aibo_human_save` memaksa
   penyimpanan. `aibo_human_reload` menyimpan lalu memuat ulang data.

Untuk latihan self-play, cukup jalankan pertandingan CS biasa yang berisi bot
T dan CT. Fitur ini tidak membuat proses simulator lain, tidak otomatis membuka
server baru, dan tidak menjalankan latihan saat CS ditutup.

## Pengaturan

| Console/cvar | Default | Fungsi |
| --- | ---: | --- |
| `aibo_learning` | 1 | Sakelar utama sistem pembelajaran baru. |
| `aibo_learning_rl` | 1 | Pembaruan Q dari reward. Saat 0, bobot yang sudah ada masih dapat dipakai untuk inferensi. |
| `aibo_learning_imitation` | 1 | Mengambil dan memakai prior gerak manusia dalam kebijakan baru. |
| `aibo_learning_selfplay` | 1 | Mengizinkan pembaruan Q dari lawan bot dan hasil tim yang hanya menghadapi bot. |
| `aibo_learning_lifelong` | 1 | Membaca/menulis `.hlearn`. Saat 0, pengalaman sesi tetap ada di RAM; reload/map baru membersihkannya. |
| `aibo_learning_adaptation` | 1 | Pengamatan pola lawan serta pemakaian pola dalam keputusan. |
| `aibo_learning_multiagent` | 1 | Belajar formasi dan reward ronde bersama. Tabel Q tempur tetap dibagi per tim saat opsi ini 0. |
| `aibo_learning_exploration` | 6 | Persen eksplorasi di antara gerakan yang lolos pemeriksaan keselamatan, rentang 0–30. |

Untuk membuat pengaturan permanen, edit `cstrike/addons/AiBo/conf/AiBo.cfg`.
Rekaman/peniruan RBCC lama tetap memiliki sakelar `aibo_human_record` dan
`aibo_human_replay` sendiri. Prior gerak baru mengambil sampel melalui jalur
perekaman manusia, sehingga `aibo_human_record 1` juga diperlukan. Pembatasan
guru `aibo_human_teacher` tetap berlaku. Prior gerak baru digabung per tim;
`aibo_human_profile` tetap khusus pemilih demonstrasi/model RBCC lama.

## Statistik yang dapat diperiksa

- `Q_updates`: jumlah transisi tempur yang benar-benar memperbarui Q.
- `human_samples`: sampel prior aksi manusia, dibatasi maksimal 4/s per guru.
- `selfplay_updates`: bagian dari `Q_updates` yang berasal dari lawan bot.
- `team_results`: hasil taktik yang dikreditkan per tim; satu ronde dapat
  menambah dua jika kedua tim menjalankan formasi dan mendapat pengalaman.
- `observed`, `pattern`, `changes`: pengamatan dan perubahan pola lawan dari
  perspektif tim tersebut. Pattern 0 = campuran, 1 = mendekat, 2 = menahan posisi.
- `tactics(split/A/B)`: nilai cepat hasil formasi. A/B adalah dua sayap tujuan
  yang dipilih dari graf map, bukan pengenalan nama bombsite resmi pada semua map.

## Detail teknis dan batas kemampuan

Kebijakan memakai 432 state diskret: tim, fase ronde, kelompok HP, jarak/visibilitas
lawan, dukungan rekan, status objektif, dan pola lawan. Masing-masing memiliki
5 aksi. Q-learning menggunakan reward terbatas, faktor diskonto 0,85, nilai cepat
dengan laju 0,16, serta nilai lambat 0,025. Pemilihan menggabungkan keduanya
70/30 dengan prior demonstrasi terbatas. Ini bukan neural network atau Deep RL.
Memori berupa ringkasan terbatas, bukan penyimpanan semua frame selamanya.

Keputusan gerak baru dibuat sekitar setiap 0,75 detik. Pola lawan diambil paling
cepat 2/s per bot yang sedang bertempur. Gerak strafe/mundur yang sedang dipakai
tetap diperiksa setiap perintah terhadap hull, dinding, dan lantai. Gerak hanya
diubah melalui input GoldSrc biasa; tidak ada perubahan langsung velocity,
teleportasi, auto-headshot, atau pembacaan posisi lawan tersembunyi untuk pola.

Gerakan baru tunduk pada tugas objektif, granat, kebutaan, freeze, tangga,
air, jalur lompat, ruang sempit, kemacetan, dan replay manusia yang sedang aktif.
Karena replay asli memiliki prioritas, Q_updates dapat bertambah lebih lambat
ketika banyak rekaman cocok. Gerak mundur tidak ditawarkan untuk pembawa C4,
VIP, atau bot yang sedang membawa hostage. Pengamanan aim dan friendly fire
dari versi sebelumnya tetap menjadi pemeriksaan akhir.

Self-play berarti pembelajaran dari pertandingan online bot melawan bot, bukan
liga checkpoint/populasi kebijakan historis atau latihan berjuta pertandingan
secara otomatis. Multi-agent memakai berbagi parameter dan reward tim sederhana;
tidak menggunakan centralized neural critic atau merancang taktik bebas dari nol.
Pembelajaran formasi diterapkan pada pembukaan map demolition dengan dua sayap
tujuan yang terpisah dan tim sedikitnya dua bot; perubahan beban sayap paling
terlihat pada tim lima bot (split 2/2/1 menjadi pressure 3/2 atau 2/3).
Free-for-all tidak menggunakan mesin pembelajaran baru.

Data baru berada di `cstrike/addons/AiBo/data/copycat/<map>-<fingerprint>.hlearn`
dengan format `HCLEARN1`. Fingerprint mengikuti BSP dan parameter fisika engine.
File sekitar 35 KB/map/fingerprint, disimpan dengan temporary file lalu atomic
replace, serta diperiksa panjang, dimensi, nama map, environment, checksum, dan
nilai numeriknya saat dibaca. File tidak cocok/rusak ditolak dan dilaporkan di
console. File `.rbcc` dan `.hnav` lama tetap terpisah dan kompatibel.

Tidak ada janji bot pasti semakin kuat pada setiap ronde. Reward, jumlah data,
keberagaman lawan, dan abstraksi state membatasi hasil; bot bisa belajar kebiasaan
yang kurang berguna jika contoh/pertandingannya buruk. Validasi sintetis membuktikan
mekanisme dan integrasi yang diuji, bukan kenaikan win rate dalam pertandingan
Windows CS 1.6 nyata. Lihat `VALIDATION.md` untuk hasil dan batas pengujian.
