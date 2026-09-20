# 残件

torabo-tsuki 側の未着手・未確認事項。片付いたら消すこと。

**畳んだもの**（調査結果は README に移してあります）

| 元項目 | 顛末 |
|---|---|
| Ctrl+Alt+Tab コンボ | 動いているので調査打ち切り。DYA 側の `Timeout ms` は既定のままだと確認でき、残っていた仮説も消えた。原因は未解明のまま |
| DYA Studio の USB 接続不良 | 切り分け打ち切り。あわせて `CONFIG_ZMK_OS_DETECTION_USB` を `=y` に戻した（このシンボルは原因ではない） |
| 慣性スクロール | 現状の値で確定。調査結果は README「スクロールの調整はどこで効くか」へ |
| スクロール倍率の控え | 値は記録しない。「Studio で変えた値はリポジトリに残らない」ことだけ README に記載 |
| west.yml のリビジョン固定 | 29 プロジェクトすべて SHA 固定。`update-modules` ブランチと `tools/west-pins.py` を用意。運用は README「ブランチ運用」へ |

---

## 1. 拡張モジュール — 準備は済み、残るは現物と実機検証

torabo-tsuki LP の拡張モジュールは3種類。のぎけす屋 (BOOTH) で単体販売。

| パーツ | 価格 | ドライバ | west.yml | スニペット | 実機 |
|---|---|---|---|---|---|
| トラックパッド（円形 30/40mm・ミニ） | — | `zmk-driver-iqs7211e` | ✅ | ✅ | ❌ |
| 4方向スイッチモジュール | ¥2,300 | `zmk-driver-kscan-4-direction-switch` | ✅ | ✅ 移植済 | ❌ |
| ハイレゾダイヤルモジュール | ¥2,500 | `zmk-driver-hires-dial` | ❌ 未追加 | ❌ | ❌ |

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

### 1-a. build.yaml は3ターゲットだけ

実際に使う構成しか作らない。**右 central（ボール）+ 左 peripheral（キーのみ）+ settings_reset。**

スニペットは全部残してあるので、他の構成は `snippet` 行の差し替えだけで戻せる。
書き方は `build.yaml` 冒頭のコメントと README「他の構成に戻したいとき」。

**★トラックパッドが届いたら左右そろえて差し替えること。**

```yaml
left:  "studio-rpc-usb-uart input-trackpad-mini input-split"
right: "studio-rpc-usb-uart split-central input-trackball input-listener
        input-split-listener input-scroll-inertia"
```

左だけ変えても動かない（右が `input-split-listener` を持たないと受け取れない）。
**届く前に焼かないこと** — 繋がっていない i2c デバイスをプローブし続ける。

届いたあとの確認事項

1. スクロールの向き。逆なら `snippets/input-trackpad-mini/` の DT に
   `v-invert` / `h-invert` を足す（上流は `h-invert` を付けている）
2. カーソルとして使いたくなったら左を `input-trackpad` に差し替える
3. パッドの直径（下記 1-e）

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

**2026-09-20、キーフリで BMP Boost LED Extender を入手（無料配布）。右手に装着。**
Extender Mini を買う必要はなくなった。どちらも同じ追加IOポートに挿さり、
FFC へ出る4本も同一。

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

#### 残っている未確認

1. **電池カバーがコンスルーに干渉するか。** ngsyst の README は Extender Mini に
   ついて「純正の電池カバーを使うとコンスルーとわずかに干渉する」と書いている。
   取り付け方が同じなので LED Extender にも当てはまるはず。**実機で確認中**
2. **エクステンダーが純正FFCポート（白いコネクタ）を塞がないか。** 右手には
   トラックボールが刺さっている。被るなら左手に移すことになる
3. nRF52840 は **TWIM1 と SPIM1 が同じペリフェラル**。ext-trackball 版と併用不可
4. トラックパッドの向き（`v-invert` / `h-invert`）は現物が無いので未調整。
   上流は `h-invert` を付けている

#### `snippets/input-ext-trackpad/` に足りていないもの

上流 `snippets/led-plus/led-plus.overlay` のコメントアウト部分と突き合わせた結果、
ピン・アドレス・`scroller-mode`・ノード名まで一致していたが、3行足りない。
**ミニパッドを買ったら足すこと。**

```dts
init-symbol = "mini_trackpad_iqs7211e_init";
init-length = <217>;
h-invert;
```

（`init-symbol` を書かないとドライバ既定 = 30/40mm 円形モジュール用の blob になる）

### 1-d. `zmk-driver-hires-dial` は未追加のまま

`upstream/feat/input-hires-dial`（`46ea7ca`, 2026-09-01、「動作未確認」）。
`CONFIG_ZMK_HIRES_DIAL_RADIAL_CONTROLLER` で **Windows の Radial Controller**
（Surface Dial 相当）になる。ただし HID ディスクリプタが変わるので
**ペアリング済みホストは再ペアリングが要る場合がある**と README にある。

押しボタン（P0.19）が row 5 / col 5 に生えるので、4方向スイッチと同じ
`#if` 方式に乗る。買ったときに `TORABO_TSUKI_LP_INPUT_HIRES_DIAL` を足す。

### 1-e. パッドの直径が未確認

単体売りの円形パッドは **30mm / 40mm**。純正オプションは「ミニ」で
**専用 init blob**（`mini_trackpad_iqs7211e_init`、217バイト）を使う。
電極配置が別物ということなので小さいはずだが、**直径は repo に書かれていない。
BOOTH の商品ページ要確認。**

Extender 側のリポジトリは 30mm 用ケースの STL を同梱している。

---

## 2. ブランチまわりの手作業 — あと2つ

1. **Default branch を `main` にする** — Settings → General。表示だけで動作には影響しない
2. **`update-modules` を作る** — 浮動の manifest を置いて上流追随を試す場所。
   `tools/west-pins.py --unpin` で作る。運用は README「ブランチ運用」

---

## 3. RAM が足りなくなったらここ — せきごんさんのメモリ削減ブランチ

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
