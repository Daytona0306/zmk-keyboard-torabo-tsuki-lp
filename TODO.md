# 残件

torabo-tsuki 側の未着手・未確認事項。片付いたら消すこと。

**畳んだもの**（調査結果は README に移してあります）

| 元項目 | 顛末 |
|---|---|
| Ctrl+Alt+Tab コンボ | 動いているので調査打ち切り。DYA 側の `Timeout ms` は既定のままだと確認でき、残っていた仮説も消えた。原因は未解明のまま |
| DYA Studio の USB 接続不良 | 切り分け打ち切り。あわせて `CONFIG_ZMK_OS_DETECTION_USB` を `=y` に戻した（このシンボルは原因ではない） |
| 慣性スクロール | 現状の値で確定。調査結果は README「スクロールの調整はどこで効くか」へ |
| スクロール倍率の控え | 値は記録しない。「Studio で変えた値はリポジトリに残らない」ことだけ README に記載 |
| west.yml のリビジョン固定 | 31 プロジェクトすべて SHA 固定。`update-modules` ブランチと `tools/west-pins.py` を用意。運用は README「ブランチ運用」へ |

---

## 1. 拡張モジュール — 準備は済み、残るは現物と実機検証

torabo-tsuki LP の拡張モジュールは3種類。のぎけす屋 (BOOTH) で単体販売。

| パーツ | 価格 | ドライバ | west.yml | スニペット | 実機 |
|---|---|---|---|---|---|
| トラックパッド（円形 30/40mm・ミニ） | — | `zmk-driver-iqs7211e` | ✅ | ✅ | ❌ |
| 4方向スイッチモジュール | ¥2,300 | `zmk-driver-kscan-4-direction-switch` | ✅ | ✅ 移植済 | ❌ |
| ハイレゾダイヤルモジュール | ¥2,500 | `zmk-driver-hires-dial` | ✅ | ✅ 自前 | ❌ |

### ★純正の拡張ポートには1デバイスしか載らない

トラックボール・トラックパッド・ダイヤル・4方向スイッチの**4つが同じ FFC
ポートを取り合う。片側につき1つだけ。**

| | トラックボール | トラックパッド | ダイヤル | 4方向スイッチ |
|---|---|---|---|---|
| バス | `spi0` | `i2c0` | `i2c0` / `0x75` | GPIO 直 |
| ピン | SCK P0.18 / MOSI・MISO P0.16 | SDA P0.18 / SCL P0.16 | 同左 | **P0.16 / P0.18** |
| 電源 | P0.8 | P0.8 | P0.8 | — |
| 割り込み | P0.19 (irq) + P0.20 (CS) | P0.20 (irq) | P0.20 (motion) | **P0.19 / P0.20** |

2個載せたいなら次のどちらか。

- **左右で分担**（純正。`input-split` で転送）→ build.yaml の C / D / E
- **`ngsyst/bmp_boost_extender_mini`** で追加IOポートを引き出す → 1-c

### 1-a. build.yaml は5ターゲット

いま焼いているのは2つだけ。**右 central（ボール）+ 左 peripheral（キーのみ）。**
それに `settings_reset` と、**まだ実機のない「左にダイヤル＋拡張基板のパッド」**の
2つを足して5つ。後者は CI でビルドが通ることだけを見ている。

```
torabo_tsuki_lp_dial_pad_left_peripheral   左 = キー + ダイヤル + 拡張基板のパッド
torabo_tsuki_lp_dial_pad_right_central     右 = ボール + ダイヤル受け + パッド受け
```

**★この2つは必ず組で焼くこと。** ダイヤルの押し込みでマトリクス変換が
1行伸びるので、片側だけ差し替えると押した位置がずれる。

スニペットは全部残してあるので、他の構成は `snippet` 行の差し替えだけで戻せる。
書き方は `build.yaml` 冒頭のコメントと README「他の構成に戻したいとき」。

**届く前に焼かないこと** — 繋がっていない i2c デバイスをプローブし続ける。

届いたあとの確認事項

1. スクロールの向き。逆なら `snippets/input-ext-trackpad/` の DT に
   `v-invert` / `h-invert` を足す（上流は純正ポート側に `h-invert` を付けている）
2. カーソルとして使いたくなったら `scroller-mode` を外し、
   受け側 `input-ext-split-listener` のリスナーに `zip_xy_transform` を足す
