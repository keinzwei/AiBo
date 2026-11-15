# HUMAN AIM dan Boost — FINAL 4.3.3

Default paket adalah batas kemampuan Pro `100`. Profil pemain dan bobot peringkat memberi variasi kemampuan individual; lihat `PRO-PROFILES-4.3.3-ID.md`. Kuota 31 bot, bahasa profil dan fitur 4.3.0 tetap aktif; lihat `FINAL-GUIDE-ID.md` untuk server 32 slot.

## Level dan proses belajar

| Pengaturan | Default | Fungsi |
|---|---:|---|
| `aibo_human_aim_adaptive` | `1` | Kalibrasi kemampuan dari manusia yang bermain |
| `aibo_human_aim_level` | `100` | Batas Pro; `-1` otomatis; `0` mempertahankan aim 4.3.0 |
| `aibo_human_aim_techniques` | `1` | Disiplin tapping/burst/spray, kontrol momentum dan timing scope |
| `aibo_human_aim_wallbang` | `1` | Wallbang terbatas dari kontak baru yang sah, efektif mulai level adaptasi sekitar 35% |
| `aibo_human_aim_preaim` | `1` | Penempatan crosshair menurut navigasi/kontak yang diketahui |
| `aibo_human_aim_quickswitch` | `1` | Ganti senjata sesudah tembakan AWP yang terkonfirmasi |
| `aibo_human_aim_crouchhop` | `1` | Crouch-hop sesekali pada level atas dan geometri aman |
| `aibo_human_aim_save` | `1` | Simpan kalibrasi manusia dan pengalaman motor bot ke `.haim` |
| `aibo_human_objective_smoke` | `1` | Smoke pelindung sebelum plant/defuse, dengan prioritas waktu bom |
| `aibo_final_boost` | `1` | Koordinasi dua bot untuk permukaan lebih tinggi |

Pengaturan ada di `cstrike/addons/AiBo/conf/AiBo.cfg`. Default `aibo_human_aim_skill 100` dan `aibo_human_reaction_skill 100`. Batas global tetap Pro, sedangkan level teknik efektif dikalikan bobot peringkat profil; error bidik dan waktu reaksi juga dibedakan. Profil peringkat bawah tetap dapat menang dan belajar. Matikan pembobotan dengan `aibo_human_hltv_skill 0` bila ingin level yang sama.

`aibo_human_aim_level 100` sudah aktif. Gunakan `aibo_human_aim_level -1` untuk memilih mode otomatis di bawah ini. `aibo_human_aim_level 0` mempertahankan kontrol aim sebelumnya. Untuk menonaktifkan sistem aim baru: `aibo_human_aim_adaptive 0` dan `aibo_human_aim_level -1`.

Dalam mode otomatis (-1), pada awal pengamatan aim belum dinaikkan tanpa bukti. Sedikitnya 20 peluru teramati dan dua sampel reaksi atau duel diperlukan untuk mulai memperoleh keyakinan. Sampel bertambah melalui pertempuran normal; bot tidak memberikan kill atau menambah frag untuk kalibrasi. Refleks yang terlalu singkat untuk diukur tidak dianggap otomatis sebagai bukti reaksi sempurna.

Ketika bot sedang menghadapi manusia yang telah terukur, profil lawan itu diutamakan. Jika tidak ada profil tersebut, statistik manusia lawan di server dipakai; tim bot yang tidak memiliki lawan manusia dapat mengikuti referensi manusia di timnya. Spectator tidak menjadi referensi. Jika sama sekali tidak ada manusia aktif, pengalaman self-play dapat mendorong peningkatan. Pemain baru yang datanya belum cukup tidak langsung dianggap pro.

Sistem memakai statistik terbaru yang dilunakkan, confidence, dan perubahan level yang dibatasi. Satu headshot, satu kematian atau satu ronde buruk tidak langsung mengubah seluruh server menjadi sangat lemah/kuat. Ini estimasi berbasis observasi, bukan jaminan pertandingan selalu imbang.

## Tembakan dan headshot alami

