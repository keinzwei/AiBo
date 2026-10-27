# Validasi AiBo 4.3.3

293/293 pengujian kode dan 11/11 skenario installer lulus setelah pembersihan source.
DLL Windows x86 berhasil dibangun; 10 export engine/Metamod sesuai dan import hanya
DLL sistem. Resource DLL 4.3.3.1. Installer NSIS EXE dibangun setelah validasi plugins.ini
memastikan tepat satu `win32 addons/AiBo/bin/AiBo.dll` serta semua DLL rujukan tersedia.

Tes baru memeriksa 42 peran pembelian pada kedua tim dan alias nama, pencegahan
pengambilan AWP oleh rifler, bobot HLTV tanpa perubahan frag, ketiga daerah pencarian
dengan graph Dust2 1238 node asli, prioritas kontak/bom dan tujuan tak tersambung.
Tes graph memberikan kedatangan posisi pada host uji; tidak menjalankan fisika game.

Tes installer memeriksa pemasangan baru, path Unicode, merge plugins.ini, backup .bak
sebelum penggantian, cadangan bernomor, kedua file unggahan yang identik, pemulihan
saat gagal, instal ulang idempotent, perlindungan graph custom, penolakan DLL arsitektur
salah, path salah dan symlink. Ini core installer native Linux; UI dan aturan Windows
terhadap file yang sedang dipakai belum dijalankan pada Windows.

Source untuk distribusi hanya rilis aktif. Histori versi, patch lama, implementasi
lama, log, executable dan build cache tidak masuk folder source/. Fixture sintetis
1200 perintah menggantikan source referensi lama; toleransi sudut 0.001 derajat dan
keputusan fire harus sama. Dependensi vendored dan lisensi tetap disertakan.

Pertandingan CS 1.6 sebenarnya, pergerakan pada BSP Dust2, perbandingan dengan pro
player, dan workflow GitHub Actions belum dijalankan. Hasil tes bukan jaminan menang
atau bebas masalah pada setiap mod/server. Bukti tes rilis saat ini ada di folder
validation/ paket lengkap; tidak ada histori rilis sebelumnya.