3. パッドの直径（下記 1-e）
4. ダイヤルの `counts-per-revolution`（下記 1-d）
5. **FFC のピン「順」** — ダイヤルは 0.5mm 6ピン、同一電極面のケーブルだと
   対向コネクタで順序が反転する。現物合わせで確認すること

#### ★端の帯が横スクロールになる（既定で有効）

2026-09-20、キーフリでせきごんさん本人に確認。「パッドの下のほうだけ
左右スクロールにしている。ファームでいじれる」とのこと。**ドライバに
その機能があり、しかも既定で入っている。**

`zmk-driver-iqs7211e/Kconfig`:

```
CONFIG_IQS7211E_SCROLLER_HWHEEL_ZONE_MIN_PERMILLE   既定 0
CONFIG_IQS7211E_SCROLLER_HWHEEL_ZONE_MAX_PERMILLE   既定 150
```

**Y方向の 0〜15% の帯にいる指は `INPUT_REL_HWHEEL` として報告される。**
1本指でも2本指でも同じ判定（`iqs7211e.c:743` と `:789`）。ただし
**`scroller-mode` のときだけ**なので、`input-trackpad-mini` では効くが
`input-trackpad`（カーソルモード）では関係ない。

| やりたいこと | 設定 |
|---|---|
| 帯を広く / 狭く | `MAX` を変える（150 = 15%） |
| 帯が反対端に出た | `MIN=850` / `MAX=1000` |
| 無効化 | `MIN` と `MAX` を同じ値に |
| 端のタップ誤爆を調整 | `CONFIG_IQS7211E_TAP_EDGE_MARGIN_PERMILLE`（既定 80 = 各辺8%） |

**これは Kconfig であって devicetree ではない。** ビルド時固定で、ZMK Studio
からは変えられない。左右で別の値にもできない。調整のたびに焼き直しになる。
Y=0 がパッドのどちら側かは実装からは決まらないので、現物で見ること。

### 1-b. 4方向スイッチは左手側専用

`snippets/kscan-4-direction-switch/` は**右手側でビルドすると `#error` で止まる。**
オフセットが二重に掛かるため。

- kscan-composite は子 kscan の報告に `column_offset` を足す（`kscan_composite.c:113`）
- そのあと matrix-transform が自分の `col-offset` を足す（`matrix_transform.c:78`）
- 右手側の変換は `col-offset = <7>` を持つので、composite 側でも足すと 14..20 で範囲外

右手側に付けたくなったら、変換の `map` を `RC(5,0..4)` から `RC(5,7..11)` へ動かす。

**上流の `feat/input-hires-dial` は同じ穴に落ちている**（右で `MATRIX_COL_OFFSET = 7`
を composite に渡しつつ変換の `col-offset = 7` も残している）。
コミットメッセージが「動作未確認」なのと整合する。移植時に直してある。

キーマップ側は13レイヤーすべてに `#if defined(TORABO_TSUKI_LP_KSCAN_4_DIRECTION_SWITCH)`
で5個ずつ足してある（フラグ無し 66 / 有り 71、全レイヤー検証済み）。
ベースレイヤーはスクロール上下左右＋ミドルクリック、他は `&trans`。

### 1-c. 追加IOポート — LED Extender で埋まった

**2026-09-20、キーフリで BMP Boost LED Extender を入手（無料配布）。**
Extender Mini を買う必要はなくなった。どちらも同じ追加IOポートに挿さり、
FFC へ出る4本も同一。

#### ★いまの方針: 左へ移す。LED は使わない

最初は右手に付けて3色とも点灯させるところまで確認できたが、**電池カバーに
隠れて見えない**。カバーに穴を開けて導光材を挿す案（下記）も、ケーブルを
引き回せばどのみち隠れるので見送った。

- `build.yaml` から `led-plus` を外した。スニペット自体は残してあるので、
  戻すなら central 側の `snippet` 行に足すだけ
- **エクステンダーは左手へ移す。** 左の純正ポートにハイレゾダイヤル、
  エクステンダーの FFC にミニトラックパッドを載せる。
  右は純正ポートにトラックボールのまま
