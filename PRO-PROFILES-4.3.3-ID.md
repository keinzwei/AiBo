# Profil senjata dan bobot HLTV — AiBo 4.3.3

Tabel berikut adalah data yang benar-benar dikompilasi ke DLL. Peringkat merujuk
prestasi tahunan terbaik yang terverifikasi pada acuan ini, bukan ranking pemain
aktif hari ini. Daftar lintas era CS 1.6/CS:GO/CS2 dipakai untuk mewakili masa kuat
masing-masing pemain. HLTV tidak menyediakan ranking universal sepanjang masa
untuk seluruh 42 nama tersebut.

Peran AWP/rifle adalah adaptasi gaya bermain ke CS 1.6, bukan klaim bahwa pemain
hanya pernah memakai satu senjata. Pasha, karrigan dan beberapa pemain lain pernah
berganti/hybrid; karrigan di sini memakai periode AWPer CS 1.6 tahun 2011. Untuk
rifler, pilihan AK-47 T dan M4A1 CT mengikuti ekonomi/aturan game, bukan survei
pribadi senjata favorit. AWP tetap membutuhkan uang, izin server dan kepemilikan
senjata sungguhan. Alias Kenny-S, Forest dan PashaBiceps dikenali, termasuk (2).

Bobot simulasi = `1 - (rank - 1) * 0.025` untuk peringkat 1–20. Level teknik efektif
= level pengalaman/setting × bobot. Tambahan error sudut = `1 + (1-bobot)*1.5` kali;
tambahan waktu reaksi = `(1-bobot)*120` ms. Ini rumus rancangan AiBo, bukan formula
HLTV atau probabilitas kill. Peringkat #1 berbobot 1; #2 0.975; #10 0.775; #20 0.525.
Entri tanpa peringkat terverifikasi memakai bobot awal 0.5. Tanda — berarti belum
dipetakan pada referensi Top 20 ini, bukan peringkat 21 atau penilaian buruk terhadap
pemain. Nama custom yang tidak dikenali tetap memakai kebijakan generik lama.

NEO/f0rest diharapkan lebih kompetitif dari profil urutan bawah karena kontrol
bidik/reaksi/teknik, tanpa tambahan frag, damage, HP atau pembacaan lawan tersembunyi.
Pengalaman belajar tetap tersimpan. Setting global 100 adalah batas Pro; perbedaan
individual tetap aktif. `aibo_human_hltv_skill 0` mematikan bobot dan
`aibo_human_pro_profiles 0` mematikan aturan senjata profil secara terpisah.

