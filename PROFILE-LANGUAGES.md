# Profil bahasa karakter bot

Pemetaan ini adalah pilihan bahasa untuk karakter simulasi berdasarkan profil
negara; bukan klaim mengenai bahasa sehari-hari, gaya bicara, atau dukungan dari
orang yang namanya dipakai. Tidak ada model pribadi/rekaman suara pemain.
Nama custom yang tidak terdaftar memakai fallback bahasa Inggris.

| Bahasa | Kode | Karakter |
| --- | --- | --- |
| Inggris | en | Hiko, EliGE, Stewie2K |
| Indonesia | id | BnTeT, xccurate |
| Swedia | sv | f0rest, GeT_RiGhT, moddii, Delpan, Gux, Xizt, JW, flusha, KRiMZ |
| Denmark | da | trace, zonic, Friis, karrigan |
| Polandia | pl | NEO, TaZ, pasha, Snax |
| Prancis | fr | shox, NBK, ScreaM, kennyS, Happy |
| Rusia | ru | Dosia, sh1ro |
| Ukraina | uk | markeloff, starix, Edward |
| Slovakia | sk | GuardiaN |
| Portugis | pt | FalleN, coldzera |
| Turki | tr | XANTARES |
| Arab | ar | ISSAA, BOROS |
| Mandarin | zh | somebody |
| Korea | ko | XigN |
| Jepang | ja | Laz |
| Jerman | de | gob b |

Rujukan profil publik untuk perluasan roster antara lain:

- [HLTV: BnTeT dan xccurate dalam aturan kebangsaan WESG](https://www.hltv.org/news/24845/qz-to-stand-in-for-tyloo-at-wesg)
- [HLTV: ISSAA](https://www.hltv.org/player/10449/issaa)
- [HLTV: BOROS](https://www.hltv.org/player/19763/boros)
- [HLTV: somebody](https://www.hltv.org/player/8605/somebody)
- [HLTV: Laz](https://www.hltv.org/stats/players/9585/laz)
- [VLR: XigN](https://www.vlr.gg/player/15535/xign)
- [HLTV: FalleN](https://www.hltv.org/player/2023/fallen)
- [HLTV: XANTARES](https://www.hltv.org/player/7938/xantares)
- [HLTV: gob b](https://www.hltv.org/player/136/gob-b)

Nama klasik dan rujukan peringkat historis tetap ada di `PRO-PROFILES-4.3.3-ID.md`.
Empat belas tambahan internasional tidak semuanya termasuk kelompok Top 10
historis yang menjadi syarat daftar klasik. Pool baru dapat dinonaktifkan dengan
`aibo_human_international_roster 0` jika ingin tetap memakai hanya 28 nama lama.

Katalog dapat ditinjau/diedit di `source/data/dialogue_catalogue.json`. Setelah
mengubahnya, jalankan `python tools/build_dialogue_catalogue.py` dari source dan
bangun ulang DLL. File C++ hasil generator disertakan, sehingga build biasa
tidak membutuhkan tahap generasi atau Python pada komputer pemain.