- P0.24（LED の共通アノード ＝ 拡張基板の電源）は `input-ext-trackpad` 側の
  `power-gpios` が High にするので、`led-plus` を外しても電源は落ちない。
  **`led-plus` だけを外して拡張基板に何も挿さない構成にすると電源が来なくなる**
  — `led-plus` の `gpio-hog` が唯一の駆動源だったため

#### 追加IOはメイン列に1.27mm ずらして挟まっている

端に生えているのではなく、**Pro Micro と同じ2列の中に半ピッチで割り込んでいる。**
BMP Boost の README にある「コンスルーのピンを差し替えて一部をハーフピッチにして
拡張可能」はこれのこと。取り付け用コンスルーとは基板の反対面になるので干渉しない。

```
左列 (x = -7.62)              右列 (x = +7.62)
  y=11.43  GND      ●           ─穴なし─
  y=13.97  ─穴なし─              ─穴なし─
  y=16.51  P0.17    ●           P0.31   ●
  y=19.05  P0.21    ●           P0.29   ●
  y=21.59  P0.24    ●           P1.11   ●
  y=24.13  P1.02    ●           P1.10   ●
```

**9穴・2.54mmピッチ・穴径0.85φ。12ピンコンスルーを6+6に割り、3本抜いて使う。**
高さは 2.5mm（torabo 本体のビルドガイドが指定しているものと同じ）。

#### ピンの行き先（`bmp-boost-led-extender.kicad_sch` / `.kicad_pcb` から確認）

| BMP Boost | ネット | 行き先 |
|---|---|---|
| P0.17 | `SCLK` | FFC |
| P0.21 | `SDIO` | FFC |
| P0.24 | `POW` | FFC |
| P0.29 | `MOTION` | FFC |
| P0.31 | `CS` | FFC |
| **P1.02** | `Y` | **黄LED** |
| **P1.10** | `YG` | **緑LED** |
| **P1.11** | `R` | **赤LED** |

**LED は P1 バンク、FFC は P0 バンクで完全に住み分けている。**
`snippets/input-ext-trackpad/` が要求する4本（SDA P0.17 / SCL P0.21 /
RDY P0.31 / 電源 P0.24）は全部 FFC 側に出ており、**LED とは衝突しない。**

FFC コネクタは `GT-F0501SR10-06S1101`（0.5mm・6ピン）。ネット名は PMW3610 を
想定した命名だが、nRF52840 はペリフェラルを自由に割り当てられるので I2C も張れる。
**キーフリで見送った ¥1,000 の PMW3610 基板は、ここに直挿しする相方だった。**

基板は 17.8 × 14.5mm で、BMP Boost の Y=10.5〜25.0 を覆う。**USB-C は範囲外**
なので、避けるべきは nRF モジュールと白いFFCコネクタの約1.5mm だけ。
部品は全部おもて面（F.Cu）にあり、GND ベタもおもて面。**裏面はまっさら**なので
基板間の隙間に入り込む部品は無い。GND ベタが BMP Boost と反対を向くのは
アンテナへの影響を考えた設計と読める。

#### ★LED は P0.24 を High にしないと光らない — 上流のバグ

**2026-09-20、実機で点灯確認。** ただしそこに至るまでに1つ罠があった。

LED101（3in1パッケージ）の**共通アノードは pad 2 = `/POW` = P0.24** に繋がっている。

```
P0.24 ──┬─ LED共通アノード ─┬─ 赤 ─ R101 ─ P1.11
        │                   ├─ 緑 ─ R102 ─ P1.10
        │                   └─ 黄 ─ R103 ─ P1.02
        └─ FFC コネクタ 6番（拡張モジュールの電源）
```

電流は P0.24 から入って GPIO が吸い出す向き。だから各 LED の `GPIO_ACTIVE_LOW`
は正しいが、**P0.24 が浮いていると GPIO が何をしても電流が流れない。**

`snippets/led-plus/led-plus.overlay` に `gpio-hog` を入れて起動時に High で固定した（`9926f85`）。

**上流の `snippets/led-plus` は P0.24 を駆動していない。** あちらで High にしているのは
コメントアウトされた `trackpad_ext` の `power-gpios` だけ。west.yml に
`caksoylar/zmk-rgbled-widget` が無い件と合わせて、**あのスニペットは通しで
試されていないと思われる。**

##### 切り分けに効いたこと