| Profil | Peran AiBo | HLTV | Tahun | Bobot | Acuan ranking |
|---|---|---:|---:|---:|---|
| NEO | Rifle | 1 | 2011 | 1.000 | [HLTV](https://www.hltv.org/news/8054/top-20-players-of-2011-neo-1) |
| f0rest | Rifle | 2 | 2013 | 0.975 | [HLTV](https://www.hltv.org/news/11964/top-20-players-of-2013-f0rest-2) |
| GeT_RiGhT | Rifle | 1 | 2013 | 1.000 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| markeloff | AWP | 1 | 2010 | 1.000 | [HLTV](https://www.hltv.org/news/5888/top-20-players-of-2010-introduction) |
| trace | Rifle | 3 | 2010 | 0.950 | [HLTV](https://www.hltv.org/news/5888/top-20-players-of-2010-introduction) |
| starix | Rifle | 4 | 2010 | 0.925 | [HLTV](https://www.hltv.org/news/5888/top-20-players-of-2010-introduction) |
| Edward | Rifle | 5 | 2010 | 0.900 | [HLTV](https://www.hltv.org/news/5888/top-20-players-of-2010-introduction) |
| zonic | Rifle | 8 | 2010 | 0.825 | [HLTV](https://www.hltv.org/news/5888/top-20-players-of-2010-introduction) |
| moddii | Rifle | 9 | 2010 | 0.800 | [HLTV](https://www.hltv.org/news/5888/top-20-players-of-2010-introduction) |
| Delpan | AWP | 9 | 2011 | 0.800 | [HLTV](https://www.hltv.org/news/7984/top-20-players-of-2011-introduction) |
| TaZ | Rifle | 6 | 2011 | 0.875 | [HLTV](https://www.hltv.org/news/7984/top-20-players-of-2011-introduction) |
| Friis | AWP | 7 | 2011 | 0.850 | [HLTV](https://www.hltv.org/news/7984/top-20-players-of-2011-introduction) |
| karrigan | AWP | 8 | 2011 | 0.825 | [HLTV](https://www.hltv.org/news/7984/top-20-players-of-2011-introduction) |
| Gux | Rifle | 10 | 2011 | 0.775 | [HLTV](https://www.hltv.org/news/7984/top-20-players-of-2011-introduction) |
| shox | Rifle | 3 | 2013 | 0.950 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| Dosia | Rifle | 4 | 2013 | 0.925 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| NBK | Rifle | 5 | 2013 | 0.900 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| Xizt | Rifle | 6 | 2013 | 0.875 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| ScreaM | Rifle | 7 | 2013 | 0.850 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| JW | AWP | 5 | 2014 | 0.900 | [HLTV](https://www.hltv.org/news/13903/top-20-players-of-2014-introduction) |
| Hiko | Rifle | 9 | 2013 | 0.800 | [HLTV](https://www.hltv.org/news/11895/top-20-players-of-2013-ranking) |
| GuardiaN | AWP | 2 | 2015 | 0.975 | [HLTV](https://www.hltv.org/news/16848/top-20-players-of-2015-guardian-2) |
| flusha | Rifle | 2 | 2014 | 0.975 | [HLTV](https://www.hltv.org/news/13903/top-20-players-of-2014-introduction) |
| pasha | AWP | 3 | 2014 | 0.950 | [HLTV](https://www.hltv.org/news/13903/top-20-players-of-2014-introduction) |
| Snax | Rifle | 4 | 2014 | 0.925 | [HLTV](https://www.hltv.org/news/13903/top-20-players-of-2014-introduction) |
| kennyS | AWP | 6 | 2014 | 0.875 | [HLTV](https://www.hltv.org/news/13903/top-20-players-of-2014-introduction) |
| KRiMZ | Rifle | 7 | 2015 | 0.850 | [HLTV](https://www.hltv.org/news/16800/top-20-players-of-2015-krimz-7) |
| Happy | Rifle | 8 | 2015 | 0.825 | [HLTV](https://www.hltv.org/news/16785/top-20-players-of-2015-happy-8) |
| BnTeT | Rifle | — | — | 0.500 | Belum dipetakan |
| xccurate | AWP | — | — | 0.500 | Belum dipetakan |
| somebody | Rifle | — | — | 0.500 | Belum dipetakan |
| XigN | Rifle | — | — | 0.500 | Belum dipetakan |
| Laz | Rifle | — | — | 0.500 | Belum dipetakan |
| ISSAA | Rifle | — | — | 0.500 | Belum dipetakan |
| BOROS | Rifle | — | — | 0.500 | Belum dipetakan |
| FalleN | AWP | 2 | 2016 | 0.975 | [HLTV](https://www.hltv.org/news/19655/top-20-players-of-2016-fallen-2) |
| coldzera | Rifle | 1 | 2016 | 1.000 | [HLTV](https://www.hltv.org/news/19662/top-20-players-of-2016-coldzera-1) |
| XANTARES | Rifle | 14 | 2025 | 0.675 | [HLTV](https://www.hltv.org/news/43524/top-20-players-of-2025-xantares-14) |
| gob b | Rifle | — | — | 0.500 | Belum dipetakan |
| EliGE | Rifle | 4 | 2019 | 0.925 | [HLTV](https://www.hltv.org/news/28858/top-20-players-of-2019-elige-4) |
| Stewie2K | Rifle | — | — | 0.500 | Belum dipetakan |
| sh1ro | AWP | 3 | 2022 | 0.950 | [HLTV](https://www.hltv.org/news/35379/top-20-players-of-2022-sh1ro-3) |

## Sumber primer peran/riwayat senjata

- [NEO](https://www.hltv.org/stats/players/weapon/165/neo?csVersion=CSGO)
- [f0rest](https://www.hltv.org/stats/players/weapon/29/f0rest?csVersion=CSGO)
- [GuardiaN](https://www.hltv.org/news/11926/top-20-players-of-2013-guardian-10)
- [kennyS](https://www.hltv.org/news/13985/top-20-players-of-2014-kennys-6)
- [markeloff](https://www.hltv.org/news/6001/top-20-players-of-2010-markeloff-1)
- [Delpan](https://www.hltv.org/news/5938/top-20-players-of-2010-delpan-10)
- [Friis](https://www.hltv.org/news/8020/top-20-players-of-2011-friis-7)
- [karrigan](https://www.hltv.org/news/8013/top-20-players-of-2011-karrigan-8)
- [JW](https://www.hltv.org/news/13990/top-20-players-of-2014-jw-5)
- [pasha](https://www.hltv.org/news/14001/top-20-players-of-2014-pasha-3)
- [xccurate](https://www.hltv.org/news/23489/xccurate-maybe-for-now-ill-be-a-more-of-a-support-awper)
- [FalleN](https://www.hltv.org/news/19655/top-20-players-of-2016-fallen-2)
- [sh1ro](https://www.hltv.org/news/33137/top-20-players-of-2021-sh1ro-4)
- [XigN](https://www.hltv.org/stats/players/weapon/9172/xign?csVersion=CSGO)
- [BOROS](https://www.hltv.org/news/43203/boros-re-signs-with-jijiehao-issaa-departs)
- [BnTeT](https://www.hltv.org/news/40214/bntet-i-think-that-im-still-capable-of-playing-internationally)

Artikel tahunan HLTV di tabel memberi konteks peran untuk pemain lainnya. Pemilihan
rifle bagi profil tanpa acuan senjata individual merupakan pemetaan taktis AiBo;
tidak diklaim sebagai hasil wawancara favorit senjata. Sumber diperiksa 26 Oktober 2026.
