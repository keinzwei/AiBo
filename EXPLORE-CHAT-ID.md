# Eksplorasi dan percakapan lintas bahasa — AiBo 4.3.3

Panduan umum: **FINAL-GUIDE-ID.md**. Katalog memakai 23 topik dan 1.440 fragmen; bagian berikut menjelaskan eksplorasi dan percakapan aktif.

Pembaruan ini menambahkan navigasi dari geometri yang ditemukan sendiri dan
percakapan antarkarakter dengan bahasa profil masing-masing. Keduanya aktif
dalam konfigurasi permainan. Enam kemampuan belajar lokal juga tersedia.

## Mengapa perilaku sebelumnya berulang

Versi sebelumnya memakai rute native dari `.GRAPH` dan overlay `.hnav` yang hanya
bertambah ketika manusia merekam gerakannya. Pemilih pembukaan juga mengikat
hampir semua anggota tim ke dua sayap tujuan selama 75 detik. Akibatnya, variasi
acak pada biaya rute tidak cukup untuk menemukan area yang tidak ada dalam
kedua sumber itu, dan CT jarang memilih daerah spawn T sebagai tujuan.

## Yang sekarang dilakukan bot

- Bot membangun jaringan navigasi **terpisah dari `.GRAPH`**, dimulai dari
  posisi yang benar-benar ditempati. Pemeriksaan hull dan lantai memperluas
  jaringan ke delapan arah, termasuk area yang tidak memiliki node native
  maupun rekaman manusia. Tidak ada daftar koordinat tunnel/pit yang ditanam
  khusus untuk Dust2.
- Tujuan dipilih dari area yang belum atau jarang dikunjungi oleh tim tersebut.
  Penilaian juga memperhatikan umur kunjungan, panjang perjalanan, variasi
  antarronde, dan tujuan rekan. Reservasi tujuan hanya dibagi dalam satu tim.
  Karena daerah spawn lawan tidak dilarang oleh tag node native pada overlay
  ini, daerah itu dapat dijelajahi jika ada jalur fisik yang aman.
- Default 40% bot yang memenuhi syarat bergiliran menjadi penjelajah. Pembawa
  C4, VIP, dan pembawa hostage tidak mengambil peran ini. Pada tim berisi lebih
  dari satu kandidat, setidaknya satu kandidat tetap menangani tugas native.
  Kontak musuh, suara musuh, tugas penting, bom terpasang, HP rendah, serta
  30 detik terakhir pada map objektif menghentikan eksplorasi santai.
- Jalur berjalan mengikuti tangga kecil, kemiringan yang dapat dilalui,
  penurunan terbatas, serta lorong yang memerlukan crouch. Pemeriksaan antara
  titik mencegah jalur melintasi celah lantai hanya karena ujungnya aman.
  Segmen diperiksa kembali ketika digunakan. Segmen terhalang atau tidak
  menghasilkan kemajuan dibatalkan; daerah sekitar diperiksa ulang berkala.
- AWP mempertimbangkan pandangan panjang yang ditemukan dari raycast geometri.
  Setelah mencapai posisi yang sesuai, bot dapat berhenti beberapa detik dan
  mengawasi arah terbuka sebelum bergerak lagi. Ini tidak mengenali label
  “pit” secara ajaib atau memperoleh posisi musuh tersembunyi.

`.GRAPH` tetap menjadi fondasi mesin AiBo untuk pembuatan bot, tujuan objektif,
dan manuver native yang rumit. Pembaruan ini **menghapus batas wilayah gerak
yang ditentukan node file tersebut**, bukan menghapus seluruh subsistem AiBo.
Map yang sama sekali tidak mempunyai `.GRAPH` masih memerlukan graph dasar agar
AiBo dapat membuat bot. Jaringan eksplorasi dapat tumbuh dan mengarahkan gerak
ke luar cakupan graph dasar itu tanpa mengedit file `.GRAPH`.

Pemetaan berlangsung bertahap selama permainan dan disimpan antarpermainan.
Tidak ada jaminan setiap sudut sebuah BSP akan didatangi dalam satu ronde.
Area tertutup, loncatan presisi, lompatan jurang, lift, tangga vertikal, air,
atau area yang memerlukan urutan interaksi khusus masih mengandalkan jalur
native yang sesuai. Dinding dan lantai tetap membatasi gerak fisik bot.

## Bahasa profil, bercampur dalam satu percakapan

Default `aibo_human_chat_language "profile"` mempertahankan bahasa karakter.
Hiko memakai bahasa Inggris, Dosia bahasa Rusia, dan f0rest bahasa Swedia.
Ketika salah satu menyapa, karakter lain dapat membalas dalam **bahasanya
sendiri**, bukan menyalin bahasa pengirim atau menggantinya secara acak.

Contoh bentuk percakapan yang dapat disusun:

| Karakter | Bahasa profil | Contoh isi |
| --- | --- | --- |
| Hiko | Inggris | hello everyone; timing matters here |
| Dosia | Rusia | привет всем; без спешки |
| f0rest | Swedia | hej allihop; håll lite avstånd |
| ISSAA | Arab | مرحباً بالجميع |
| xccurate | Indonesia | halo semuanya; jaga jarak antar pemain |

Katalog berisi **16 bahasa, 1.104 fragmen**, dan 16 maksud percakapan: sapaan,
pujian, permintaan maaf, utility, ekonomi, senjata, hasil duel, kematian,
plant, eksplorasi, rotasi, penjagaan sudut, rencana, klarifikasi, obrolan ringan,
dan kondisi HP. Kalimat menggabungkan awalan, isi, penutup, serta keadaan aktual
seperti uang, senjata, granat dan HP. Balasan rencana/permintaan maaf memakai
bentuk respons tersendiri. Memori topik dan riwayat kalimat mengurangi pengulangan.