**親指の赤 `status_led` が光るかどうか。** あれとエクステンダーの3つは同じ
`gpio-leds` device の子で、Zephyr のドライバは**子が1つでも GPIO 設定に失敗すると
device 全体を `-ENODEV` で落とす**（`drivers/led/led_gpio.c`）。
親指が光る＝3本の GPIO 設定も成功している＝ファームは信号を出している、と分かり、
ハード側の経路に絞れた。同じ状況になったらまずここを見ること。

#### ★ケースの作り直しで入れるもの

**2026-09-20、実機で確認。ngsyst の警告どおり、純正の電池カバーはコンスルーの
ピンと干渉する**（「わずかに」どころではなく、ピンが刺さる）。
高さ 2.5mm のコンスルーを使用。2mm にすればエクステンダーが 0.5mm 下がるので
突き出しもその分減るが、ピンの長さ自体は変わらないので根本解決ではない。
**ピンは切らないこと**（バネ機構が入っている）。

トラックパッド用にどのみちケースを作り直すので、**次の版でまとめて入れる**。

##### せきごんさんの公開データにエクステンダー対応版は無い

`sekigon-gonnoc/torabo-tsuki-lp` の `3d-models/STL/` にあるのは次のとおりで、
**薄肉版もモジュール搭載版も無い**。

```
controller-cover-aa-Body.stl / -mirror.stl      ← 電池カバー (S/M/L = 単三)
controller-cover-aaa-xs-Body.stl / -mirror.stl  ← XS (単四)
trackball-case-19mm / 25mm
option/                                          ← シンプルケース
```

**`controller-cover-*` がまさに干渉している部品。** そのまま刷っても解決しない。

ただし **FreeCAD のソースが公開されている**ので、STL から起こす必要はない。

```
3d-models/FreeCAD/controller-cover-aa.FCStd
```

これを開いて凹みと穴を足し、STL に書き出す。

##### ★トラックパッド版のカバーが公開されている

`3d-models/STL/` の直下ではなく **`option/mini-trackpad/`** の中にある。

```
STL:      3d-models/STL/option/mini-trackpad/controller-cover-aa-trackpad-Body.stl (+ -mirror)
                                             controller-cover-aaa-trackpad-xs-Body.stl (+ -mirror)
FreeCAD:  3d-models/FreeCAD/option/controller-cover-aa-trackpad.FCStd
          3d-models/FreeCAD/option/controller-cover-aaa-trackpad-xs.FCStd
```

**通常版より肉が薄い。** V1 と V2 のメッシュを比較した結果:

| | 体積 V1 | 体積 V2 | 差 |
|---|---|---|---|
| トラックパッド版 | 8156.59 mm³ | 7776.84 mm³ | **−379.75 (−4.66%)** |
| 通常版 | 9721.94 mm³ | 9583.93 mm³ | −138.02 (−1.42%) |

**体積が減って表面積が増えている**（+0.58% / +0.27%）ので、肉を削って凹みを
作った形。外形の bbox は V1/V2 とも完全に同一
（X −29.55..9.00 / Y −65.70..1.30 / Z −5.10..13.10）。

**どこを削ったかはメッシュからは特定できなかった。** 再エクスポートで全体の
分割が変わっており、三角形の分布では区別がつかない。
**コンスルーの逃げなのか、トラックパッドの FFC の取り回しなのかは未確認。**
FreeCAD のソースがあるので、開けば寸法を直接読める。

##### ★V1 / V2 で形状が変わっている。使うデータを間違えないこと

ビルドガイド:

> 電池カバーは…**V2ではV1から形状が若干変わっています。** 過去にV1を組み立て済みで、
> 今回V2を組み立てる方は、改めてデータをダウンロードしてください。

`0155059「V2用データを公開」`（2026-06-15）でカバー類が差し替わっている。

**★手持ちは V1。** したがって `master` ではなく **`d512de6`（2026-03-22）から落とすこと。**

ハッシュで境目を確定させた（`controller-cover-aa-trackpad-Body.stl`）:

| ref | 日付 | サイズ | sha256 先頭 | |
|---|---|---|---|---|
| `d512de6` | 2026-03-22 | 695,184 | `03ca1d0e5d16…` | **V1** |
| `32060c2` | 2026-06-07 | 695,184 | `03ca1d0e5d16…` | **V1**（d512de6 と同一） |
| `0155059` | 2026-06-15 | 694,784 | `939cad486735…` | V2 |
| `master` | — | 694,784 | `939cad486735…` | V2 |

