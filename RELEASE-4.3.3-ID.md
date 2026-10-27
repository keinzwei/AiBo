# AiBo FINAL 4.3.3

CT menyisir wilayah untuk mencari musuh. Ketika situasi ronde memungkinkan,
sebagian CT bergerak ke Tunnel, T-Spawn dan Pit pada `de_dust2`, mengecek beberapa
titik berbeda dan mengarahkan pandangan ke sudut sekitar. Titik yang baru diperiksa
menjadi kurang menarik. Kontak yang terlihat/terdengar langsung mengalihkan bot
ke pertempuran; bom dan tugas mendesak tetap didahulukan.

Hingga sekitar separuh CT mendapat giliran berburu, berganti antar ronde; sebagian
tetap menjaga posisi. Pemilihan mencakup seluruh node GRAPH yang terhubung, bukan
sampel acak kecil. Daerah tanpa jalur GRAPH dapat dicoba oleh eksplorasi geometri
yang sudah ada, dengan pemeriksaan lantai, ruang dan koneksi. Penghalang fisik tetap
berlaku. Tidak ada janji menjangkau setiap sudut atau rute yang memang tidak tersambung.
Fallback graph Dust2 1238 node disertakan; graph pengguna yang sudah ada dipertahankan.

42 profil memiliki peran senjata. GuardiaN, kennyS, markeloff dan spesialis AWP lain
mengutamakan AWP jika uang dan aturan server mengizinkan. NEO dan f0rest mengutamakan
rifle: M4A1 untuk CT, AK-47 untuk T; mereka tidak mengambil AWP. AWPer boleh memakai
rifle sementara ketika tidak mampu membeli AWP. Ini adaptasi peran ke ekonomi CS 1.6.

Peringkat tahunan terbaik HLTV yang terverifikasi menjadi bobot kemampuan individual.
NEO memakai #1 (2011), f0rest #2 (2013). Profil tinggi memiliki teknik efektif lebih
kuat, error lebih kecil dan reaksi lebih cepat dibanding profil di urutan bawah.
Kill dan skor tetap hasil pertandingan. Tidak ada pemberian frag atau jaminan
peringkat tertentu selalu menang. Tabel lengkap, sumber dan rumus ada di
`PRO-PROFILES-4.3.3-ID.md`.

Installer `AiBo-4.3.3-Setup.exe` menyertakan `autobuy.txt` dan `listenserver.cfg`
unggahan Anda tanpa mengubah satu byte. Jika isi lama berbeda, file lama dipindahkan
ke `.bak` sebelum file baru dipasang. Jika `.bak` sudah ada, digunakan `.bak.1`,
`.bak.2`, dan seterusnya. Kegagalan pemasangan memulihkan file asli. Instal ulang
dengan isi sama tidak menambah cadangan. Validasi `plugins.ini` dan DLL dijalankan
sebelum installer dibangun, lalu diperiksa kembali saat pemasangan.

## Pengaturan dan pemeriksaan

| Pengaturan | Default | Fungsi |
|---|---:|---|
| `aibo_human_ct_patrol` | `1` | Penyisiran musuh CT pada Dust2 |
| `aibo_human_pro_profiles` | `1` | Peran AWP/rifle menurut nama profil |
| `aibo_human_hltv_skill` | `1` | Bobot kemampuan dari peringkat tahunan |
| `aibo_human_aim_level` | `100` | Batas Pro; `-1` memilih mode adaptif |
| `aibo_human_aim_skill` | `100` | Skill aim dasar |
| `aibo_human_reaction_skill` | `100` | Skill reaksi dasar |

Gunakan `aibo version`, `aibo_human_aim_status` dan `aibo_final_status` di console.
Status mencantumkan peran, peringkat/tahun, bobot serta area penyisiran.
`aibo_human_hltv_skill 0` menyamakan bobot tanpa mengubah peran senjata.

Tapping, burst, spray, counter-strafe, scope/switch AWP, preaim, wallbang berbasis
kontak, boost, smoke objective, 16 bahasa profil, memori belajar dan lobby hingga
31 bot tetap tersedia. Boost memerlukan tabrakan pemain yang diizinkan Semiclip.

## Validasi rilis

293/293 pengujian kode dan 11/11 skenario installer lulus. Pengujian rute memuat
graph Dust2 asli tetapi memberikan perpindahan posisi melalui host uji; solver
fisika game dan BSP asli tidak dijalankan. DLL dan installer Windows x86 berhasil
dibangun. UI, kunci file Windows dan pertandingan CS 1.6 asli belum diuji di lingkungan
ini. Lihat `VALIDATION.md` dan log yang disertakan.

## Source untuk GitHub

Folder source/ berisi hanya rilis aktif. Arsip versi terdahulu, patch histori,
snapshot implementasi lama, log dan hasil kompilasi tidak disertakan. Regresi aim
memakai fixture numerik sintetis sehingga tidak memerlukan source versi lama.
Dependensi vendored dan lisensi/atribusi penulis tetap lengkap. Isi folder source/
dapat dijadikan root repository; workflow build dan panduan sudah disesuaikan.