Roster default diperluas dari 28 nama klasik menjadi 42 profil agar Asia,
Timur Tengah, Amerika, dan beberapa bahasa Eropa dapat ikut muncul. Lihat
`PROFILE-LANGUAGES.md`. Nama tidak berarti model meniru kepribadian, suara,
keterampilan pribadi, atau percakapan asli para pemain tersebut.

Satu pesan spontan dapat memperoleh satu balasan yang dijadwalkan dengan
jeda pengetikan dan cooldown. Balasan itu tidak memulai rantai baru. Pengirim
dan penjawab berbeda, bahasa profil dipertahankan, kanal tim dan status
hidup/mati diperiksa, serta balasan tertunda dibatalkan jika keadaan pihak
terkait berubah. Percakapan tidak memerlukan pemain manusia untuk memulainya.

Ini generator bahasa **prosedural lokal berbasis konteks**, bukan LLM atau
pemahaman percakapan bebas setara ChatGPT. Dialog antarkarakter memakai metadata
maksud yang diketahui; pesan manusia dikenali melalui kata kunci yang didukung.
Tidak ada API, model unduhan, atau biaya layanan AI. Huruf Rusia, Arab, Jepang,
Korea dan Mandarin dikirim sebagai UTF-8; tampilan dan pembentukan huruf Arab
bergantung pada versi klien/font CS yang digunakan. Semua pesan dibatasi pada
150 byte tanpa memotong karakter UTF-8 di tengah.

## Pengaturan dan pemeriksaan

| Pengaturan | Default | Fungsi |
| --- | ---: | --- |
| `aibo_explore` | 1 | Eksplorasi mandiri dan pengendalian rute di luar cakupan node native. |
| `aibo_explore_percent` | 40 | Persentase kandidat penjelajah yang bergiliran per tim. |
| `aibo_explore_probes` | 16 | Maksimum percobaan ekspansi per 50 ms, dibagi seluruh bot. Pemetaan dibatasi anggaran 2 ms per pembaruan. |
| `aibo_explore_save` | 1 | Penyimpanan overlay dan riwayat kunjungan. |
| `aibo_human_chat_language` | profile | Bahasa per nama profil. `auto`/`mix` juga berarti profil tetap. Kode seperti `en` atau `ru` memaksa satu bahasa untuk semua jika admin menginginkannya. |
| `aibo_human_chat_catalogue` | 1 | Generator multibahasa baru. Nilai 0 menyediakan fallback katalog lama Inggris/Indonesia. |
| `aibo_human_chat_conversation` | 1 | Balasan antarkarakter yang diberi jeda dan dibatasi. |
| `aibo_human_international_roster` | 1 | Menambahkan 14 profil internasional ke 28 profil klasik. Perubahan pool nama memerlukan restart/reload konfigurasi nama. |

Console `aibo_explore_status` menunjukkan jumlah node/edge hasil pemetaan,
penjelajah aktif, percobaan probe, kedatangan, dan segmen yang dibatalkan.
`aibo_chat_status` menunjukkan mode bahasa, jumlah fragmen, pesan yang disusun,
balasan yang dikirim, serta bahasa setiap bot. `aibo_human_status` juga menampilkan
ringkasannya. `aibo_human_save` menyimpan seluruh jenis pengalaman yang berubah.

Data geometri berada di
`cstrike/addons/AiBo/data/copycat/<map>-<fingerprint>.hexplore` (format `HCEXPL01`).
Data terikat BSP/fisika, memiliki checksum, validasi ukuran, koordinat,
tautan dan versi; disimpan melalui temporary file dan atomic replace. Batas
overlay 16.384 node dengan jarak sampel horizontal sekitar 64 unit dan maksimal
16 tautan per node. Data lama `.rbcc`, `.hnav` dan `.hlearn` tetap dipertahankan.

## Memasang dan mencoba pada Dust2

1. Tutup CS/HLDS, ekstrak seluruh ZIP, dan jalankan `AiBo-4.3.3-Setup.exe`.
2. Pilih folder `cstrike` yang benar. Pemasang memperbarui DLL **dan konfigurasi**.
   Menyalin DLL saja dapat menyisakan `aibo_human_chat_language id` dari versi lama.
3. Buka `de_dust2`, gunakan graph dasar yang valid, lalu mainkan beberapa ronde.
   Periksa `aibo_explore_status`: jumlah node baru dan kedatangan seharusnya
   bertambah jika lantai/hull dapat diperiksa dan penjelajah tidak terus sibuk
   bertempur. Tidak diperlukan demonstrasi manusia untuk memulai pemetaan baru.
4. Periksa `aibo_chat_status`: mode harus `profile`, katalog dan percakapan aktif.
   Pengaturan `aibo_chat 0` tetap mematikan chat.

Regresi sintetis memeriksa tunnel/pit buatan, kunjungan seluruh 73 region
yang dapat dicapai dalam skenario itu, gerak di luar empat node native,
tangga/crouch/celah lantai, prioritas objektif, penyimpanan, dan percakapan
Hiko–Dosia. Pengujian ini bukan uji BSP Dust2 atau pertandingan Windows nyata.
Lihat `VALIDATION.md` untuk hasil lengkap dan batas verifikasinya.