`32060c2`「v2の情報を追加」はこのパスの別ファイルを触っただけで STL は変えていない。
**V1 として使えるのは `d512de6` または `32060c2` のどちらでもよい。**

S サイズ = 単三なので `aa`。左右で別部品なので `-Body` と `-Body-mirror` の両方が要る。

```
https://raw.githubusercontent.com/sekigon-gonnoc/torabo-tsuki-lp/d512de6/3d-models/STL/option/mini-trackpad/controller-cover-aa-trackpad-Body.stl
https://raw.githubusercontent.com/sekigon-gonnoc/torabo-tsuki-lp/d512de6/3d-models/STL/option/mini-trackpad/controller-cover-aa-trackpad-Body-mirror.stl
```

`mini-trackpad` 配下の4ファイルすべてが V1/V2 で異なり、差はいずれも 400 バイト
（= 8 三角形）で揃っている。同じ修正が4バリアント全部に入れられている。

FreeCAD 版も同じ ref から取れる:
`3d-models/FreeCAD/option/controller-cover-aa-trackpad.FCStd`

| 入れるもの | 寸法 |
|---|---|
| **エクステンダーの逃げ** | **17.8 × 14.5mm** の凹み。深さはコンスルーのピンの突き出し + 基板厚を実測して決める |
| ~~LED の光窓~~ | **不要になった**（LED は使わない）。下の実測値は戻したくなったとき用に残す |
| トラックパッドの開口 | 現物が来てから |

LED の位置（`bmp-boost-led-extender.kicad_pcb` から実測）:

- 基板は 17.8 × 14.5mm、LED101 は **幅方向の中央**（左右の端から 8.9mm）
- 長さ方向は **USB-C と遠いほうの端から 1.1mm**
- FFC コネクタはその 3.1mm 内側

**3色とも 1.6 × 1.6mm の1パッケージ**なので、穴は1つで足りる。
透明フィラメント / アクリル丸棒を差せば導光材になる。カバー全体を
半透明で刷ってしまえば穴も導光材も要らない（ぼんやり光るだけで良ければ）。

#### 残っている未確認

1. **エクステンダーが純正FFCポート（白いコネクタ）を塞がないか。**
   左手ではダイヤルが純正ポートに刺さる。被るならどちらかを諦めることになる
2. nRF52840 は **TWIM1 と SPIM1 が同じペリフェラル**。ext-trackball 版と併用不可
3. トラックパッドの向き（`v-invert` / `h-invert`）は現物が無いので未調整。
   上流は純正ポート側に `h-invert` を付けている

#### `snippets/input-ext-trackpad/` は左手 peripheral 用に書き直した

- `init-symbol = "mini_trackpad_iqs7211e_init"` / `init-length = <217>` を追加
  （書かないとドライバ既定 = 30/40mm 円形モジュール用の blob になる）
- ローカルのリスナーを切り離した。peripheral 側なので入力は
  `zmk,input-split` で central へ転送する

| スニペット | 役割 |
|---|---|
| `input-ext-trackpad` | `i2c1` + `trackpad_ext@56` のデバイス定義だけ |
| `input-ext-split` | 送り側（peripheral）。`reg = <1>` |
| `input-ext-split-listener` | 受け側（central）。`reg = <1>` + リスナー |
| `input-ext-trackpad-listener` | central に直付けするとき用のローカルリスナー |

`reg` を 1 にしてあるのは、純正ポート側を転送する `input-split` が 0 を
使っているから。central 側は `reg` を値で照合するだけなので連番である必要はない
（`app/src/pointing/input_split.c`）。

`h-invert` はまだ付けていない。現物で向きを見てから。

### 1-d. ダイヤル — ファームは書いた。現物待ち

`zmk-driver-hires-dial` を `353a2196` で固定して `config/west.yml` に追加し、
`snippets/input-hires-dial{,-central}/` を書いた。**実機はまだ無い。**

| | 左（モジュール側 / peripheral） | 右（central） |
|---|---|---|
| スニペット | `input-hires-dial` | `input-hires-dial-central` |
| DT | `sekigon,hires-dial@0x75` on `i2c0` | ダミー `sekigon,remote-hires-dial` |
| Kconfig | `ZMK_HIRES_DIAL` のみ | ＋ `_SCROLL` / `_RADIAL_CONTROLLER` |
| 押し込み | `kscan-gpio-direct` P0.19 → RC(5,0) | kscan は触らず CPP フラグだけ |

