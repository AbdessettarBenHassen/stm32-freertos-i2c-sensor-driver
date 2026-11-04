# Guide d'implémentation (FR)

Ce guide t'accompagne pas à pas pour terminer le driver BME280. Toute la structure
(CMake, CI, API, tests, faux bus I2C, exemples) est déjà prête. Il te reste le coeur
du driver, marqué dans le code par `TODO(Abdessettar)`.

Règle d'or : **une étape = des tests qui passent = un commit**. Ne passe pas à l'étape
suivante tant que les tests de l'étape en cours ne sont pas verts.

Document de référence : *BME280 datasheet* de Bosch Sensortec (BST-BME280-DS002).
Les numéros de sections ci-dessous renvoient à ce document.

---

## Étape 0 : préparer l'environnement

1. Installe les outils : `cmake` (>= 3.18), `gcc` ou `clang`, `ninja` (ou `make`),
   `clang-format` 18 (`pip install clang-format==18.1.8`), `cppcheck`.
2. Compile et lance les tests une première fois :

   ```bash
   cmake -S . -B build -DBME280_WERROR=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

3. C'est **normal** que les tests échouent : les fonctions du coeur renvoient
   `BME280_E_NOT_IMPLEMENTED` ou `0`. Le build, lui, doit réussir sans aucun warning.
   Au départ, 15 tests Unity sur 55 passent (ceux qui ne dépendent pas de ton code).
4. Pour ne lancer qu'un seul fichier de tests :

   ```bash
   ctest --test-dir build -R test_bme280_regs --output-on-failure
   # ou directement l'exécutable, plus lisible :
   ./build/tests/test_bme280_regs
   ```

5. Lis `include/bme280.h` en entier : chaque fonction y est documentée (Doxygen),
   avec les unités et les codes d'erreur attendus.

Astuce : ouvre `tests/support/fake_i2c.c`. C'est le "faux capteur" utilisé par les tests.
Il simule 256 registres, compte les appels, et peut injecter des erreurs de bus.

---

## Étape 1 : lecture et écriture de registres

**Fichier :** `src/bme280.c`, fonctions `bme280_read_regs()` et `bme280_write_reg()`
**Tests :** `tests/test_bme280_regs.c` (9 tests)
**Datasheet :** section 6.2 (interface I2C), figures 9 et 10 (lecture et écriture I2C)

Ce que tu dois faire :

- Vérifier les pointeurs : `dev`, `buf`, `dev->hal` et le callback utilisé. Si un pointeur
  est NULL, renvoyer `BME280_E_NULL_PTR`.
- Pour la lecture, refuser `len == 0` avec `BME280_E_INVALID_ARG`.
- Appeler le callback de la HAL en lui passant `dev->hal->ctx` en premier argument,
  puis `dev->addr`, le registre, le buffer et la longueur.
- Le callback renvoie `0` si tout va bien. Toute autre valeur doit devenir `BME280_E_COMM`.

Indices :

- La lecture "burst" (plusieurs octets d'affilée) est gérée par le capteur : l'adresse de
  registre s'incrémente toute seule. Un seul appel au callback suffit, quel que soit `len`.
- Pour l'écriture, le callback attend un pointeur vers des données : un simple `uint8_t`
  local et son adresse suffisent.

Vérification :

```bash
cmake --build build && ./build/tests/test_bme280_regs
```

Comme `bme280_init()` utilise ces deux fonctions, plusieurs tests de
`test_bme280_init` devraient aussi passer maintenant (sauf ceux qui touchent la calibration).

Commit :

```bash
git add src/bme280.c
git commit -m "feat(driver): implement register read and write helpers"
```

---

## Étape 2 : décodage de la calibration

**Fichier :** `src/bme280.c`, fonction `bme280_parse_calibration()`
**Tests :** `tests/test_bme280_calib.c` (6 tests), puis `tests/test_bme280_init.c`
**Datasheet :** section 4.2.2, tableau 16 "Compensation parameter storage, naming and data type"

Chaque capteur est calibré en usine. Les coefficients sont stockés dans deux zones :

| Zone | Registres | Tableau en entrée | Contenu |
|---|---|---|---|
| 1 | 0x88 à 0xA1 (26 octets) | `tp[]` | dig_T1..dig_T3, dig_P1..dig_P9, dig_H1 |
| 2 | 0xE1 à 0xE7 (7 octets) | `h[]` | dig_H2..dig_H6 |

L'indice dans le tableau est `registre - adresse de début` (par exemple `tp[0x8E - 0x88]`).

Indices :

- Les mots de 16 bits sont en **little endian** : l'octet de poids faible vient en premier.
  Construis la valeur avec un décalage et un OU, sans `memcpy` (le résultat dépendrait
  de l'endianness du CPU).
- Attention au **signe** : regarde la colonne "data type" du tableau 16. `dig_T1` et
  `dig_P1` sont `unsigned short`, les autres `signed short`. Le cast vers le bon type
  doit se faire sur la valeur 16 bits complète.
- `dig_H1` est seul, à l'adresse 0xA1 (dernier octet de `tp[]`).
- Le piège classique : **dig_H4 et dig_H5** font 12 bits et se partagent le registre 0xE5.
  - dig_H4 = 0xE4 (bits 11..4) et 0xE5 bits 3..0
  - dig_H5 = 0xE6 (bits 11..4) et 0xE5 bits 7..4
  - Ce sont des valeurs **signées** : le test `test_parse_humidity_nibbles_negative`
    vérifie que -1 et -2033 sont bien décodés. Pense à traiter l'octet de poids fort
    comme un `int8_t` avant de le décaler.
- `dig_H6` est un `signed char`.

Vérification :

```bash
cmake --build build && ./build/tests/test_bme280_calib && ./build/tests/test_bme280_init
```

À ce stade, `test_bme280_init` doit être entièrement vert.

Commit :

```bash
git commit -am "feat(driver): parse factory calibration data"
```

---

## Étape 3 : compensation de la température

**Fichier :** `src/bme280.c`, fonction `bme280_compensate_temperature()`
**Tests :** `tests/test_bme280_compensation.c` (tests `test_temperature_*`)
**Datasheet :** section 4.2.3 "Compensation formulas", fonction `BME280_compensate_T_int32`
(et l'annexe 8.2 pour la version 32 bits détaillée)

Le capteur donne une valeur brute `adc_T` sur 20 bits. La formule Bosch la transforme
en centièmes de degré (`2508` = 25,08 °C) et produit au passage `t_fine`, une
"température fine" réutilisée par les formules de pression et d'humidité.

Indices :

- Recopie la logique de la datasheet, mais avec les noms de champs de `bme280_calib_t`.
- Les décalages `>>` sur des nombres négatifs sont utilisés par Bosch. Garde des
  `int32_t` partout et caste explicitement `dig_T1` (non signé) en `int32_t`.
- Avec `-Wconversion` activé, le compilateur peut se plaindre : ajoute des casts
  explicites plutôt que de désactiver le warning.
- Si `t_fine` est NULL, calcule quand même la température (le test
  `test_temperature_accepts_null_t_fine` le vérifie).

Valeurs de contrôle (exemple de la datasheet) : avec dig_T1 = 27504, dig_T2 = 26435,
dig_T3 = -1000 et adc_T = 519888, tu dois obtenir `t_fine = 128422` et `T = 2508`.

Vérification :

```bash
cmake --build build && ./build/tests/test_bme280_compensation
```

Commit :

```bash
git commit -am "feat(driver): add integer temperature compensation"
```

---

## Étape 4 : compensation de la pression

**Fichier :** `src/bme280.c`, fonction `bme280_compensate_pressure()`
**Tests :** `test_pressure_*` dans `tests/test_bme280_compensation.c`
**Datasheet :** section 4.2.3, fonction `BME280_compensate_P_int64`

Indices :

- Tous les calculs intermédiaires en `int64_t`. Sur 32 bits, ça déborde.
- La formule contient une **division** par `var1`. Si `var1 == 0`, renvoie `0`
  (test `test_pressure_zero_divisor_returns_zero`, avec dig_P1 = 0).
- Le résultat est en format **Q24.8** : 24 bits entiers, 8 bits de fraction.
  Divise par 256 pour obtenir des pascals. Exemple : 25767236 / 256 = 100653 Pa,
  soit 1006,53 hPa.
- Le test accepte un écart de quelques unités sur la valeur Q24.8 : l'exemple de la
  datasheet a été calculé en flottant, la formule entière donne une valeur très proche.

Commit :

```bash
git commit -am "feat(driver): add 64-bit integer pressure compensation"
```

---

## Étape 5 : compensation de l'humidité

**Fichier :** `src/bme280.c`, fonction `bme280_compensate_humidity()`
**Tests :** `test_humidity_*` dans `tests/test_bme280_compensation.c`
**Datasheet :** section 4.2.3, fonction `bme280_compensate_H_int32`

Indices :

- C'est la formule la plus longue : écris-la par petits morceaux avec des variables
  intermédiaires nommées, puis compare chaque ligne avec la datasheet.
- La datasheet borne la valeur intermédiaire entre `0` et `419430400` avant le dernier
  `>> 12`. Sans ce bornage, les tests `test_humidity_clamped_*` échouent.
- Le résultat est en format **Q22.10** : `102400` correspond à 100 %RH. Divise par 1024
  pour obtenir le pourcentage.

Quand les trois compensations sont faites, tout le coeur est terminé :

```bash
cmake --build build && ctest --test-dir build -R "regs|init|calib|compensation|data" --output-on-failure
./build/examples/host_simulated/bme280_host_demo
```

La démo doit afficher `T = 25.08 degC`, `P = 1006.53 hPa` et `H = 54.997 %RH`.

Commit :

```bash
git commit -am "feat(driver): add integer humidity compensation"
```

---

## Étape 6 : port STM32 (Cube HAL)

**Fichier :** `port/stm32/bme280_stm32.c`
**Tests :** `tests/test_bme280_stm32_port.c` (7 tests, exécutés sur PC grâce à un faux
Cube HAL dans `tests/support/stm32_hal_stub/`)
**Doc ST :** UM1884 (HAL STM32L4), fonctions `HAL_I2C_Mem_Read`, `HAL_I2C_Mem_Write`, `HAL_Delay`

Indices :

- Le contexte `ctx` est un `bme280_stm32_ctx_t *` : il contient le handle I2C
  (`hi2c1` généré par CubeMX) et un timeout en millisecondes.
- Le Cube HAL attend l'adresse sur **8 bits** : `0x76` devient `0xEC`. Décale de 1 vers
  la gauche (test `test_read_uses_shifted_address_and_8bit_register`).
- Taille d'adresse de registre : `I2C_MEMADD_SIZE_8BIT`.
- `HAL_I2C_Mem_Write()` prend un `uint8_t *` non const : un cast est nécessaire.
- Renvoie `0` si la HAL renvoie `HAL_OK`, sinon une valeur non nulle.
- Pour le délai : `HAL_Delay()` en bare metal. Si `BME280_STM32_USE_FREERTOS` est défini
  et que l'ordonnanceur tourne, préfère `vTaskDelay()` pour laisser la main aux autres tâches.

Commit :

```bash
git commit -am "feat(port): implement STM32 HAL I2C binding"
```

---

## Étape 7 : tâche FreeRTOS

**Fichier :** `port/freertos/bme280_task.c`, fonction `bme280_task()`
**Test :** `tests/freertos_sim/test_bme280_task_sim.c`, qui fait tourner ta tâche sur le
simulateur FreeRTOS POSIX (Linux)
**Doc :** *Mastering the FreeRTOS Real Time Kernel*, chapitres "Task Management" et
"Queue Management"

Le commentaire TODO décrit l'algorithme. Points importants :

- Utilise `vTaskDelayUntil()` et non `vTaskDelay()` : la période reste fixe même si la
  lecture I2C prend un peu de temps (pas de dérive).
- `xQueueSend(..., 0)` : le producteur ne doit jamais bloquer. Si la file est pleine,
  incrémente `cfg->dropped`.
- En mode `BME280_MODE_FORCED`, il faut relancer une mesure à chaque période puis attendre
  `bme280_measurement_time_ms()` avant de lire (datasheet section 3.3.3 et 9.1).
- Une tâche FreeRTOS ne doit **jamais** retourner.

Vérification (télécharge le noyau FreeRTOS V11.1.0 au premier configure) :

```bash
cmake -S . -B build-sim -DBME280_FREERTOS_SIM=ON
cmake --build build-sim
ctest --test-dir build-sim -L freertos --output-on-failure
```

Résultat attendu : `PASS: 3 samples received and checked`.

Commit :

```bash
git commit -am "feat(rtos): implement periodic sampling task"
```

---

## Étape 8 : test sur la carte NUCLEO-L476RG

Suis `examples/nucleo_l476rg/README.md` :

1. Câblage (voir le tableau du README principal) : SCL sur PB8 (D15), SDA sur PB9 (D14),
   SDO à GND pour l'adresse 0x76.
2. Projet CubeMX : I2C1 à 400 kHz, USART2, FreeRTOS, base de temps HAL sur TIM6.
3. Ajoute les sources du driver, `app_main.c`, et la define `BME280_STM32_USE_FREERTOS=1`.
4. Ouvre un terminal série à 115200 bauds : une ligne par seconde avec T, P et H.

Si `bme280_init()` renvoie `BME280_E_COMM` : vérifie le câblage, les pull-ups et
l'adresse (0x76 ou 0x77). Si elle renvoie `BME280_E_CHIP_ID` : tu as peut-être un BMP280
(chip id 0x58, pas d'humidité).

Bonus pour le portfolio : une capture d'analyseur logique de la lecture du chip id,
ajoutée dans `docs/`, avec une phrase d'explication dans le README.

Commit :

```bash
git commit -am "docs: add NUCLEO-L476RG bring-up notes"
```

---

## Conventions de commit

Format [Conventional Commits](https://www.conventionalcommits.org/fr/) : `type(scope): message`
en anglais, à l'impératif, sans point final.

| Type | Usage | Exemple |
|---|---|---|
| `feat` | nouvelle fonctionnalité | `feat(driver): read chip id` |
| `fix` | correction de bug | `fix(driver): sign-extend dig_H5` |
| `test` | ajout ou modification de tests | `test(driver): cover pressure overflow` |
| `refactor` | changement sans effet visible | `refactor(port): extract address helper` |
| `docs` | documentation | `docs: add wiring photo` |
| `build` / `ci` | CMake, workflow GitHub | `ci: cache FetchContent downloads` |
| `chore` | maintenance | `chore: bump Unity to v2.6.1` |

Scopes conseillés : `driver`, `port`, `rtos`, `tests`, `examples`.

---

## Checklist finale avant de publier

- [ ] Tous les tests passent : `ctest --test-dir build --output-on-failure`
- [ ] Les tests passent aussi avec les sanitizers : `-DBME280_SANITIZE=ON`
- [ ] Le test FreeRTOS passe : `-DBME280_FREERTOS_SIM=ON`
- [ ] Aucun warning avec `-DBME280_WERROR=ON`, avec GCC **et** Clang
- [ ] Code formaté : `git ls-files '*.c' '*.h' | xargs clang-format --dry-run --Werror`
      (pour corriger : remplace `--dry-run --Werror` par `-i`)
- [ ] cppcheck propre (commande dans le README)
- [ ] Plus aucun `TODO(Abdessettar)` : `grep -rn "TODO(Abdessettar)" src port`
- [ ] README mis à jour : roadmap cochée, photo ou capture de la carte si possible
- [ ] Version passée de `0.1.0-dev` à `0.1.0` dans `include/bme280.h` (`BME280_DRIVER_VERSION`)
- [ ] Dépôt créé sur GitHub, poussé, badge CI vert
- [ ] Tag de version :

  ```bash
  git tag -a v0.1.0 -m "v0.1.0: first working BME280 driver"
  git push origin main --tags
  ```

Bon courage !
