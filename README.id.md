# AiBo 4.3.5 (Upgrade Level 5 Apex: Self-Modifying Neuroevolution & Hebbian Plasticity ~99,9% Otonom — Instalasi Single-DLL dengan Penyimpanan Kurikulum Mandiri yang Terus Berkembang di Hard Disk)

**Bot Counter-Strike 1.6 berbasis C++20 dengan Self-Modifying ABCD Hebbian Plasticity, Structural Synaptic Neurogenesis & Pruning, Liquid Time-Constant (LTC) Recurrent Working Memory, Population Neuroevolution DNA Pool, Latent World Model (3-Step Mental Rollout), Evolving Skill Library (hingga 512 skill/map), Keseimbangan 10 Pemain (5v5), serta Instalasi 1 File `AiBo.dll` yang Otomatis Membuat Folder & File Pengetahuan Mandiri yang Terus Bertumbuh (`.MB`) di Hard Disk.**

**Pembuat dan pengelola proyek dasar AiBo: K9_einzwei** (GitHub: [keinzwei](https://github.com/keinzwei)).

[English](README.md) · [Unduh installer](https://github.com/keinzwei/AiBo/releases/tag/v4.3.5) · [Panduan build](docs/human-copycat/BUILD.md) · [Kredit](CREDITS.md)

## Pembaruan Level 5 (Theoretical Apex ~99,9% Otonom & Self-Bootstrapping Single-DLL)

- **Instalasi Cukup 1 File `AiBo.dll` + Pembuatan Folder/File Kurikulum Otomatis di Hard Disk (`aibo_single_dll_mode 1`):**
  - Saat pertama kali dipasang, pengguna cukup mengekstrak **1 file `AiBo.dll`** (atau `aibo.so` di Linux) dan mendaftarkannya pada `addons/metamod/plugins.ini` tanpa perlu menyertakan file `.cfg` atau `.graph` apa pun.
  - Saat map dimulai untuk pertama kalinya, `AiBo.dll` langsung memetakan titik *spawn*, target bom/sandera, dan koridor map lewat `BootstrapInMemoryGraph()` + `GraphAnalyze`, lalu **secara otomatis membuat folder di hard disk pengguna** (`cstrike/addons/AiBo/data/graph/`, `data/train/`, dan `data/copycat/`) serta menyimpan dan terus menumbuhkan ukuran file pengetahuan mandirinya (`.MB`) seiring berjalannya ronde demi ronde:
    - `<map>.graph`, `<map>.vis`, `<map>.prc`, `<map>.pmx`: graf 3D map, tabel visibilitas, pengalaman bahaya/rute, dan matriks jalur.
    - `<map>-<env>.hlearn` & `<map>-<env>.hlearn2` (`HCLRNML5`): menyimpan jaringan *TinyMLP*, *HRL Option-Critic*, *Latent World Model* 16-D, *RND Curiosity*, **serta seluruh sinapsis plastis Hebbian ABCD, peta struktur sinapsis aktif (`activeMask`), dan memori kerja *Liquid Time-Constant***.
    - `<map>-<env>.hfinal`, `<map>-<env>.hfinal2`, & `<map>-<env>.hevo` (`HCEVOBR5`): menyimpan *spatial heatmap*, memori tempat/strategi, **serta kurikulum mandiri tiap bot dan populasi 16 DNA *NeuroevolutionPool*** lintas sesi.
    - `<map>-<env>.hskills` (`HCSKILL4`, kapasitas hingga 512 manuver/map), `<map>-<env>.rbcc` (kapasitas hingga 2.048 klip / 262.144 frame), `<map>-<env>.hexplore` (hingga 16.384 node GAT 3D), `<map>-<env>.hnav`, dan `<map>-<env>.haim`.
- **Self-Modifying ABCD Hebbian Plasticity & Structural Neuroevolution (`HebbianPlasticCortex` di `copycat_learning.*`):**
  - Setiap sinapsis $(i, j)$ memodifikasi bobotnya sendiri pada setiap *frame* menggunakan aturan plastisitas lokal $\Delta H_{ij} = \eta_i \cdot m(t) \cdot (A_i x_j y_i + B_i x_j + C_i y_i + D_i)$ serta melakukan **pemangkasan sinapsis pasif (`prunedSynapses`) dan penumbuhan sinapsis baru (`sproutedSynapses`)** secara otomatis.
- **Liquid Time-Constant (LTC) Recurrent Working Memory (`LiquidRecurrentCore` di `copycat_learning.*`):**
  - Memori kerja waktu-kontinu ($\tau_i(x_t)$ adaptif) yang mempertahankan jejak kecepatan, posisi, dan ritme lawan meskipun lawan bersembunyi di balik tembok atau *smoke*.
- **Inter-Bot Population Neuroevolution (`NeuroevolutionPool` di `copycat_final.*`):**
  - Ke-10 bot mewariskan DNA koefisien Hebbian $(A, B, C, D, \eta)$ dan konstanta waktu *Liquid* terbaik di setiap akhir ronde melalui *elitism*, persilangan (*crossover*), dan mutasi, lalu menyimpannya ke file `.hevo` di hard disk.
- **Keseimbangan 10 Pemain 5v5 (`aibo_quota 21`):**
  - Saat **0 pemain manusia** di server: otomatis mengisi **21 Bot (Internet Mode)** (5 CT vs 5 T).
  - Saat **1 pemain manusia** masuk ke server: otomatis **meng-kick 1 Bot** sehingga jumlah pemain tetap **10 pemain** (**9 Bot + 1 Manusia = 5 CT vs 5 T seimbang**).

## Pemeriksaan Console Level 5

- `aibo_learning_status`: menampilkan `level5=1`, `active_synapses`, `pruned`, `sprouted`, `rewires`, `hebbian_steps`, `liquid_ticks`, `evo_gen`, `world_rollouts`, `rnd_curiosity`, dan `skills`.
- `aibo_final_status`: menampilkan `autonomy=99.9%`, `dna_gen`, `fit`, `evo_gen`, `crossovers`, `mutations`, serta keseimbangan pemain (`bots`, `humans`, `target_bots`).
- `aibo_human_save`: memaksa penyimpanan seluruh pengetahuan yang sedang berkembang ke hard disk kapan saja (selain *autosave* otomatis setiap ronde/interval).
