# Optimisations possibles du portage FreeArc C/C++ *(sans altérer la sortie bit-à-bit exacte)*

**Contrainte absolue** : chaque modification doit produire un flux d'octets **strictement identique** à celui produit par l'encodeur FreeArc 0.6x legacy (Haskell `arc` + filtres C), déjà validé par la suite de tests bit-exact (`tests/run_tests.sh`, cross-Wine, comparaison d'archives CRC-à-CRC). Aucun codec ni aucune structure de données de sortie ne change — seules les **implémentations internes** évoluent. Les suites de tests (`make test` et `make test-win64`) servent de garde-fou après **chaque** patch. Idéalement on ajoutera un diff hexadécimal d'archives d'entrée données pour chaque codec activé (`arc a -m5` …), proche de ce que fait `run_tests_win64.sh` avec Wine.

---

## 0 · Recette de sécurité avant tout commit optimisation

1. `make clean && make` puis `make test` **et** `make test-win64`.
2. Test additionnel bas-niveau recommandé avant/après chaque patch :
   ```bash
   arc a -m5 -mt1 /tmp/a.arc tests_files/corpus/*
   cmp /tmp/a.arc build/legacy_reference_same_flags.arc   # archives générées par commit de référence
   ```
3. En cas de doute sur UB non détecté, tourner aussi `make DEBUG=1` + `-fsanitize=address,undefined` sur un petit corpus (les sanitizers ne changent que la durée d'exécution, pas un seul octet de sortie).

---

## 1 · Micro-architecture : compilateur et ABI (impact « gratuit », zéro risque bit-exact)

Le `Makefile` actuel utilise `-O2` + LTO + `gc-sections` pour un excès de compacité. Les gains se situent à **trois niveaux de plus en plus fins** — chaque niveau est purement « compilateur », donc **100 % bit-exact garanti par construction** (les algorithmes ne bougent pas d'un epsilon).

### 1.1 `-O3` + auto-vectorisation ciblée
- `-O3` active l'auto-vectorisation SLP/LP des boucles chaudes des filtres (Delta/rep/BCJ/MM/DICT/LZP).
- Gain typique : **+10 à +30 %** sur les filtres et codecs « stream », ~+5 à +15 % sur LZMA/PPMD (moins de marge, registres déjà bien remplis).
- **Risque bit-exact : nul.** `-O3` ne change pas la sémantique entière (ne **jamais** activer `-ffast-math`; `fno-math-errno` ne modifie pas les résultats).

### 1.2 `-march=native` (et `-mtune`)
- SSE2 est garanti baseline x86-64 ; `AVX2`/`BMI2` apportent +20 à +40 % sur `UpdateCRC` (slice-by-8) et les filtres.
- **Windows/Win64** : `arc.exe` / `unarc.exe` sous MinGW ne doivent PAS utiliser `-march=native` (compatibilité Win7+ / CPUs AMD-Intel hétérogènes).
  Prévoir deux variantes : `make` (baseline x86-64 + LTO) et une cible `make fast` du type `OPT_FLAGS="-march=native"`.
- Sur Windows, une baseline pragmatique est `-msse4.2 -mpclmul` (PCLMULQDQ est omniprésent depuis ~Sandy Bridge / Bulldozer, mais dégrade la compatibilité pré-2011) ; `-mavx2` n'est envisageable que pour une distribution "native".
- **Alternative propre et portable** : connaître le CPU cible au runtime (CPUID) et dispatcher à l'intérieur des seules fonctions SIMD réécrites (CRC32, filtres) — le reste du binaire reste baseline. C'est l'approche zlib-ng / ISA-L.

### 1.3 Profile-Guided Optimization (-fprofile-generate / -fprofile-use)
- Le code est une suite de branches chaudes (codecs et filtres). PGO donne **+5 à 25 %** additionnels sur top de `-O3` (GCC 12/13/14), surtout par meilleure prédiction de branches et inlining ciblé.
- **Impact binaires** : taille +2 à +8 % (que `--gc-sections` en limitera une partie). Aucune conséquence sur le format `.arc` ni la sortie — le code n'est pas stocké dans l'archive.
- **Recette** :
  ```bash
  # Phase 1 : build instrumenté
  # (nécessite d'abord d'ajouter le support CXXFLAGS_EXTRA dans src/Makefile, ou d'éditer MODE_CFLAGS à la main)
  make clean && make CXXFLAGS_EXTRA="-fprofile-generate -fprofile-arcs"
  arc a -m5 -bcj -dict /tmp/train.arc tests_files/corpus/*      # parcours des chemins de compression
  arc x /tmp/train.arc -dp=/tmp/train_out                        # parcours des chemins de décompression
  # Phase 2 : build final profilé
  make clean && make CXXFLAGS_EXTRA="-fprofile-use -fprofile-correction"
  ```
  (les compteurs `__gcov` écrivent des fichiers `.gcda` — phase d'entraînement uniquement, jamais en production finale).

### 1.4 LTO inter-TU (déjà présent) + dévirtualisation
- `-flto=auto` est déjà là, ce qui est bien. Pistes additionnelles : `-fdevirtualize-speculatively` (actif par défaut à -O2 mais renforcé par LTO) et `-fipa-pta` (meilleure analyse pointeurs en LTO ; coûteux en RAM au link, ~+2-5 % mesurables sur les codecs).
- Bénéfice principal : dévirtualiser les dispatchs virtuels de `CompressionLibrary.cpp` (classes `HFilter`/`HDataEncoder` par codec) quand le codec est connu statiquement.

### 1.5 Accès mémoire des macros `value32`/`setvalue32`
- Les codecs font massivement `*(uint32*)(p)` sur des pointeurs potentiellement non alignés (`value32(p) = *(uint32*)(p)` dans Common.h).
- C'est fonctionnellement correct sur x86-64, mais GCC ne "connaît" pas l'alignment : certains patterns peuvent générer des accès moins efficaces et des sous-optimisations.
- **Remplacement propre** : remplaçable par `memcpy(&v, p, 4)` / `memcpy(p, &v, 4)` — GCC 12 les compile en un seul `mov eax,[mem]` non-aligné + élimine le mauvais aliasing. Identique au bit près (même résultat binaire, même ordre d'écriture).
- Les gains appliqués : de l'ordre de 1-3 % sur Tornado MatchFinder, et il débloque l'auto-vectorisation de plusieurs boucles (Delta, MM) qui sont bloquées par les casts.

---

## 2 · Threading : où le portage perd le plus (zéro impact bit-exact)

L'encodeur top-level est séquentiel, mais `SetCompressionThreads()` existe déjà (LZMA / GRZip / Tornado l'utilisent). Trois axes :

### 2.1 LZMA : match finder multithread 7-zip — ne PAS l'activer
- `C_LZMA.cpp` est compilé avec `-DFREEARC_NO_MF_MT` (Makefile) : le **match finder MT de 7-zip** (`LzFindMt.c`) est désactivé.
- **Pourquoi le laisser désactivé** : la sortie LZMA dépend du match finder exact + parseur + coûts internes. Le MF MT de 7-zip peut produire une sortie **légèrement différente** de la version mono-thread (correspondances différentes à certaines positions). Ce risque bit-exact n'est pas maîtrisable → **garder `FREEARC_NO_MF_MT` actif**.
- Le multithreading LZMA reste possible **par bloc** (§2.3), bit-exact par construction.

### 2.2 GRZip : déjà MT par bloc, mais I/O et BWT restent séquentiels
- `C_GRZip.cpp` utilise `MultiThreading.h` (queue de jobs par bloc). OK.
- **Piste** : augmenter la profondeur de pipelining (pré-lecture de `job->InBuf` pendant la compression du bloc précédent) pour masquer les `callback read/write`.
- **Bit-exact : OK** — chaque bloc est compressé indépendamment ; l'ordre d'écriture est préservé par la file de jobs.

### 2.3 Pipeline parallèle **par bloc** dans l'encodeur arc (le plus gros levier)
- L'encodeur `arc.cpp` traite les blocs DATA_BLOCK séquentiellement. Or FreeArc legacy (0.664+) parallélisait par bloc (workers daemon). Le portage actuel est mono-thread à ce niveau.
- **Action** : pipeline producteur/consommateur : 1 thread I/O (lecture fichiers, CRC32 → §2.4), N threads de compression par bloc, 1 thread d'écriture (entrelacement des blocs dans l'archive).
- **Bit-exact : OUI** — chaque bloc a son propre CRC et son propre header ; l'ordre d'écriture dans l'archive est figé par le header initial (Taille des blocs connue à l'avance), l'entrelacement ne déplace aucun octet.
- Gain : quasi-linéaire avec `nproc` pour toute chaîne LZMA/PPMD/Tornado+filtres — le gain le plus massif possible sur ce portage (2×-4× wall-clock sur 4 cœurs).
- **Attention Tornado** : `Tornado.cpp` gère déjà son propre NumThreads entre matchfinder et coder — à désactiver/segmenter proprement dans le produit pipeline (numThreads=1) ; les stats de l'ARICODER sont mono-bloc donc déterministe par bloc.

### 2.4 CRC-32 parallélisable
- Le CRC-32 linéaire est **parallélisable exactement** via GF(2)[x] : `CRC(A‖B) = combine(CRC(A), CRC(B))` avec matrices de combinaison. Catalogue dans zlib RFC 1952-adjacent (PCLMULQDQ `crc32_combine`).
- **Action** : calculer le CRC des tranches indépendantes en parallèle puis combiner : la combinaison exacte valant CRC(A‖B) = CRC(A)·x^(8·len(B)) ⊕ CRC(B) dans GF(2)[x]/P(x) — c'est ce que fait `crc32_combine` de zlib. Les `.arc` stockent un CRC-32 par bloc/fichier ; cela accélère `arc a` pendant la lecture des fichiers, et sert de brique au pipeline §2.3.
  - **Bit-exact : oui, exactement** (math GF(2) exacte du polynôme CRC-32/ISO-HDLC 0xEDB88320).

---

## 3 · Micro-optimisations ciblées par hot-path (garanties bit-exact)

### 3.1 CRC32 (`Environment.cpp`, UpdateCRC)
- État actuel : slice-by-1 (table 256) + un déroulé 4+4 octets (~350-700 MB/s).
- **Slice-by-8** (4 tables de 256, 8 octets/itération) : ~1,2-1,5 GB/s (2-4×), **bit-exact garanti** (même polynôme, même valeur de table).
- **PCLMULQDQ folding** (`_mm_clmulepi64_si128`, polynôme réfléchi 0xEDB88320 — celui du code actuel) : 8-15 GB/s (~10-20×). Référence : papier Intel *Fast CRC Computation with PCLMULQDQ* ; implémentations de référence : zlib-ng, ISA-L.
- **Dispatch runtime** : `__builtin_cpu_supports("pclmulqdq")` → PCLMULQDQ | sinon slice-by-8 | sinon table actuelle. Même résultat quel que soit le chemin : le CRC-32 est une fonction pure.
- Ce CRC (poly 0xEDB88320 réfléchi, init/xorout 0xFFFFFFFF) = CRC-32/ISO-HDLC standard (zlib). La fonction `crc32_combine` de zlib implémente déjà exactement le §2.4.
- Pas d'alignement requis : l'implémentation SIMD gère têtes/queues scalaires (`movdqu`).

### 3.2 Filtres MM/Delta/DICT/REP/BCJ : auto-vectorisation
- `diff1/undiff1/diff2/undiff2/diff4` (`mm.cpp`) : boucles avec `uint16/uint32` purement élémentaire — **vectorisables** par GCC nativement ou via **intrinsics AVX2** (`_mm256_sub_epi16/32`), **bit-exact garanti** : l'arithmétique modulo-wrapped `p[i] - base[i]` = SIMD `epi16/epi32` modulus wrap.
- `Delta/Delta.cpp` (détection de tables binaires) : scan chaud avec hash par distances 2/4/8 ; l'auteur original annotait en tête de fichier « *the speed may be 3-fold increased* ». Auto-vec + peel + `__builtin_expect` sur les branches rares.
- `C_BCJ.cpp` (préprocessing x86 branch/offset) : boucle de détection par octet avec états. La version moderne 7-Zip (Bra86 vectorisé AVX2) montre un portage SIMD sémantiquement identique — réimplémentation possible sans changer la sortie.
- `REP/rep.cpp`, `LZP/C_LZP.cpp`, `memxor` (Environment.cpp) : boucles byte-wise vectorisables.

### 3.3 Tornado MatchFinder (`MatchFinder.cpp`)
- `find_matchlen` compare 4 octets par itération de boucle et avance de 4 : remplacer par `w = *(uint64*)p ^ *(uint64*)q` → `__builtin_ctzll(w)` pour obtenir la longueur de correspondance **sans boucle** (pattern standard zstd/xz/Snappy). **Identique par construction** : `find_matchlen` retourne le même `len` pour les mêmes pointeurs → les matchs encodés restent les mêmes → sortie bit-exact. Gain observé dans des projets comparables : +30 à +60 % sur l'étape match-finder.
- `hashx` : multiplication constante — peu de marge, mais garde `HashShift`/`HashMask` invariants déjà hoistés.
- `update_hash1` (row shift) pour les méthodes 7-11 : décale `hash_row_width` entrées scalaires unes à unes — remplacer par un `memmove` de la rangée contiguë ou des copies SIMD. La HTable est interne au codec : le résultat dépend uniquement du contenu final, qui reste identique.
- `clear_hash()` / `shift()` : boucles scalaires sur `HashSize` entrées (jusqu'à ~8M) — vectoriser. Le `shift()` notamment re-scanne TOUTE la table à chaque slide (plusieurs fois par MB sur les méthodes 9-11). Alternative structurelle : stocker des offsets relatifs à une base logique dans la HTable, de sorte que le slide devienne un simple incrément de la base sans réécrire la table. Bit-exact garanti si la liste des correspondances retournées reste identique — c'est le cas.
- Gain attendu : significatif sur les méthodes 7+ (buffer ≥ 256 MB, slides fréquents). À mesurer.

### 3.4 LZMA (SDK 7-zip legacy) — codec par défaut (`-m5`)
- `LzmaEnc.c` : gain principal via -O3 + PGO (~20-30 % typiques), sur les hot loops du range coder (`RangeEnc::EncodeDirectBits`, `ShiftLow`) et la recherche de littéraux.
- On n'active **pas** le match finder MT (§2.1) : LZMA reste mono-thread, c'est le pipeline par bloc (§2.3) qui le parallélise au niveau de l'encodeur arc.
- Le range coder du SDK est déjà très optimisé (ShiftLow) : rien à gagner sans changer le flux de bits.

### 3.5 PPMD : SubAlloc + Model
- `SubAlloc.hpp` : sous-allocateur PPMd Variant H déjà très serré. `PrefetchData()` est no-op (dépend de `_USE_PREFETCHING`) : essayer `__builtin_prefetch` sur `pText` (+0 à +5 % selon CPU, timing uniquement).
- Hugepages : `madvise(MADV_HUGEPAGE)` sur le bloc du sous-allocateur (>128 MB) peut donner +3-8 % (moins de TLB misses).
- `Model.cpp` : tables statiques BinSumm/SEE2 — gains via -O3/PGO seulement. **Aucune** modification structurelle possible sans changer la sortie (le modèle PPMd est un flux d'arithmetic coder strictement séquentiel où le suballocator détermine la structure).
- **Threading** : PPMD est intrinsèquement séquentiel. Seul le pipeline par bloc (§2.3) est exploitable.

### 3.6 GRZip : BWT + MTF arith
- `BWT.c` (FastBWT / StrongBWT ternary sort) : remplacer par **libdivsufsort** (BSD) donnerait ~1,5-2× sur les gros blocs. **Caveat bit-exact** : la dernière colonne BWT est unique pour un texte donné — tout algo de suffix array correct produit le même SA → même colonne BWT → bit-exact en théorie. **Mais** il faut vérifier ce que fait le fallback `GRZ_FAST_BWT_FAILS` du code actuel (comportement identique ?). À valider par `cmp` sur le corpus complet — effort modéré, priorité moyenne.
- `MTF_Ari.c` / `WFC_Ari.c` : scanning MTF linéaire (`while (MTF_List[rank]!=Char) rank++`, liste de 256, ~64 itérations moyennes en pratique) — remplacer par une **rank table** maintenue incrémentalement (`rank_of_char[256]`, update O(1) par mouvement). Bit-exact : le rank produit est identique, seul le chemin de recherche change → gain 2-4× sur la phase MTF.
- `WFC_Ari.c` hot loop `while (Frq>=Cum)` : recherche linéaire sur cumuls — un index de cumul pré-calculé permet une lecture O(1) par symbol (l'update reste O(256) par symbole). Équivalent au bit près : les probabilités cumulées restent les mêmes.

### 3.7 TTA (MM/tta.cpp)
- TTA (True Audio) : hot loop = prédicteur à 4 coefficients fixes par canal + filtres adaptatifs, arithmétique 32-bit avec wraparound. Vectorisable si les wraps entiers sont reproduits exactement — l'intrinsèque AVX2 `_mm256_add_epi32` garantit le wraparound identique au C. Gain typique 1,5-2× sur la phase du prédicteur. Priorité moyenne (TTA peu répandu).

### 3.8 Chiffrement (LibTomCrypt 2006)
- AES logiciel (`aes.c` de LibTomCrypt, sans hurler, version antérieure à l'AES-NI dans les codecs) : une implémentation AES-NI (`_mm_aesenc_si128`, 10/12/14 rounds) est 10-20× plus rapide que l'AES logiciel (10 GB/s vs ~0,5 GB/s). Bit-exact : oui — AES est FIPS-197 ; AES-NI implémente exactement la même spécification.
- CTR : attention au schéma LTC : incrémentation little-endian sur les `ctrlen` premiers octets + un `pad` pour les tailles non alignées (cf. `ctr_encrypt.c`). L'implémentation AES-NI doit reproduire exactement ce compteur/pad.
- CFB : séquentiel par nature (résultat = PT[i] XOR churn de IV) — non vectorisable, mais AES-NI s'applique (1 block AES par 16 octets de flux).
- Blowfish/Twofish/Serpent : Serpent bitsliced AVX2 = +4-8× mais effort élevé pour un usage rare, priorité basse.
- Structure recommandée : fast-path dispatch CPUID + fallback logiciel (structure bit-exact garantie).

### 3.9 I/O et paging
- `arc.cpp` : passer les buffers de lecture/écriture à 1-4 MB et utiliser `posix_fadvise(POSIX_FADV_SEQUENTIAL)` (éventuellement `O_DIRECT` en option).
- `BigAlloc`/`BigFree` : `mmap` + `madvise(MADV_HUGEPAGE)` au-dessus d'un seuil (sub-allocateur PPMD, dictionnaire LZMA, buffers Tornado ≥ 256 MB).
  - **Impact bit-exact : nul** (uniquement disposition mémoire / TLB).
- **I/O asynchrone / back-pressure** : couplé au pipeline §2.3.

---

## 4 · Tableau de priorisation (impact × effort)

| # | Optimisation | Gain attendu | Effort | Bit-exact | Priorité |
|---|--------------|--------------|--------|-----------|----------|
| 1 | Pipeline multithread par blocs dans `arc a` (§2.3) | **2-4× wall-clock** sur N cœurs | Moyen | ✅ par construction | ★★★★★ |
| 2 | CRC32 slice-by-8 / PCLMULQDQ + `crc32_combine` | 3-10× sur CRC, débloque le pipeline | Faible | ✅ | ★★★★★ |
| 3 | -O3 + PGO + `-march=native` (cible Linux) | +15-40 % global | Très faible | ✅ | ★★★★ |
| 4 | MatchFinder Tornado : comparaison XOR-64 + `ctz` | +30-60 % sur Tornado 5-11 | Faible | ✅ même résultat | ★★★★ |
| 5 | AES-NI pour CTR/CFB (AES-128/256) | 10-20× chiffrement | Moyen | ✅ (FIPS-197) | ★★★★ |
| 6 | MM / Delta SIMD (AVX2 intrinsics ou -O3 auto-vec) | 2-4× sur filtres | Faible | ✅ (arithm. modulo) | ★★★ |
| 7 | BCJ vectorisé (style 7-Zip 21+) | 2-4× sur BCJ | Moyen | ✅ | ★★★ |
| 8 | GRZip BWT → libdivsufsort | 1,5-2× sur GRZip encode | Moyen | ✅ (SA unique, à valider par cmp) | ★★★ |
| 9 | MTF rank-table dans GRZip MTF_Ari / WFC_Ari | 2-4× sur phase MTF | Faible | ✅ | ★★☆ |
| 10 | Hugepages / madvise pour allocs >128 MB | +3-8 % sur PPMD/LZMA | Faible | ✅ | ★★ |
| 11 | I/O buffers 1-4 MB + fadvise | +5-15 % sur fichiers petits | Faible | ✅ | ★★ |
| 12 | TTA SIMD | 1,5-2× | Moyen | ✅ à valider par test | ★☆ |
| 13 | Serpent bitsliced AVX2 / Twofish | +4-8× | Élevé | ✅ | ★ |

**⛔ Ce qu'il ne faut pas toucher** (casserait le bit-exact) :
- Le parseur de matchs Tornado (greedy/lazy/optimal) et les tables `std_Tornado_method[]` — modifier le flux de matchs changerait la sortie.
- `FREEARC_NO_MF_MT` pour LZMA : réactiver le match finder MT 7-zip peut produire une sortie différente. Garder désactivé.
- Les paramètres `PackMethod` de Tornado et les defaults (`default_Tornado_method=5`), PPMD `mem`/`o`/`memui`. Modifier ces paramètres changerait le résultat d'encodage. (Sont des entrées de config, pas des optimisations internes.)
- Le RangeCoder 7-zip et l'arith coder semi-adaptatif de Tornado (les statistiques sont accumulées sur tout le flux → ordre de traitement strict à préserver).
- L'ordre strict d'écriture des blocs/headers/dir/footer et les CRC : tout changement de contenu d'un bloc change son CRC → sortie différente.
- TTA : le prédicteur (ordre-2 intra-canal + filtres adaptatifs) et sa quantization — le SIMD doit reproduire exactement les wrappings arithmétiques 16/32-bit.
- PPMD : le modèle (fallback ordre-1, stats SEE), et le suballocateur qui détermine la structure du modèle — tout changement d'allocation = un flux de arithmetic coder différent = **sortie différente**.

## 5 · Notes finales

Le portage actuel est déjà bien armé (LTO, -O2, gc-sections). Chaque patch se fait au cas par cas, avec un benchmark avant/après (hyperfine ou `time` multi-itérations) et **jamais** avant la validation de la recette §0 (make test + make test-win64). Vous pouvez démarrer par les 2-3 premières lignes du tableau pour un gain maximum à effort minimal.

---

*Document écrit à partir de l'analyse de `src/arc.cpp`, `src/Environment.cpp`, `src/Compression/{Tornado,GRZip,PPMD,MM,Delta,REP,LZP,_Encryption,LZMA2}/...` et valable pour le build `make` (production, Linux x86_64).*
