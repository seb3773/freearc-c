# Benchmark Comparatif Global : Corpus 10 Go (79 431 fichiers)
- **Date** : 2026-11-24 / 2026-11-25 (`v1.4.5`)
- **Taille utile des 79 431 fichiers réguliers** : 10,000,000,000 octets (9.313 GiB / 9536.7 MiB) (`10,017,494,016` octets via `du -sb` incluant les inœuds des répertoires)
- **Machine** : 11th Gen Intel(R) Core(TM) i3-1115G4 @ 3.00GHz (2 cœurs / 4 threads, AVX2/AVX-512, SSD NVMe)

## Résultats Comparatifs Détaillés

| Compresseur | Profil / Méthode | Taille Archive (o) | Taille (GiB) | Ratio (%) (sur 10 Go) | Tps Encodage | Débit Encodage | Tps Décodage | Débit Décodage |
|---|---|---|---|---|---|---|---|---|
| **FreeArc-c (Bit-Exact)** | `-mx (Standard)` | 3,961,852,890 o | 3.690 GiB | 39.62% | 1333.2 s (22.2 m) | 7.15 MiB/s | 63.9 s (1.1 m) | 149.2 MiB/s |
| **Zstandard 1.5.4** | `--ultra -22 -T4` | 3,816,208,568 o | 3.554 GiB | 38.16% | 7102.8 s (118.4 m) | 1.34 MiB/s | 27.7 s (0.5 m) | 344.3 MiB/s |
| **7-Zip 26.02** | `-mx=9 -ms=on` | 3,652,676,518 o | 3.402 GiB | 36.53% | 4931.5 s (82.2 m) | 1.93 MiB/s | 76.3 s (1.3 m) | 125.0 MiB/s |
| **FreeArc-c (`v1.4.0` Nobitexact)** | `-nobitexact -mx` | 3,602,280,111 o | 3.355 GiB | 36.02% | 1416.4 s (23.6 m) | 6.73 MiB/s | 889.7 s (14.8 m) | 10.7 MiB/s |
| **FreeArc-c (`v1.4.5` Nobitexact)** | **`-nobitexact -mx`** | **3,352,618,009 o** | **3.122 GiB** | **33.53%** | **1007.7 s (16.8 m)** | **9.46 MiB/s** | **353.3 s (5.9 m)** | **27.00 MiB/s** |

## Analyses & Points Clés (`v1.4.5`)
- **FreeArc-c `v1.4.5 -nobitexact` vs `Bit-Exact`** :
  * Gain en compression : **−609,234,881 octets** (**−581.01 MiB**, **−15.38%**)
  * Vitesse d'encodage : **1.32× plus rapide** (1007.7 s vs 1333.2 s)
- **FreeArc-c `v1.4.5 -nobitexact` vs `v1.4.0 -nobitexact`** :
  * Gain en compression : **−249,662,102 octets** (**−238.10 MiB**, **−6.93%**)
  * Vitesse d'encodage : **~28.9% plus rapide** (1007.7 s vs 1416.4 s)
  * Vitesse de décompression : **2.52× plus rapide** (353.3 s vs 889.7 s)
  * Compatibilité legacy `unarc.exe 0.67` (Wine `t`) : **All OK** (`rc=0`, `448.9 s`)
- **FreeArc-c `v1.4.5 -nobitexact` vs 7-Zip Ultra (`-mx=9`)** :
  * Taille : FreeArc-c est **300,058,509 octets (286.16 MiB, 8.21%) plus compact** que 7-Zip
  * Vitesse d'encodage : FreeArc-c est **4.89× plus rapide** que 7-Zip Ultra (1007.7 s vs 4931.5 s)
- **FreeArc-c `v1.4.5 -nobitexact` vs Zstandard (`--ultra -22 -T4`)** :
  * Ratio : FreeArc-c bat Zstandard Ultra de **463,590,559 octets** (**442.11 MiB**, **−12.15%**)
  * Vitesse d'encodage : FreeArc-c est **7.05× plus rapide** (1007.7 s vs 7102.8 s) ; décodage Zstandard plus rapide (27.7 s vs 353.3 s)