#### 値が離れると壊れるところ

- `counts-per-revolution` は**両側で同じ値**にすること。behavior が
  コンパイル時に `DT_PROP(DT_INST_PHANDLE(n, sensor), counts_per_revolution)`
  で読むので、ずれると換算が狂う。いまは上流に合わせて **1275**。
  モジュールの README は「1回転で約1500カウント」なので、実機で
  1回転がずれたらここを上げる
- `triggers-per-rotation` も両側で揃える（いま 20）

#### 上流と違えたところ

上流 `feat/input-hires-dial`（`46ea7ca`、「動作未確認」、CI でもビルドしていない）
とは2点違う。

1. **押し込みの位置。** 上流は `RC(5,5)` に置き、そのために
   `TORABO_TSUKI_LP_MATRIX_COL_OFFSET` を新設して composite と変換の
   col-offset を付け替えている（1-b の二重オフセット問題への対処）。
   こちらは既存の4方向スイッチと同じ流儀のまま **`RC(5,0)` / 左手側専用**に
   した。上流を取り込むときはここが衝突する
2. **スクロールを拾う `zmk,input-listener`。** 上流は `input-hires-dial` に
   だけ置いていて、split 用の `split-input-hires-dial` に入れていない。
   `hires_dial_scroll` の behavior は計算したホイール値を自分自身を
   入力デバイスとして `input_report_rel` するだけなので、**リスナーが
   無いと回しても1ミリも動かない。** behavior の実体は central 側にしか
   無いので、リスナーも central 側（`input-hires-dial-central`）に置いた

#### 実機で確認すること

1. 回転方向（逆なら DT に `invert`）
2. `counts-per-revolution`（1回転でちょうど1周するか）
3. `sleep1-enable` / `sleep2-enable` で回し始めを取りこぼさないか
4. レイヤー割り当て（`config/keymap.keymap`）— **moNa2 のロータリー
   エンコーダに合わせてある。** 対応はレイヤー「名」で取った
   （moNa2 とは `bluetooth` と `gest_arrow` の並び順が入れ替わっているので
   番号では合わない）

   | レイヤー | moNa2 のエンコーダ | torabo のダイヤル |
   |---|---|---|
   | 0 Base | `scroll_up_down`（CW=下） | `&hires_dial_scroll 1 1` |
   | 1 iPad | なし → 0 に落ちる | なし |
   | 2 NUM/SYM1 | `scroll_right_left`（CW=左） | `&hires_dial_hscroll 1 1` |
   | 3 SYM2 | `scroll_up_down` | `&hires_dial_scroll 1 1` |
   | 4 MOUSE | `rsr_vol`（CW=音量down） | `&hires_dial_encoder C_VOL_DN C_VOL_UP` |
   | **5 NUM-SCROLL** | ~~`scroll_right_left`~~ | **`&hires_dial_radial_controller 4 1`** |
   | 6 bluetooth | `scroll_up_down` | `&hires_dial_scroll 1 1` |
   | 7 gest_arrow | `scroll_right_left` | `&hires_dial_hscroll 1 1` |
   | 8〜12 gest_* | なし | なし |

   **レイヤー5 だけ意図的に moNa2 と変えている**（Surface Dial を使いたいため）。
   moNa2 に完全に揃えたくなったら `&hires_dial_hscroll 1 1` に戻し、
   あわせて `CONFIG_ZMK_HIRES_DIAL_RADIAL_CONTROLLER` を `n` にすること
   （使わないのに有効だと USB の HID が1本増えるだけ損）。

   moNa2 との実装の違い: moNa2 は `&msc SCRL_*` をクリック単位で飛ばすが、
   こちらは `hires_dial_scroll` の高分解能スクロールなので滑らかに出る。
   音量だけはエンコーダ扱いで、`triggers-per-rotation`（=20）ごとに1回。

   押し込みは 0 でミドルクリック、5 で Surface Dial のボタン。

   **★回転方向は実機で要確認。** 逆なら keymap ではなく
   `snippets/input-hires-dial/` の DT に `invert` を足す（縦横まとめて反転する。
   moNa2 は CW で「下」と「左」の両方なので符号の向きは揃っている）