- Tapping melepaskan tombol di antara tembakan yang terkonfirmasi. Rifle jarak jauh lebih sering ditap; pistol mengikuti jeda yang sesuai.
- Burst biasa menargetkan 2–5 peluru. Profil precision dapat mencoba 4–6 peluru pada jarak jauh di level atas. Gerak, error kompensasi recoil, cooldown, ammo atau hilangnya sasaran dapat menghentikannya lebih cepat.
- Spray dipakai lebih dekat; NEO memiliki kecenderungan spray lebih besar. Kontrol recoil memakai punchangle yang teramati dan sudut pandang yang digerakkan secara terbatas. Tidak ada pembacaan seed peluru atau pembatalan spread acak.
- Pemilihan kepala tetap melewati pemeriksaan FOV, garis pandang dan smoke. Jika kepala terhalang tetapi torso terlihat, bot dapat memilih torso. Pengendali masih membutuhkan waktu reaksi, putaran dan koreksi; error kecil tetap ada pada level 100.
- Counter-strafing adalah input arah berlawanan velocity yang terukur. Bot tidak memaksa velocity menjadi nol dan tidak menembak presisi saat masih terlalu cepat atau di udara. Akurasi penuh tidak dijamin hanya dengan berhenti: aturan inaccuracy/spread senjata tetap berlaku.

Tidak ada kode untuk memberikan headshot, mengubah hitgroup, damage, health atau frag. Hit/headshot yang terjadi tetap diputuskan game engine berdasarkan peluru sebenarnya.

## AWP: fast zoom dan quick switch

Bot meminta zoom lewat attack2, lalu menunggu FOV benar-benar scoped dan bidikan cukup siap. Quick switch baru dilakukan setelah pesan ammo membuktikan sebuah peluru ditembakkan. Dalam situasi yang sesuai ia memilih sidearm/knife sebentar lalu kembali ke AWP.

Quick switch tidak menghapus cooldown/draw time AWP. Pengendali juga mempertahankan jeda AWP sekurangnya 1,45 detik dari tembakan yang teramati; engine/mod server boleh mensyaratkan waktu lebih lama. Tidak ada penulisan next-attack timer atau manipulasi animasi senjata.

## Sudut AWP: Pit → A Long → Bombsite A

Sniper mengevaluasi pandangan ke node bombsite yang diketahui serta geometri pintu/celah. Arah dari Pit adalah naik melewati A Long menuju A; bukan menunduk ke lantai Pit. Probe tambahan memeriksa sudut naik/turun dan perbedaan lantai agar posisi rendah atau hasil boost tetap dapat memberi sudut tembak. Bombsite yang terhalang ditolak melalui trace; koordinat musuh tersembunyi tidak dipakai.

Posisi Double Door juga dinilai dari celah pandang yang terbuka. Saat menahan sudut, navigasi preaim tidak meratakan pitch sniper. Pengamatan sasaran nyata dan kalibrasi aim atas mendukung jarak sampai 8192 unit, dengan cooldown, scope, reaksi, error dan geometri tetap berlaku. Ini perilaku berbasis geometri, bukan waypoint yang menjamin setiap AWP selalu berada di Pit. Arah sniper diuji dengan geometri sintetis; pemilihan tujuan tambahan diuji dengan graph Dust2 asli, belum solver fisika BSP Dust2.

## Smoke sebelum plant/defuse

Pada map bom, bot memprioritaskan pembelian smoke dari uang yang benar-benar tersedia. Saat siap melakukan objective, bot meminta rekan satu tim terdekat yang mempunyai smoke dan jalur lempar aman. Jika tidak ada, pekerja dapat melempar sendiri bila waktu mencukupi, lalu melanjutkan tugas plant/defuse yang tersimpan. Screen ditempatkan dekat pekerja ke arah pendekatan/kontak sah yang diketahui, tanpa membaca posisi lawan tersembunyi.

Bot menunggu smoke yang benar-benar meledak di dekat objective, paling lama sekitar 4,5 detik. Granat yang diam tetapi belum meledak tidak dihitung sebagai cover. Tidak ada pemunculan asap/granat buatan atau perubahan velocity proyektil. Satu request aktif per tim dan cooldown membatasi lemparan serempak. Smoke mengurangi pandangan, bukan memberikan kebal peluru atau jaminan selamat dari spam.

Progress plant/defuse yang sudah berjalan tidak diinterupsi. Defuse dengan sisa waktu pendek tidak menunggu atau mengganti senjata untuk melempar sendiri; rekan tetap boleh membantu sambil defuser bekerja. Tidak punya smoke, pembatasan senjata, jalur lempar terhalang, helper mati atau lemparan gagal membuat objective dilanjutkan, bukan ditunda tanpa batas. Matikan dengan `aibo_human_objective_smoke 0`.

## Wallbang dan pre-fire

Wallbang baru dikendalikan oleh `aibo_human_aim_wallbang`; jalur wall targeting lama tetap diblokir oleh Human Copycat. Perintah lama `aibo_shoots_thru_walls` bukan cara mengaktifkan wall tracking tanpa informasi.

Bot memerlukan salah satu dari berikut:

- posisi yang benar-benar terlihat pada perintah terbaru, dengan umur memori sekitar 1,1 detik;
- posisi sumber suara yang benar-benar lolos pendengaran native, dengan umur sekitar 0,75 detik dan ketidakpastian arah/jarak.

Tidak ada pembaruan koordinat sasaran ketika lawan bergerak di balik tembok. Kontak juga gugur setelah identitas pemain berubah. Pemeriksaan jalur bersifat konservatif: satu penghalang, budget senjata/material, ketebalan terbatas dan ruang sesudah penghalang harus masuk akal. Tembakan dihentikan bila ada rekan setim di koridor tersebut. Empat peluru terkonfirmasi membatasi satu jendela percobaan; jeda serta umur informasi mencegah penembakan terus-menerus.

Ini percobaan penetrasi, bukan jaminan peluru akan mengenai lawan. Informasi bisa sudah usang, suara tidak tepat, atau engine menolak penetrasi. Bot dapat gagal seperti manusia.

## Crouch-hop dan boost

Crouch-hop dilakukan sesekali ketika mendekati sumber suara, bukan saat sedang melepaskan tembakan presisi. Jalur tanah dan ruang kepala diperiksa; objective, utility, ladder, water dan boost terkoordinasi memiliki prioritas. Engine tetap menentukan percepatan, jump limit, suara langkah dan landing sound. Crouch-jump/bunny hop tidak dijanjikan selalu cepat atau senyap.

Untuk boost, bot mencari teman satu tim dan permukaan 60–132 unit lebih tinggi. Permukaan di atas 112 unit memakai tahapan tambahan: naik ke helper yang crouch, beri ruang agar helper berdiri, bertumpu lagi di atas helper, lalu jump/crouch menuju box. Dua kontak pijakan serta pendaratan harus benar-benar teramati. Target yang melampaui batas tidak dipaksakan.

Sesudah berhasil, climber menahan sudut selama sekitar 3–6,5 detik sebelum kembali bergerak. Fitur ini bekerja dengan pembatas camping/memori tempat; ia tidak membuat bot mengulang box yang sama terus-menerus. Maksimal satu pasangan per tim, timeout dan cooldown tetap berlaku. Combat atau tugas mendesak membatalkan percobaan.

**Collision antarpemain diperlukan.** Atur Semiclip/no-block agar memungkinkan boost, atau matikan no-block untuk mengujinya. Paket tidak mengubah plugin AMXX Anda. Tidak ada teleport, tambahan velocity atau jaminan semua posisi dari video dapat dijangkau. Skenario box 128 unit telah diuji dalam host sintetis, dengan keadaan kontak/posisi diberikan oleh test; solver pergerakan CS asli belum dijalankan.

## Data dan pemeriksaan

`aibo_human_aim_status` menampilkan level tiap bot, pengalaman motor, jenis disiplin tembakan, jumlah peluru burst yang terkonfirmasi, serta statistik/confidence manusia. `aibo_final_status` menampilkan keberhasilan/gagal boost dan keadaan strategi. `aibo_human_save` menyimpan perubahan; autosave dan reload juga mencakup aim baru.

`.haim` memakai format `HCAIM001`, checksum dan ikatan map/BSP/fisika, maksimal 128 profil. Nama/Steam ID tidak ditulis secara mentah dalam berkas ini; kunci berasal dari hash identitas (atau nama untuk LAN), dengan ruang kunci manusia dan bot dipisah. Ia menyimpan statistik serta latihan, bukan koordinat musuh yang tersembunyi. Memori target sesaat tidak dipulihkan setelah restart. Berkas rusak/tidak cocok ditolak tanpa menimpa memori aktif. File lama `.rbcc`, `.hnav`, `.hlearn`, `.hexplore` dan `.hfinal` tetap dipertahankan.

Level tertinggi merupakan profil teknik kompetitif yang fallible, bukan klaim telah mengalahkan atau menyamai pemain pro tertentu dalam benchmark. Detail tes ada di `VALIDATION.md`; build ini belum diuji dalam pertandingan Windows CS 1.6 atau BSP Dust2 asli.

Acuan engine: [AWP](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/wpn_shared/wpn_awp.cpp), [AK-47](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/dlls/wpn_shared/wpn_ak47.cpp), dan [pergerakan/suara](https://github.com/rehlds/ReGameDLL_CS/blob/master/regamedll/pm_shared/pm_shared.cpp) pada ReGameDLL_CS. Referensi tersebut menjelaskan mekanik; bukan bukti bahwa sebuah boost tertentu sudah berhasil di server Anda.