#### Radial Controller は fork を引いて動かしている

`CONFIG_ZMK_HIRES_DIAL_RADIAL_CONTROLLER=y` は**上流のドライバのままでは通らない。**

```
zmk-driver-hires-dial/src/radial_controller/endpoints.c:12:45: error: invalid initializer
```

ドライバは

```c
struct zmk_endpoint_instance endpoint = zmk_endpoints_selected();
```

を呼ぶが、この関数は本家 ZMK の **`#3140`**（Joel Spadin, 2026-02-12,
"feat(endpoints): add \"no endpoint\" value"）で
`zmk_endpoint_get_selected()` に改名されている。
宣言が無いので暗黙宣言 = `int` 扱いになり、構造体の初期化子として弾かれる。

**ドライバのバグではない。** 上流 torabo は `west.yml` で ZMK を `v0.3` に
固定しており、あちらでは古い名前が正しくビルドも通る。こちらが v0.4 系
（`#3140` 以降）へ移ったことによるずれ。

**対応: fork を引いている。**

| | |
|---|---|
| fork | [`Daytona0306/zmk-driver-hires-dial`](https://github.com/Daytona0306/zmk-driver-hires-dial) |
| ブランチ | `zmk-main-endpoints`（`main` は上流のミラーのまま） |
| コミット | `2204195` = 上流 `353a2196` + 1コミット |
| 差分 | 1行だけ。実体は `tools/hires-dial-zmk-main.patch` |

```diff
- struct zmk_endpoint_instance endpoint = zmk_endpoints_selected();
+ struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
```

`switch` には `default:` があるので、`#3140` が足した `ZMK_TRANSPORT_NONE`
は扱わなくても問題ない。

**本家への PR は出さない。** せきごんさんは v0.3 固定なので、単純に
書き換える PR は**向こうのビルドを壊す**。`#if` の両対応にするしかなく、
しかも向こうが v0.4 へ移れば不要になる分岐を増やすだけになる。

**★上流が v0.4 へ移ったら fork を捨てること。**
`config/west.yml` の `remote:` を `sekigon-gonnoc` に戻して
revision を上流の SHA にするだけ。

#### 有効にしたことの副作用 — 接続がおかしくなったらここ

- USB: HID インタフェースが1本増える（`USB_HID_DEVICE_COUNT` が 2 になる）
- BLE: レポートマップが変わるため、
  **ペアリング済みホストは再ペアリングが要る場合がある**とドライバ README にある

ダイヤルを載せたファームに焼き替えたあと、既存のペアリングで挙動が
怪しくなったら、まずこれを疑って **`settings_reset` を焼いてペアリングし直す。**

---

## 2. RAM が足りなくなったらここ — せきごんさんのメモリ削減ブランチ

**いま入れる必要はない。** 不具合が出ていないなら空いた RAM は何もしないため。
「機能を足したいのに RAM で詰まった」と分かった時点で検討する。

### どこにあるか

`sekigon-gonnoc/<module>` の **`feat/firmware-memory-reduction`** ブランチ。
**8モジュールに揃っていて、全部うちの固定 SHA の真上に載っている**（2026-09 時点）。

| モジュール | 先 |
|---|---|
| `zmk-feature-custom-settings` | +4 |
| `zmk-feature-runtime-macro` / `-runtime-combo` / `-default-layer` | +3 |
| `zmk-module-runtime-input-processor` / `zmk-feature-os-detection` / `-watchdog` | +2 |
| `zmk-feature-device-info` | +1 |

`zmk-feature-default-layer` はうちが cormoran の `codex/custom-rpc-rewrite` に固定している
変わり種だが、そこにも載っている。**せきごんさんは我々とほぼ同じモジュール構成で作業している。**

cormoran 側の `main` は未マージ（`c6a7fef` のまま）。

### 何をしているか — 3つとも RAM 削減

| 追加されるもの | 効果 |
|---|---|
| `ZMK_CUSTOM_SETTING_DEFINE_FIXED_SIZE` | 裏のバッファを `VALUE_MAX_SIZE` の最悪値でなく実サイズに切り詰める |
| `ZMK_CUSTOM_SETTING_ARRAY_DEFINE_COMPACT` | 配列をスロット丸ごとでなく「実データ + ビットセット」で持つ |
| `ZMK_CUSTOM_SETTINGS_STUDIO_HELPERS` | **カスタム Studio サブシステムが応答バッファを1個で共有する**（`default y if ZMK_STUDIO`） |

3つ目が効くはず。**うちはカスタム RPC サブシステムを9個積んでいて、いまは各自が自前のバッファを持っている。**

**互換性はヘッダで明記されている**（実地では未確認）。

> Its API and persisted representation match `ZMK_CUSTOM_SETTING_DEFINE`; only the private backing buffer is right-sized.
> The wire protocol, settings keys/values, public descriptor and all array APIs remain identical … and both forms may coexist.

**安全側の設計**でもある。応答型が共有バッファ（既定 264 バイト）に収まらなければ
`BUILD_ASSERT` でビルドが落ちる。黙って壊れない。

### 副作用として把握しておくこと

`runtime-macro` の proto が変わっている。

```
- cormoran.runtime_macro.ListMacrosResponse.macros max_count:16
+ cormoran.runtime_macro.ListMacrosResponse.macros type:FT_CALLBACK
```

固定16個の配列をやめてストリーミングエンコードにした（応答構造体を共有バッファに収めるため）。
挙動は同じはずだが、**Studio のマクロ一覧の取得経路そのものが別物になっている。**

### いつ思い出すか

**RAM 不足は「落ちる」形で出る。** 次のどれかが起きたら第一候補。

* Studio の接続が不安定になる
* 機能を足したらスタックオーバーフローや予期しないリセット
* また `*_STACK_SIZE` を上げる羽目になる

**うちは既に3箇所上げている**（余裕が無かった痕跡）。

```conf
CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE=4096
CONFIG_ZMK_STUDIO_RPC_THREAD_STACK_SIZE=6000
CONFIG_ZMK_LOW_PRIORITY_THREAD_STACK_SIZE=4096
```

### 試しかた

**`update-modules` ブランチで 8モジュールをこのブランチに向けてビルドする。**
`main` には触らない。削減量はビルドログの
`Memory region / Used Size / Region Size / %age Used` で数字が出る。

急がなくてよい理由もある。**固定してあるので上流が動いても壊れない。**
cormoran の `main` にマージされれば `update-modules` を回したときに自然に入ってくるし、
そのころには他の人が踏んだバグも潰れている。

### 参考: bmp_boost の flash の割り当て

`boards/arm/bmp_boost/bmp_boost.dts` より。**XIAO も bmp_boost も同じ nRF52840 で RAM は 256 KB。**
この削減が RAM の話である以上、効くかどうかは基板ではなく積んでいる機能の数で決まる。
ただし flash の割り当てはうちのほうが狭い。

| 領域 | 範囲 | 大きさ |
|---|---|---|
| softdevice 予約 | `0x000000-0x026000` | **152 KB**（BLE Micro Pro 由来。ZMK は使わないのに確保されている） |
| **code_partition** | `0x026000-0x0d8000` | **712 KB** ← ファーム本体 |
| storage | `0x0d8000-0x0e0000` | **32 KB** ← DYA の設定・マクロ・コンボ・ジェスチャー・BLE ボンドが全部ここ |
| bootloader | `0x0e0000-0x100000` | 128 KB |

flash が溢れていればリンカが `region FLASH overflowed` で落ちるので、
**ビルドが通っている間は入りきっている。**

---

## 参考: ドライバではないが気になるもの

**`cormoran/zmk-feature-fast-keymap`**（せきごんさんのフォークあり）

Studio の読み込みを**遅い BLE 上で速くする**読み取り専用モジュール。
キーマップは書き換えない。CRC32 のフィンガープリントでキャッシュし、
標準ビヘイビアのメタデータは Web 側に同梱、キーマップは「編集した分だけ」転送する。

DYA Studio の BLE 接続が重いと感じたときの候補。Web UI 側の対応が要るはずなので、
DYA Studio が対応しているかの確認から。

**`zmk-driver-iqs9151`** はせきごんさんのリポジトリにあるが**フォーク**。
本体は ShiniNet さんの **LaLaPad Gen2** 用トラックパッドドライバで、torabo とは無関係。
（ピンチイン/アウト・3本指ジェスチャー・慣性カーソルなど機能は上。IQS7211E の後継 IC）
